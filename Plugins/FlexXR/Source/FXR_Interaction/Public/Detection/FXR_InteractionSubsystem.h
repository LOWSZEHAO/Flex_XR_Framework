// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractionSubsystem.generated.h"

class UFXR_InteractableBase;

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

	/** Resolve the subsystem from any world context object (may return null). */
	static UFXR_InteractionSubsystem* Get(const UObject* WorldContextObject);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_InteractableBase>> Registered;
};
