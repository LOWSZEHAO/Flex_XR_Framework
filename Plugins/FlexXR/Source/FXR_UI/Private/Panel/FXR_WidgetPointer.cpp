// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Panel/FXR_WidgetPointer.h"

#include "Driver/FXR_InteractionDriver.h"
#include "InputCoreTypes.h"
#include "Interactor/FXR_Interactor.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Panel/FXR_Panel.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UFXR_WidgetPointer::UFXR_WidgetPointer()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The rig's ray and the fingertip are the source. Left on its default the engine would trace
	// from this component's own transform, which is a second ray aimed at something else.
	InteractionSource = EWidgetInteractionSource::Custom;

	// Unused while the source is Custom, but a details panel switched back to World should find the
	// framework's channel already set rather than the engine's default.
	TraceChannel = FXR_TraceChannel;
}

void UFXR_WidgetPointer::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Created with the pawn and never added at runtime; only their active state changes.
		TArray<UFXR_InteractorComponent*> Found;
		Owner->GetComponents<UFXR_InteractorComponent>(Found);
		CachedInteractors.Reset(Found.Num());
		for (UFXR_InteractorComponent* Interactor : Found)
		{
			CachedInteractors.Add(Interactor);
		}

		CachedDriver = Owner->FindComponentByClass<UFXR_InteractionDriver>();
		if (CachedDriver)
		{
			// The driver casts the rig's one far ray. Read it after it has run, not a frame late.
			AddTickPrerequisiteComponent(CachedDriver);
		}
	}

	// Two pointers, one player: Slate keeps capture and hover per pointer index, so sharing one
	// would make each hand's hover cancel the other's.
	PointerIndex = (Hand == EFXR_HandSide::Left) ? 0 : 1;
}

void UFXR_WidgetPointer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave Slate holding a press. It outlives the level, and the next one starts captured.
	SetPressed(false);

	Super::EndPlay(EndPlayReason);
}

void UFXR_WidgetPointer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_WidgetPointer);

	// The order is the whole trick: the hit has to be in place before the engine routes the move
	// event, and the press has to go after it, or Slate presses what was under the pointer last
	// frame — which, on a panel whose buttons have just changed, is the wrong one.
	UpdateSource();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePress();
}

void UFXR_WidgetPointer::UpdateSource()
{
	UFXR_Panel* Panel = nullptr;
	FHitResult Hit;
	bPokeDriving = false;

	IFXR_Interactor* Interactor = GetInteractor();

	// Yields to interaction exactly as locomotion does (ADR-005): a hand carrying a fire
	// extinguisher is not also a mouse.
	const bool bBusy = CachedDriver && CachedDriver->GetHeldInteractable(Hand) != nullptr;

	if (Interactor && !bBusy)
	{
		// Touch first. A hand close enough to press the glass is not pointing at it.
		bPokeDriving = (PokeRange > 0.f) && ResolvePoke(Interactor, Hit, Panel);
		if (!bPokeDriving)
		{
			ResolveRay(Hit, Panel);
		}
	}

	if (!bPokeDriving)
	{
		bPokeArmed = false;
	}

	// A panel mid-fade refuses the pointer, so hover leaves with it rather than sitting on a
	// surface the player can no longer see.
	if (Panel && !Panel->IsPointable())
	{
		Panel = nullptr;
	}

	HoveredPanel = Panel;

	// An empty result is not a blocking hit, which is how the engine is told there is nothing under
	// this hand — and what makes it drop the hover it was holding.
	SetCustomHitResult(Panel ? Hit : FHitResult());
}

void UFXR_WidgetPointer::UpdatePress()
{
	if (!HoveredPanel.IsValid())
	{
		SetPressed(false);
		return;
	}

	if (bPokeDriving)
	{
		// Armed in front of the glass, pressed once the finger is through it.
		SetPressed(bPokeArmed && PokeDepth <= 0.f);
		return;
	}

	const IFXR_Interactor* Interactor = GetInteractor();
	const float Select = Interactor ? Interactor->GetSelectValue() : 0.f;
	SetPressed(bPressed ? (Select > ReleaseThreshold) : (Select >= PressThreshold));
}

bool UFXR_WidgetPointer::ResolvePoke(IFXR_Interactor* Interactor, FHitResult& OutHit, UFXR_Panel*& OutPanel)
{
	FVector Tip = FVector::ZeroVector;
	float Radius = 0.f;
	Interactor->GetPokeTip(Tip, Radius);

	UFXR_Panel::GetPanelsInWorld(GetWorld(), PanelScratch);

	UFXR_Panel* Best = nullptr;
	FHitResult BestHit;
	float BestDepth = 0.f;
	float BestDistance = TNumericLimits<float>::Max();

	for (UFXR_Panel* Panel : PanelScratch)
	{
		FHitResult Candidate;
		float Depth = 0.f;
		if (!Panel || !Panel->TracePoke(Tip, PokeRange, Candidate, Depth))
		{
			continue;
		}

		// Nearest surface wins, in front or behind: two panels back to back must not fight over one
		// finger, and the one it is touching is the one it is closest to.
		const float Distance = FMath::Abs(Depth);
		if (Distance < BestDistance)
		{
			Best = Panel;
			BestHit = Candidate;
			BestDepth = Depth;
			BestDistance = Distance;
		}
	}

	// Emptied on the way out: the capacity is worth keeping across frames, the pointers are not.
	PanelScratch.Reset();

	if (!Best)
	{
		return false;
	}

	if (BestDepth > 0.f)
	{
		bPokeArmed = true;
	}

	OutPanel = Best;
	OutHit = BestHit;
	PokeDepth = BestDepth;
	return true;
}

bool UFXR_WidgetPointer::ResolveRay(FHitResult& OutHit, UFXR_Panel*& OutPanel) const
{
	FHitResult Far;
	if (!CachedDriver || !CachedDriver->GetAimHit(Hand, Far))
	{
		return false;
	}

	UFXR_Panel* Panel = Cast<UFXR_Panel>(Far.GetComponent());
	if (!Panel)
	{
		return false;
	}

	OutHit = Far;
	OutPanel = Panel;
	return true;
}

IFXR_Interactor* UFXR_WidgetPointer::GetInteractor() const
{
	for (const TObjectPtr<UFXR_InteractorComponent>& Interactor : CachedInteractors)
	{
		if (Interactor && Interactor->IsInteractorActive() && Interactor->GetHandSide() == Hand)
		{
			return Interactor.Get();
		}
	}
	return nullptr;
}

void UFXR_WidgetPointer::SetPressed(bool bNewPressed)
{
	if (bNewPressed == bPressed)
	{
		return;
	}

	bPressed = bNewPressed;
	if (bPressed)
	{
		PressPointerKey(EKeys::LeftMouseButton);
	}
	else
	{
		ReleasePointerKey(EKeys::LeftMouseButton);
	}
}
