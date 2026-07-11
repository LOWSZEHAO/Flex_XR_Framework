// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Detection/FXR_InteractionSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UFXR_InteractionSubsystem::RegisterInteractable(UFXR_InteractableBase* Interactable)
{
	if (Interactable)
	{
		Registered.AddUnique(Interactable);
	}
}

void UFXR_InteractionSubsystem::UnregisterInteractable(UFXR_InteractableBase* Interactable)
{
	Registered.RemoveSingleSwap(Interactable);
}

UFXR_InteractableBase* UFXR_InteractionSubsystem::FindBestCandidate(const FVector& GrabCenter, float GrabRadius) const
{
	UFXR_InteractableBase* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const TObjectPtr<UFXR_InteractableBase>& Interactable : Registered)
	{
		if (!Interactable || !Interactable->IsInteractionEnabled() || Interactable->IsHeld())
		{
			continue;
		}

		const float Reach = Interactable->GetActivationRadius() + GrabRadius;
		const float DistanceSq = FVector::DistSquared(GrabCenter, Interactable->GetInteractionLocation());

		if (DistanceSq <= Reach * Reach && DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Interactable;
		}
	}

	return Best;
}

UFXR_InteractionSubsystem* UFXR_InteractionSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_InteractionSubsystem>();
	}

	return nullptr;
}
