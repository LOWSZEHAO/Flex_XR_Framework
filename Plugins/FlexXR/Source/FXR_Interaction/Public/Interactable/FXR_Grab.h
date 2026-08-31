// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_Grab.generated.h"

class UPrimitiveComponent;
class USceneComponent;
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
 * Two-handed hold is a checkbox, not a component (§4). With Allow Two-Handed a second hand joins
 * the hold and glues to its grip point; Two Hand Mode then decides whether it steers. Shared
 * solves both hands symmetrically — the object's grip-to-grip axis follows the line between them,
 * so either hand can drive (a broom sweeps from whichever hand moves). Support lets the second
 * hand hold on without reorienting anything. Either hand may leave; the survivor carries on.
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

	/** True while the object is flying to a hand that claimed it from range. */
	UFUNCTION(BlueprintPure, Category = "Grab|Distance")
	bool IsFlyingToHand() const { return bFlying; }

	/** Whether a far ray may claim this object at all. */
	bool AllowsDistanceGrab() const { return bDistanceGrab; }

	/**
	 * Claim from range: the object flies to the hand and then becomes an ordinary hold, so everything
	 * downstream — grip point, pose blend, two-hand, throw — is the same code path as reaching for it.
	 */
	void BeginDistanceGrab(IFXR_Interactor* Interactor);

	/**
	 * Tell this object what its physics state was before something else parked it kinematic — a socket
	 * seating it. Without this a later grab reads the parked body, concludes it never simulated, and
	 * the object can never fall again after release.
	 */
	void NotifyParkedPhysics(bool bWasSimulating);

	/**
	 * Park every simulating body this object's hold would carry, and hand them all back. Public
	 * because a socket parks the same set for a different reason, and it must be the same set —
	 * otherwise a loose part stays simulating and falls off whatever it was docked to.
	 */
	void ParkPhysics();
	void RestorePhysics();

	/** World transform of whatever this hold moves — the actor, or just the driven mesh. */
	FTransform GetHeldTransform() const;

	/** Move it. Used by the hold itself and by anything that places the object, such as a socket. */
	void SetHeldTransform(const FTransform& NewTransform);

	/** Whether this component travels with the hold, which is what a preview has to mirror. */
	bool MovesComponent(const USceneComponent* Component) const;

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
	 * What a hold moves. Whole Actor by default, because picking something up takes all of it: a mesh
	 * that happens to sit beside the grabbed one rather than beneath it is still part of the object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab")
	EFXR_GrabScope GrabScope = EFXR_GrabScope::WholeActor;

	/**
	 * Let this object be pulled to the hand from across the room. Off by default: being able to yank
	 * something you cannot reach changes how it feels, and in a training sim it can quietly delete the
	 * physical performance the exercise is meant to teach — so it is a deliberate tick, per object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Distance")
	bool bDistanceGrab = false;

	/**
	 * How long the flight takes, regardless of how far away the object was. Constant time rather than
	 * constant speed: a fixed duration reads the same whether you summon from one metre or five.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Distance", meta = (ClampMin = "0.05", ClampMax = "2.0", Units = "s", EditCondition = "bDistanceGrab"))
	float DistanceGrabDuration = 0.3f;

	/**
	 * Let a second hand join the hold to aim the object (rifles, big tools, steering). Off by
	 * default: for an ordinary prop a second hand swinging the aim reads as a glitch, not a feature.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|TwoHand")
	bool bAllowTwoHanded = false;

	/** Whether the second hand steers the object, or merely holds on while the first hand poses it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|TwoHand", meta = (EditCondition = "bAllowTwoHanded"))
	EFXR_TwoHandMode TwoHandMode = EFXR_TwoHandMode::Shared;

	/** Use value at or above which OnUseStarted fires (controller trigger / tracked-hand index-squeeze). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseThreshold = 0.5f;

	/** Use value below which OnUseEnded fires — keep under Use Threshold for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|Use", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UseReleaseThreshold = 0.35f;

	/** How fast the object swings onto aim when the second hand joins (higher = snappier; ~10 is roughly 100 ms). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab|TwoHand", meta = (ClampMin = "0.1", EditCondition = "bAllowTwoHanded"))
	float TwoHandAimSpeed = 10.f;

private:
	/**
	 * Two-hand pose, solved symmetrically: the object's grip-to-grip axis is aligned to the line
	 * between the hands, their midpoints are matched, and roll comes from both wrists. Neither hand
	 * is an anchor, so moving either one does the natural thing — a broom sweeps whichever hand
	 * drives it — and no role ever has to switch mid-hold.
	 */
	FTransform MakeTwoHandTransform() const;
	/** Pose the object would be held in, so a distance grab arrives already seated. */
	FTransform ComputeDistanceGrabTarget(IFXR_Interactor* Interactor) const;
	/** Advance the distance-grab flight, handing over to the ordinary hold on arrival. */
	void TickDistanceGrab(IFXR_Interactor* Interactor, float DeltaTime);
	/** Re-resolve where each hand holds the object — a rail slides under the hand, a point grip does not. */
	void UpdateGripLocals();
	/** Re-anchor the single-hand offset to the primary grip so hand transitions never pop the object. */
	void ReanchorToPrimary();

	FTransform HeldOffset = FTransform::Identity;
	FTransform SnapProceduralOffset = FTransform::Identity;
	FTransform SnapTargetOffset = FTransform::Identity;
	float SnapAlpha = 1.f;
	float SnapInterpSpeed = 10.f;
	bool bRestorePhysics = false;

	// Distance-grab flight. Interpolated from elapsed time and the live hand pose — deterministic, so
	// an SOP replay reproduces it, which a physics impulse toward the hand would not (ADR-001).
	bool bFlying = false;
	float FlightElapsed = 0.f;
	FTransform FlightStart = FTransform::Identity;
	// Physics state is captured when the flight starts, so the later OnBegin does not read the
	// already-disabled body and forget to restore simulation on release.
	bool bPhysicsCaptured = false;

	/** A body this hold parked, and where it sits relative to the held frame. */
	struct FParkedBody
	{
		TWeakObjectPtr<UPrimitiveComponent> Body;
		FTransform RelativeToHeld = FTransform::Identity;
	};

	// Bodies this hold switched off, so release restores exactly what it parked and nothing else.
	TArray<FParkedBody> ParkedBodies;

	/**
	 * Place every parked body against the held frame. A component that has simulated stops following
	 * its parent by attachment even once it is kinematic again, so moving the actor alone leaves it
	 * behind — it has to be driven explicitly, which the solver does for everything else anyway.
	 */
	void PlaceParkedBodies(const FTransform& HeldFrame);
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;
	TWeakObjectPtr<UFXR_HandPose> ActiveHandPose;

	// Two-hand state. Raw interactor pointers are held only while attached — they live on the
	// pawn rig and every release path (hand, force, disable) clears them before the hold ends.
	IFXR_Interactor* PrimaryInteractor = nullptr;
	IFXR_Interactor* SecondaryInteractor = nullptr;
	TWeakObjectPtr<UFXR_HandPose> SecondaryHandPose;
	TWeakObjectPtr<UFXR_GripPoint> PrimaryGripPoint;
	TWeakObjectPtr<UFXR_GripPoint> SecondaryGripPoint;

	// Where each hand holds the object, in its local space. Re-resolved every frame so a rail slides
	// under the hand; that freedom is what lets both hands sit exactly on a broom shaft whatever
	// their spacing, instead of one of them hanging in the air.
	FVector PrimaryGripLocal = FVector::ZeroVector;
	FVector SecondaryGripLocal = FVector::ZeroVector;
	bool bHasSecondaryGrip = false;
	/** Object-space frame of the grip-to-grip axis, captured on join — the two-hand rotation reference. */
	FQuat TwoHandLocalFrame = FQuat::Identity;

	// Whether the primary hand mesh currently rides its grip point. Sticky: once attached it stays
	// attached through a re-snap, so a promoted hand travels home with the object instead of
	// teleporting to the controller the instant the other hand lets go.
	bool bPrimaryAttached = false;

	// The second hand rarely lands exactly on its grip, so the aim correction is eased in over a
	// moment rather than snapping the object the instant the hand closes.
	FTransform TwoHandJoinOffset = FTransform::Identity;
	float TwoHandBlend = 1.f;

	FVector LastLocation = FVector::ZeroVector;
	FQuat LastRotation = FQuat::Identity;
	FVector TrackedLinearVelocity = FVector::ZeroVector;
	FVector TrackedAngularVelocity = FVector::ZeroVector;

	float CurrentUseValue = 0.f;
	bool bUsing = false;
};
