// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_RayTarget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_RayTargetEvent, EFXR_HandSide, Hand);

/**
 * UFXR_RayTarget — "you can point at me from far away".
 *
 * Not a laser-grab component. The laser lives on the interactor as an FXR_Core service; this only
 * marks an object as answerable by it, and behaviour composes with whatever else is on the object:
 * alone it is select/focus ("point to the correct extinguisher"), and alongside FXR_Grab it becomes
 * the distance-grab target.
 *
 * There is deliberately no ray-driven Latch: dragging a valve open by laser feels cheap and teaches
 * a trainee nothing. Ray-*selecting* a latch object is fine; ray-*driving* one is a game-side
 * custom interactable.
 *
 * Found by tracing the FXR_Interaction channel rather than by the registry (ADR-002): a far ray has
 * to respect occlusion and hit where the mesh actually is, which is exactly the mesh-accurate case
 * the channel is reserved for.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_RayTarget : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	UFXR_RayTarget();

	/** Rays, not the grab sphere, find this — so it never competes with a hand reaching for something. */
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
