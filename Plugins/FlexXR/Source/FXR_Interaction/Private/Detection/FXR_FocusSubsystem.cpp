// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Detection/FXR_FocusSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UFXR_FocusSubsystem* UFXR_FocusSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_FocusSubsystem>();
	}
	return nullptr;
}

UFXR_InteractableBase* UFXR_FocusSubsystem::GetHovered(EFXR_HandSide Hand) const
{
	return Hovered[Index(Hand)].Get();
}

UFXR_InteractableBase* UFXR_FocusSubsystem::GetSelected(EFXR_HandSide Hand) const
{
	return Selected[Index(Hand)].Get();
}

EFXR_FocusState UFXR_FocusSubsystem::GetState(const UFXR_InteractableBase* Interactable) const
{
	if (!Interactable)
	{
		return EFXR_FocusState::None;
	}

	// Strongest wins, and either hand counts: a mug held in one hand and reached for by the other
	// reads as Selected, not as two competing states.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Selected[Side].Get() == Interactable)
		{
			return EFXR_FocusState::Selected;
		}
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (Hovered[Side].Get() == Interactable)
		{
			return EFXR_FocusState::Hovered;
		}
	}
	return EFXR_FocusState::None;
}

bool UFXR_FocusSubsystem::IsHoveredByAnyHand(const UFXR_InteractableBase* Interactable) const
{
	return Interactable && (Hovered[0].Get() == Interactable || Hovered[1].Get() == Interactable);
}

void UFXR_FocusSubsystem::SetHovered(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable)
{
	const int32 Side = Index(Hand);
	UFXR_InteractableBase* Previous = Hovered[Side].Get();
	if (Previous == Interactable)
	{
		return;
	}

	Hovered[Side] = Interactable;

	// Both ends are recomputed: the one we left may still be hovered by the other hand, and the one
	// we arrived at may already be Selected. Only the resulting strongest state is broadcast.
	NotifyIfStateChanged(Previous, Hand);
	NotifyIfStateChanged(Interactable, Hand);
}

void UFXR_FocusSubsystem::SetSelected(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable)
{
	const int32 Side = Index(Hand);
	UFXR_InteractableBase* Previous = Selected[Side].Get();
	if (Previous == Interactable)
	{
		return;
	}

	Selected[Side] = Interactable;

	NotifyIfStateChanged(Previous, Hand);
	NotifyIfStateChanged(Interactable, Hand);
}

void UFXR_FocusSubsystem::NotifyIfStateChanged(UFXR_InteractableBase* Interactable, EFXR_HandSide Hand)
{
	if (!Interactable)
	{
		return;
	}

	// Broadcast unconditionally for the affected interactable rather than caching a per-object
	// state: the set is at most two hovered plus two selected, so there is nothing to cache, and a
	// missed closing event is the failure mode that strands a highlight.
	OnFocusChanged.Broadcast(Interactable, GetState(Interactable), Hand);
}
