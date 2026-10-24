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

	// Custom source: the hit comes from the rig's ray / fingertip. The default would trace from this
	// component's own transform, which is a different ray.
	InteractionSource = EWidgetInteractionSource::Custom;

	// Not used while the source is Custom, but if someone switches it back to World it should
	// already be on the framework's channel.
	TraceChannel = FXR_TraceChannel;
}

void UFXR_WidgetPointer::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Created with the pawn; only their active state changes at runtime.
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
			// Tick after the driver, which casts the far ray, so the hit isn't a frame old.
			AddTickPrerequisiteComponent(CachedDriver);
		}
	}

	// Separate pointer index per hand. Slate tracks hover and capture per index, so with a shared
	// one each hand would cancel the other's hover.
	PointerIndex = (Hand == EFXR_HandSide::Left) ? 0 : 1;
}

void UFXR_WidgetPointer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Always release the press on the way out, or Slate keeps the capture into the next level.
	SetPressed(false);

	Super::EndPlay(EndPlayReason);
}

void UFXR_WidgetPointer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_WidgetPointer);

	// Order matters: set the hit before the engine routes the move event, then send the press.
	// Otherwise Slate presses whatever was under the pointer last frame.
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

	// A hand that's holding something isn't a pointer, same rule as locomotion (ADR-005).
	const bool bBusy = CachedDriver && CachedDriver->GetHeldInteractable(Hand) != nullptr;

	if (Interactor && !bBusy)
	{
		// Touch wins: a hand close enough to touch the panel isn't pointing at it.
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

	// Panels that are fading don't accept the pointer, so hover goes away with the panel.
	if (Panel && !Panel->IsPointable())
	{
		Panel = nullptr;
	}

	HoveredPanel = Panel;

	// An empty (non-blocking) hit tells the engine there's nothing under this hand, so it drops hover.
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
		// Armed while in front, pressed once the finger goes through.
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

		// Closest surface wins, in front or behind, so two back-to-back panels don't fight over a finger.
		const float Distance = FMath::Abs(Depth);
		if (Distance < BestDistance)
		{
			Best = Panel;
			BestHit = Candidate;
			BestDepth = Depth;
			BestDistance = Distance;
		}
	}

	// Emptied after use; keep the capacity, not the pointers.
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
