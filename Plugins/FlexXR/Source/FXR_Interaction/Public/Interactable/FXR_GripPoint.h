// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_GripPoint.generated.h"

/**
 * UFXR_GripPoint — "a sticker on the object: hands go here, shaped like this."
 *
 * A SceneComponent whose transform is the authored grip pose. Add one (or several) to a
 * grabbable actor; FXR_Grab scores them by hand side, priority and distance, then snaps
 * the held object so the chosen grip point aligns to the hand's grip — a consistent,
 * authored hold instead of wherever the hand happened to close.
 *
 * This slice delivers the snap + selection. Finger shaping via UFXR_HandPose retargeting
 * (design 5.3) is a following slice; the point's transform is the "where", the pose is
 * the "shaped like this".
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_GripPoint : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_GripPoint();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True if this grip point accepts the given hand. */
	bool AcceptsHand(EFXR_HandSide Side) const;

	int32 GetPriority() const { return Priority; }
	float GetActivationRadius() const { return ActivationRadius; }
	bool ShouldSnap() const { return bSnapToGrip; }

protected:
	/** Which hand may use this grip point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	EFXR_GripHandedness Handedness = EFXR_GripHandedness::Both;

	/** Higher priority wins when several grip points are in range for the same hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	int32 Priority = 0;

	/** Radius (cm) from the hand's grip within which this point is a candidate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint", meta = (ClampMin = "0.5"))
	float ActivationRadius = 8.f;

	/** Snap the object so this point aligns to the hand's grip pose when grabbed here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	bool bSnapToGrip = true;

	/** Draw this grip point's axes + activation radius at runtime (authoring aid). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint|Debug")
	bool bDrawDebug = false;
};
