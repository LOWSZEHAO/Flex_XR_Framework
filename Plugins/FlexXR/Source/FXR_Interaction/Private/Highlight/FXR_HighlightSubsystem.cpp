// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Highlight/FXR_HighlightSubsystem.h"
#include "Highlight/FXR_Highlight.h"
#include "Detection/FXR_FocusSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UFXR_HighlightSubsystem* UFXR_HighlightSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_HighlightSubsystem>();
	}
	return nullptr;
}

void UFXR_HighlightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Bound here rather than in Initialize: subsystem construction order is not guaranteed, and by
	// BeginPlay every world subsystem exists.
	if (UFXR_FocusSubsystem* Focus = InWorld.GetSubsystem<UFXR_FocusSubsystem>())
	{
		Focus->OnFocusChanged.AddDynamic(this, &UFXR_HighlightSubsystem::HandleFocusChanged);
	}
}

void UFXR_HighlightSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (UFXR_FocusSubsystem* Focus = World->GetSubsystem<UFXR_FocusSubsystem>())
		{
			Focus->OnFocusChanged.RemoveDynamic(this, &UFXR_HighlightSubsystem::HandleFocusChanged);
		}
	}

	Super::Deinitialize();
}

void UFXR_HighlightSubsystem::HandleFocusChanged(UFXR_InteractableBase* Interactable, EFXR_FocusState State, EFXR_HandSide Hand)
{
	Refresh(Interactable);
}

void UFXR_HighlightSubsystem::SetGuidance(UFXR_InteractableBase* Interactable, bool bGuided)
{
	if (!Interactable)
	{
		return;
	}

	if (bGuided)
	{
		Guided.Add(Interactable);
	}
	else
	{
		Guided.Remove(Interactable);
	}
	Refresh(Interactable);
}

EFXR_HighlightState UFXR_HighlightSubsystem::GetHighlightState(const UFXR_InteractableBase* Interactable) const
{
	if (!Interactable)
	{
		return EFXR_HighlightState::None;
	}

	const UFXR_FocusSubsystem* Focus = UFXR_FocusSubsystem::Get(this);
	const EFXR_FocusState FocusState = Focus ? Focus->GetState(Interactable) : EFXR_FocusState::None;

	// Acting on it outranks being told to; being told to outranks merely being able to. Guidance
	// therefore survives the player reaching for the object, and stops once they have it.
	if (FocusState == EFXR_FocusState::Selected)
	{
		return EFXR_HighlightState::Selected;
	}
	if (Guided.Contains(Interactable))
	{
		return EFXR_HighlightState::Guidance;
	}
	if (FocusState == EFXR_FocusState::Hovered)
	{
		return EFXR_HighlightState::Hover;
	}
	return EFXR_HighlightState::None;
}

void UFXR_HighlightSubsystem::GatherTargets(const UFXR_InteractableBase* Interactable, TArray<UPrimitiveComponent*>& OutTargets) const
{
	OutTargets.Reset();
	if (!Interactable)
	{
		return;
	}

	const UFXR_Highlight* Config = Interactable->GetOwner()
		? Interactable->GetOwner()->FindComponentByClass<UFXR_Highlight>()
		: nullptr;

	const EFXR_HighlightScope Scope = Config ? Config->GetScope() : EFXR_HighlightScope::Everything;

	if (Scope == EFXR_HighlightScope::TargetMesh)
	{
		if (UPrimitiveComponent* Driven = Interactable->GetDrivenComponent())
		{
			OutTargets.Add(Driven);
		}
		return;
	}

	// Everything: the owning actor's primitives, so an extinguisher glows with its pin, handle and
	// hose as one object. Scoped to this actor — other actors and the hand meshes are not ours.
	if (const AActor* Owner = Interactable->GetOwner())
	{
		Owner->GetComponents<UPrimitiveComponent>(OutTargets);
	}
}

int32 UFXR_HighlightSubsystem::StencilFor(EFXR_HighlightStyle Style)
{
	// One value per style so a single post-process material can branch rather than needing one
	// material per style.
	switch (Style)
	{
	case EFXR_HighlightStyle::Outline:    return 1;
	case EFXR_HighlightStyle::InnerBlink: return 2;
	case EFXR_HighlightStyle::Sweep:      return 3;
	default:                              return 0;
	}
}

void UFXR_HighlightSubsystem::Refresh(UFXR_InteractableBase* Interactable)
{
	if (!Interactable)
	{
		return;
	}

	const EFXR_HighlightState State = GetHighlightState(Interactable);

	const UFXR_Highlight* Config = Interactable->GetOwner()
		? Interactable->GetOwner()->FindComponentByClass<UFXR_Highlight>()
		: nullptr;

	// No component is the common case: the style comes straight from project settings, which is what
	// makes "drop FXR_Grab and it glows" true with zero setup.
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	EFXR_HighlightStyle Style = EFXR_HighlightStyle::None;
	if (State != EFXR_HighlightState::None)
	{
		Style = Config ? Config->ResolveStyle(State)
			: (Settings ? Settings->GetStyleFor(State) : EFXR_HighlightStyle::None);
	}

	const int32 Stencil = StencilFor(Style);

	TArray<UPrimitiveComponent*> Targets;
	GatherTargets(Interactable, Targets);

	for (UPrimitiveComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		Target->SetRenderCustomDepth(Stencil != 0);
		Target->SetCustomDepthStencilValue(Stencil);
	}

	// Track only what is lit, so the "clear everything" path never walks the whole registry.
	if (Stencil != 0)
	{
		Lit.Add(Interactable);
	}
	else
	{
		Lit.Remove(Interactable);
	}
}
