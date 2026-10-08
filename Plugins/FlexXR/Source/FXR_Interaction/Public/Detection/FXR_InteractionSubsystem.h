// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractionSubsystem.generated.h"

class UFXR_InteractableBase;
class UFXR_ScoringPolicy;

/**
 * UFXR_InteractionSubsystem — the registry-based detection service (ADR-002, design 5.4).
 *
 * Every UFXR_InteractableBase auto-registers here on BeginPlay. The broad phase is a
 * registry query against activation radii (never physics overlap events); the narrow
 * phase scores candidates by distance. Deterministic and setup-free for designers.
 *
 * First pass uses a linear scan over a pre-sized array; a spatial hash is the optimisation
 * once candidate counts grow (design 5.4 / Phase 5).
 */
UCLASS()
class FXR_INTERACTION_API UFXR_InteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Register an interactable into the broad-phase registry. */
	void RegisterInteractable(UFXR_InteractableBase* Interactable);

	/** Remove an interactable from the registry. */
	void UnregisterInteractable(UFXR_InteractableBase* Interactable);

	/**
	 * Best available interactable in grab reach of the hand, scored by nearest distance. Reach
	 * is per-interactable (ADR-007): owned grip points if any (filtered by hand side), else the
	 * activation radius. Skips disabled and already-held interactables. Null if none.
	 */
	UFXR_InteractableBase* FindBestCandidate(const FVector& GrabCenter, float GrabRadius, EFXR_HandSide HandSide) const;

	/** All registered interactables — for driver-side passes (pokes). Broad-phase culling is the caller's job until the spatial hash lands. */
	const TArray<TObjectPtr<UFXR_InteractableBase>>& GetRegistered() const { return Registered; }

	/** Resolve the subsystem from any world context object (may return null). */
	static UFXR_InteractionSubsystem* Get(const UObject* WorldContextObject);

private:
	/**
	 * The scoring policy, resolved once from project settings. Held rather than looked up per call:
	 * detection runs for both hands every frame and must not allocate or hit the asset registry there.
	 */
	const UFXR_ScoringPolicy* GetScoringPolicy() const;

	UPROPERTY(Transient)
	mutable TObjectPtr<UFXR_ScoringPolicy> ScoringPolicy;

	mutable bool bScoringPolicyResolved = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_InteractableBase>> Registered;
};
