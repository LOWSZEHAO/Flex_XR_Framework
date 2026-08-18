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
 *
 * Two-handed hold is a checkbox, not a component (§4): with Allow Two-Handed the second hand
 * joins the hold and aims the object — the first hand keeps position and roll, while the object
 * pivots about it so its *secondary grip point* tracks the second hand (rifle foregrip). The
 * second hand's mesh glues to that grip point. Either hand may leave; the survivor carries on.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Grab : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime) override;
	virtual void OnEnd(EFXR_EndReason Reason) override;

	//~ Two-hand contract.
	virtual bool CanBeginSecondary(IFXR_Interactor* Interactor) const override;
	virtual void OnBeginSecondary(IFXR_Interactor* Interactor) override;
	virtual void ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason) override;

	/** The hand pose the given hand forms (its grip point's pose), or null (procedural hold). */
	virtual UFXR_HandPose* GetActiveHandPose(EFXR_HandSide Side) const override;

	/** The second hand glues to its grip point; the first keeps the controller pose (the object came to it). */
	virtual bool GetHandAttachTransform(EFXR_HandSide Side, FTransform& OutTransform) const override;

	/** True while a second hand is joined to the hold. */
	UFUNCTION(BlueprintPure, Category = "Grab|TwoHand")
	bool IsTwoHanded() const { return SecondaryInteractor != nullptr; }

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

	/**
	 * Let a second hand join the hold to aim the object (rifles, big tools, steering). Off by
	 * default: for an ordinary prop a second hand swinging the aim reads as a glitch, not a feature.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|TwoHand")
	bool bAllowTwoHanded = false;

	/** Use value at or above which OnUseStarted fires (controller trigger / tracked-hand index-squeeze). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseThreshold = 0.5f;

	/** Use value below which OnUseEnded fires — keep under Use Threshold for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseReleaseThreshold = 0.35f;

private:
	/**
	 * Two-hand pose: the one-hand hold rotated about the primary grip so the authored secondary grip
	 * point points at the second hand. Aiming by the authored handle (rather than the object's X
	 * axis) is what makes the rotation read as "held by both grips" instead of arbitrary.
	 */
	FTransform MakeTwoHandTransform() const;
	/** Re-resolve the secondary attach point on the object — a rail slides, a point grip does not. */
	void UpdateSecondaryGripLocal();
	/** Re-anchor the single-hand offset to the primary grip so hand transitions never pop the object. */
	void ReanchorToPrimary();

	FTransform HeldOffset = FTransform::Identity;
	FTransform SnapProceduralOffset = FTransform::Identity;
	FTransform SnapTargetOffset = FTransform::Identity;
	float SnapAlpha = 1.f;
	float SnapInterpSpeed = 10.f;
	bool bRestorePhysics = false;
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
	TWeakObjectPtr<UFXR_HandPose> ActiveHandPose;

	// Two-hand state. Raw interactor pointers are held only while attached — they live on the
	// pawn rig and every release path (hand, force, disable) clears them before the hold ends.
	IFXR_Interactor* PrimaryInteractor = nullptr;
	IFXR_Interactor* SecondaryInteractor = nullptr;
	TWeakObjectPtr<UFXR_HandPose> SecondaryHandPose;
	TWeakObjectPtr<UFXR_GripPoint> SecondaryGripPoint;
	/** Secondary grip point in the driven object's local space, captured when the second hand joins. */
	FVector SecondaryGripLocal = FVector::ZeroVector;
	bool bHasSecondaryGrip = false;

	FVector LastLocation = FVector::ZeroVector;
	FQuat LastRotation = FQuat::Identity;
	FVector TrackedLinearVelocity = FVector::ZeroVector;
	FVector TrackedAngularVelocity = FVector::ZeroVector;

	float CurrentUseValue = 0.f;
	bool bUsing = false;
};
