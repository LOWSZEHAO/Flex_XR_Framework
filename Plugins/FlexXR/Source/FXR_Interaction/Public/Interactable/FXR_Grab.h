// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_Grab.generated.h"

class UPrimitiveComponent;
class UFXR_GripPoint;
class UFXR_HandPose;

/** Broadcast on the use-input edges of a held FXR_Grab (trigger pull / index-squeeze). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_GrabUseDelegate);

/**
 * UFXR_Grab — free 6-DOF grab.
 *
 * Kinematic-while-held (ADR-001): on grab the driven primitive stops simulating and
 * follows the interactor's grip pose; on release it resumes physics. This is the first
 * pass of the solver behaviour (SetWorldTransform for now); a later slice swaps in the
 * kinematic-target constraint solver and release velocity.
 *
 * Built-in use events (§4 FXR_Grab) cover the "hold grip, pull trigger" 80% case: while held,
 * the holding hand's Use value drives OnUseStarted / OnUseEnded and an analog UseValue, so a
 * gun or flashlight needs only this component — no separate FXR_Use.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Grab : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime) override;
	virtual void OnEnd(EFXR_EndReason Reason) override;

	/** The hand pose of the grip point currently in use, or null (procedural hold). */
	UFXR_HandPose* GetActiveHandPose() const;

	/** Analog use value 0..1 from the holding hand (0 when not held) — bind for variable triggers. */
	UFUNCTION(BlueprintPure, Category = "Grab|Use")
	float GetUseValue() const { return CurrentUseValue; }

	/** True while use is held past its threshold (hysteresis-latched). */
	UFUNCTION(BlueprintPure, Category = "Grab|Use")
	bool IsUsing() const { return bUsing; }

	/** Use crossed Use Threshold while held — "pull the trigger" (fire, toggle the light). */
	UPROPERTY(BlueprintAssignable, Category = "Grab|Use")
	FFXR_GrabUseDelegate OnUseStarted;

	/** Use fell below Use Release Threshold, or the object was released while using. */
	UPROPERTY(BlueprintAssignable, Category = "Grab|Use")
	FFXR_GrabUseDelegate OnUseEnded;

protected:
	/** Multiplier on the hand's tracked velocity handed to the object on release — tune throw strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab", meta = (ClampMin = "0.0"))
	float ThrowVelocityScale = 1.f;

	/** Use value at or above which OnUseStarted fires (controller trigger / tracked-hand index-squeeze). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseThreshold = 0.5f;

	/** Use value below which OnUseEnded fires — keep under Use Threshold for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseReleaseThreshold = 0.35f;

private:
	/** Best grip point on the owner for this interactor's hand (highest priority, then nearest), or null. */
	UFXR_GripPoint* SelectGripPoint(IFXR_Interactor* Interactor) const;

	FTransform HeldOffset = FTransform::Identity;
	FTransform SnapProceduralOffset = FTransform::Identity;
	FTransform SnapTargetOffset = FTransform::Identity;
	float SnapAlpha = 1.f;
	float SnapInterpSpeed = 10.f;
	bool bRestorePhysics = false;
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
	TWeakObjectPtr<UFXR_HandPose> ActiveHandPose;

	FVector LastLocation = FVector::ZeroVector;
	FQuat LastRotation = FQuat::Identity;
	FVector TrackedLinearVelocity = FVector::ZeroVector;
	FVector TrackedAngularVelocity = FVector::ZeroVector;

	float CurrentUseValue = 0.f;
	bool bUsing = false;
};
