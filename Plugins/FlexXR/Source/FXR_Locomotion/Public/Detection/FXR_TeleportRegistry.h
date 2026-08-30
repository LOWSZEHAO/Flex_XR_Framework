// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FXR_TeleportRegistry.generated.h"

class UFXR_TeleportAnchor;
class UFXR_TeleportBlocker;

/**
 * UFXR_TeleportRegistry — world registry of teleport anchors and blockers (ADR-002 pattern).
 *
 * Anchors and blockers self-register on BeginPlay; the locomotion component queries this each aim
 * frame instead of scanning the level. Linear scan for the first pass (matching the interaction
 * subsystem); a spatial partition is the optimisation once counts grow.
 */
UCLASS()
class FXR_LOCOMOTION_API UFXR_TeleportRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterAnchor(UFXR_TeleportAnchor* Anchor);
	void UnregisterAnchor(UFXR_TeleportAnchor* Anchor);
	void RegisterBlocker(UFXR_TeleportBlocker* Blocker);
	void UnregisterBlocker(UFXR_TeleportBlocker* Blocker);

	/** Nearest anchor whose snap radius contains Location, or null. */
	UFXR_TeleportAnchor* FindAnchorNear(const FVector& Location) const;

	/** The first registered blocker containing Location, or null — named so its events can fire. */
	UFXR_TeleportBlocker* FindBlockerAt(const FVector& Location) const;

	/** True if Location is inside any registered blocker. */
	bool IsBlocked(const FVector& Location) const { return FindBlockerAt(Location) != nullptr; }

	/** Resolve the subsystem from any world context object (may return null). */
	static UFXR_TeleportRegistry* Get(const UObject* WorldContextObject);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_TeleportAnchor>> Anchors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_TeleportBlocker>> Blockers;
};
