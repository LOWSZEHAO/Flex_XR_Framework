// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_RayTarget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_RayTargetEvent, EFXR_HandSide, Hand);

/**
 * UFXR_RayTarget - marks an object as something you can point at from a distance.
 *
 * The ray itself belongs to the interactor; this just marks the object as a valid target. On its
 * own it gives select/focus ("point at the right extinguisher"); with FXR_Grab it becomes a
 * distance-grab target.
 *
 * There's no ray-driven Latch on purpose: opening a valve with a laser feels cheap and doesn't
 * teach anything. Selecting a latch by ray is fine; driving one by ray would be a custom
 * interactable in the game.
 *
 * Found by tracing the FXR_Interaction channel instead of through the registry (ADR-002), since
 * a far ray has to respect occlusion and hit the actual mesh.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_RayTarget : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	UFXR_RayTarget();

	/** Rays, not the grab sphere, find this - so it never competes with a hand reaching for something. */
	virtual bool IsGrabTarget() const override { return false; }

	/** Pointed at, never held. */
	virtual bool CanEverBeHeld() const override { return false; }

	/** Furthest a ray may be cast and still land on this target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray Target", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxRayDistance = 1000.f;

	/** A hand's ray landed on this. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Ray Target")
	FFXR_RayTargetEvent OnRayEnter;

	/** That hand's ray left. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Ray Target")
	FFXR_RayTargetEvent OnRayExit;

	/** Select was pressed while this hand's ray was on it. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Ray Target")
	FFXR_RayTargetEvent OnRaySelected;

	//~ Called by UFXR_InteractionDriver, which owns ray aiming for the rig.
	void NotifyRayEnter(EFXR_HandSide Hand);
	void NotifyRayExit(EFXR_HandSide Hand);
	void NotifyRaySelected(EFXR_HandSide Hand);
};
