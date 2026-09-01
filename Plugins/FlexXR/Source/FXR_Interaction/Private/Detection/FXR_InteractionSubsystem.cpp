// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Detection/FXR_InteractionSubsystem.h"
#include "Detection/FXR_ScoringPolicy.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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

const UFXR_ScoringPolicy* UFXR_InteractionSubsystem::GetScoringPolicy() const
{
	if (bScoringPolicyResolved)
	{
		return ScoringPolicy;
	}
	bScoringPolicyResolved = true;

	// Resolved once and cached, the failure included. LoadSynchronous on a soft class pointer can
	// reach the asset registry, and this sits in the per-frame detection path for both hands. Null
	// stays null, and the caller falls back to nearest-wins.
	if (const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get())
	{
		if (UClass* PolicyClass = Settings->ScoringPolicy.LoadSynchronous())
		{
			ScoringPolicy = NewObject<UFXR_ScoringPolicy>(const_cast<UFXR_InteractionSubsystem*>(this), PolicyClass);
		}
	}
	return ScoringPolicy;
}

UFXR_InteractableBase* UFXR_InteractionSubsystem::FindBestCandidate(const FVector& GrabCenter, float GrabRadius, EFXR_HandSide HandSide) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Detection_FindBestCandidate);

	UFXR_InteractableBase* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (const TObjectPtr<UFXR_InteractableBase>& Interactable : Registered)
	{
		if (!Interactable || !Interactable->IsGrabTarget() || !Interactable->IsInteractionEnabled() || Interactable->IsHeld())
		{
			continue;
		}

		// Reach is the interactable's own business (ADR-007): grip points if it owns any, else
		// the activation radius around the driven mesh.
		float ReachDistanceSq = TNumericLimits<float>::Max();
		if (!Interactable->IsInGrabReach(GrabCenter, GrabRadius, HandSide, ReachDistanceSq))
		{
			continue;
		}

		// Reach decides eligibility, scoring decides the winner. Separating them is what lets a
		// project re-rank candidates without also inheriting the responsibility for who is in range.
		const UFXR_ScoringPolicy* Policy = GetScoringPolicy();
		const float Score = Policy
			? Policy->ScoreCandidate(*Interactable, GrabCenter, HandSide, ReachDistanceSq)
			: ReachDistanceSq;
		if (Score < BestScore)
		{
			BestScore = Score;
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
