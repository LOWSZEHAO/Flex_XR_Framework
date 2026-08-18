// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
#include "Interactor/FXR_Interactor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void UFXR_Grab::OnBegin(IFXR_Interactor* Interactor)
{
	Super::OnBegin(Interactor);
	if (!Interactor)
	{
		return;
	}

	PrimaryInteractor = Interactor;

	UPrimitiveComponent* Driven = ResolveDrivenComponent();
	HeldComponent = Driven;
	if (Driven)
	{
		bRestorePhysics = Driven->IsSimulatingPhysics();
		if (bRestorePhysics)
		{
			Driven->SetSimulatePhysics(false);
		}
		UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
		PrimaryGripPoint = GripPoint;
		ActiveHandPose = GripPoint ? GripPoint->GetHandPose() : nullptr;

		// HeldOffset relates the object to the grip: Driven == HeldOffset * Grip, followed each update.
		SnapProceduralOffset = Driven->GetComponentTransform().GetRelativeTransform(Interactor->GetGripTransform());
		const EFXR_GripSnapMode SnapMode = GripPoint ? GripPoint->GetSnapMode() : EFXR_GripSnapMode::None;

		if (GripPoint && SnapMode != EFXR_GripSnapMode::None)
		{
			// Offset that aligns the grip point to the hand's grip pose. On a rail the alignment
			// point slides to wherever the hand took hold, so a long object is not yanked to centre.
			const FTransform GripPose = GripPoint->GetGripTransformFor(Interactor->GetGripTransform().GetLocation());
			SnapTargetOffset = GripPose.GetRelativeTransform(Driven->GetComponentTransform()).Inverse();

			if (SnapMode == EFXR_GripSnapMode::Smooth)
			{
				// Ease from where it was grabbed to the snapped pose over the next updates.
				HeldOffset = SnapProceduralOffset;
				SnapAlpha = 0.f;
				SnapInterpSpeed = GripPoint->GetSnapInterpSpeed();
			}
			else // Snap (instant)
			{
				HeldOffset = SnapTargetOffset;
				SnapAlpha = 1.f;
				Driven->SetWorldTransform(HeldOffset * Interactor->GetGripTransform(), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		else
		{
			// Procedural hold: object stays where it was grabbed, relative to the grip.
			HeldOffset = SnapProceduralOffset;
			SnapAlpha = 1.f;
		}

		LastLocation = Driven->GetComponentLocation();
		LastRotation = Driven->GetComponentQuat();
		TrackedLinearVelocity = FVector::ZeroVector;
		TrackedAngularVelocity = FVector::ZeroVector;
	}
}

void UFXR_Grab::OnUpdate(IFXR_Interactor* Interactor, float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Grab_OnUpdate);

	if (!Interactor)
	{
		return;
	}

	UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven)
	{
		return;
	}

	// The driver ticks each attached hand; the primary's update does all the work (it reads both
	// grips when two-handed), so the secondary's own tick is a no-op.
	if (Interactor == SecondaryInteractor)
	{
		return;
	}

	if (SecondaryInteractor)
	{
		// Two-hand hold: position and roll ride the primary hand, aim follows the secondary handle
		// (which slides, if that handle is a rail).
		UpdateGripLocals();
		FTransform Aimed = MakeTwoHandTransform();

		// Ease out the join mismatch so the object swings onto aim instead of snapping to it.
		if (TwoHandBlend < 1.f)
		{
			TwoHandBlend = FMath::Min(TwoHandBlend + DeltaTime * TwoHandAimSpeed, 1.f);
			FTransform Eased;
			Eased.Blend(TwoHandJoinOffset * Aimed, Aimed, TwoHandBlend);
			Aimed = Eased;
		}

		Driven->SetWorldTransform(Aimed, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Smooth grip mode: ease the hold from where it was grabbed toward the snapped pose.
		if (SnapAlpha < 1.f)
		{
			SnapAlpha = FMath::Min(SnapAlpha + DeltaTime * SnapInterpSpeed, 1.f);
			HeldOffset.Blend(SnapProceduralOffset, SnapTargetOffset, SnapAlpha);
		}

		// Once the object has arrived, the hand rides its grip point from then on.
		if (SnapAlpha >= 1.f && PrimaryGripPoint.IsValid())
		{
			bPrimaryAttached = true;
		}

		Driven->SetWorldTransform(HeldOffset * Interactor->GetGripTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Use (trigger) edges + analog value while held — the "hold grip, pull trigger" case (guns, flashlights).
	CurrentUseValue = Interactor->GetUseValue();
	if (!bUsing && CurrentUseValue >= UseThreshold)
	{
		bUsing = true;
		OnUseStarted.Broadcast();
	}
	else if (bUsing && CurrentUseValue < UseReleaseThreshold)
	{
		bUsing = false;
		OnUseEnded.Broadcast();
	}

	// Track hand velocity from the driven motion so release can hand it off (ADR-001 release step).
	if (DeltaTime > SMALL_NUMBER)
	{
		const FVector NewLocation = Driven->GetComponentLocation();
		const FQuat NewRotation = Driven->GetComponentQuat();

		TrackedLinearVelocity = (NewLocation - LastLocation) / DeltaTime;

		FQuat DeltaQuat = NewRotation * LastRotation.Inverse();
		DeltaQuat.Normalize();
		FVector Axis;
		float Angle;
		DeltaQuat.ToAxisAndAngle(Axis, Angle);
		if (Angle > PI)
		{
			Angle -= 2.f * PI;
		}
		TrackedAngularVelocity = Axis * (Angle / DeltaTime);

		LastLocation = NewLocation;
		LastRotation = NewRotation;
	}
}

void UFXR_Grab::OnEnd(EFXR_EndReason Reason)
{
	if (UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		if (bRestorePhysics)
		{
			Driven->SetSimulatePhysics(true);
			Driven->SetPhysicsLinearVelocity(TrackedLinearVelocity * ThrowVelocityScale);
			Driven->SetPhysicsAngularVelocityInRadians(TrackedAngularVelocity);
		}
	}

	// Releasing while the trigger is down still ends the use — never strand a latched OnUseStarted.
	if (bUsing)
	{
		bUsing = false;
		OnUseEnded.Broadcast();
	}
	CurrentUseValue = 0.f;

	HeldComponent = nullptr;
	bRestorePhysics = false;
	ActiveHandPose = nullptr;
	PrimaryInteractor = nullptr;
	PrimaryGripPoint = nullptr;
	bPrimaryAttached = false;
	SecondaryInteractor = nullptr;
	SecondaryHandPose = nullptr;
	SecondaryGripPoint = nullptr;
	bHasSecondaryGrip = false;
	TrackedLinearVelocity = FVector::ZeroVector;
	TrackedAngularVelocity = FVector::ZeroVector;
	Super::OnEnd(Reason);
}

bool UFXR_Grab::CanBeginSecondary(IFXR_Interactor* Interactor) const
{
	return bAllowTwoHanded && IsHeld() && Interactor && Interactor != PrimaryInteractor &&
		!SecondaryInteractor && HeldComponent.IsValid();
}

void UFXR_Grab::OnBeginSecondary(IFXR_Interactor* Interactor)
{
	if (!CanBeginSecondary(Interactor))
	{
		return;
	}

	SecondaryInteractor = Interactor;

	// The second hand's grip point (hand-side filtered — e.g. a LeftOnly foregrip) both shapes the
	// fingers and becomes the handle the object aims at.
	UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
	SecondaryGripPoint = GripPoint;
	SecondaryHandPose = GripPoint ? GripPoint->GetHandPose() : nullptr;

	// Resolve where both hands now hold the object (rails slide, point grips do not).
	bHasSecondaryGrip = false;
	UpdateGripLocals();

	// Capture the grip-to-grip axis in object space — the reference the two-hand rotation is built
	// against, so the object keeps the orientation it was grabbed with rather than snapping to one.
	const FVector LocalDelta = (SecondaryGripLocal - PrimaryGripLocal).GetSafeNormal();
	if (!LocalDelta.IsNearlyZero())
	{
		const FVector LocalUp = FMath::Abs(LocalDelta.Z) > 0.95f ? FVector::ForwardVector : FVector::UpVector;
		TwoHandLocalFrame = FRotationMatrix::MakeFromXZ(LocalDelta, LocalUp).ToQuat();
	}

	// Finish any in-flight snap: from here the two-hand solve owns the pose.
	SnapAlpha = 1.f;

	// The hands rarely land exactly on the solved pose, so ease the correction in — snapping the
	// object the instant the second hand closes reads as a jerk.
	TwoHandBlend = 0.f;
	TwoHandJoinOffset = FTransform::Identity;
	if (const UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		TwoHandJoinOffset = Driven->GetComponentTransform().GetRelativeTransform(MakeTwoHandTransform());
	}
}

void UFXR_Grab::ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason)
{
	// Second hand off: back to a one-hand hold, anchored where the object is now.
	if (Interactor && Interactor == SecondaryInteractor)
	{
		SecondaryInteractor = nullptr;
		SecondaryHandPose = nullptr;
		SecondaryGripPoint = nullptr;
		bHasSecondaryGrip = false;
		ReanchorToPrimary();
		return;
	}

	// Primary off while a second hand holds on: the survivor is promoted and carries the hold.
	if (SecondaryInteractor)
	{
		// The trigger hand left — never strand a latched use.
		if (bUsing)
		{
			bUsing = false;
			OnUseEnded.Broadcast();
		}

		// The survivor inherits the grip point too, so it keeps its pose and its rail. It was already
		// riding that grip, so keep it attached: the object is about to travel home to the hand, and
		// the hand should travel with it rather than blink back to the controller.
		PrimaryInteractor = SecondaryInteractor;
		ActiveHandPose = SecondaryHandPose;
		PrimaryGripPoint = SecondaryGripPoint;
		bPrimaryAttached = SecondaryGripPoint.IsValid();
		SecondaryInteractor = nullptr;
		SecondaryHandPose = nullptr;
		SecondaryGripPoint = nullptr;
		bHasSecondaryGrip = false;
		ReanchorToPrimary();
		return;
	}

	// Last hand off — the hold ends (single-hand behaviour).
	OnEnd(Reason);
}

UFXR_HandPose* UFXR_Grab::GetActiveHandPose(EFXR_HandSide Side) const
{
	if (SecondaryInteractor && SecondaryInteractor->GetHandSide() == Side)
	{
		return SecondaryHandPose.Get();
	}
	return ActiveHandPose.Get();
}

void UFXR_Grab::UpdateGripLocals()
{
	const UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven)
	{
		return;
	}

	const FTransform DrivenTransform = Driven->GetComponentTransform();

	// A rail lets the attach point follow the hand along the shaft; a point grip resolves to the
	// same spot every frame. Without a grip point at all, the hand simply holds where it grabbed.
	if (PrimaryInteractor)
	{
		const FVector HandLocation = PrimaryInteractor->GetGripTransform().GetLocation();
		if (const UFXR_GripPoint* GripPoint = PrimaryGripPoint.Get())
		{
			PrimaryGripLocal = DrivenTransform.InverseTransformPosition(GripPoint->GetClosestPointTo(HandLocation));
		}
		else
		{
			PrimaryGripLocal = DrivenTransform.InverseTransformPosition(HandLocation);
		}
	}

	if (SecondaryInteractor)
	{
		const FVector HandLocation = SecondaryInteractor->GetGripTransform().GetLocation();
		if (const UFXR_GripPoint* GripPoint = SecondaryGripPoint.Get())
		{
			SecondaryGripLocal = DrivenTransform.InverseTransformPosition(GripPoint->GetClosestPointTo(HandLocation));
		}
		else
		{
			SecondaryGripLocal = DrivenTransform.InverseTransformPosition(HandLocation);
		}
		bHasSecondaryGrip = true;
	}
}

FTransform UFXR_Grab::MakeTwoHandTransform() const
{
	const FTransform PrimaryGrip = PrimaryInteractor ? PrimaryInteractor->GetGripTransform() : FTransform::Identity;
	const FTransform Base = HeldOffset * PrimaryGrip;

	// Support: the second hand holds on for show, so the first hand's pose stands unchanged.
	if (!SecondaryInteractor || !bHasSecondaryGrip || TwoHandMode == EFXR_TwoHandMode::Support)
	{
		return Base;
	}

	const FVector HandA = PrimaryGrip.GetLocation();
	const FTransform SecondaryGrip = SecondaryInteractor->GetGripTransform();
	const FVector HandB = SecondaryGrip.GetLocation();

	const FVector WorldAxis = (HandB - HandA).GetSafeNormal();
	if (WorldAxis.IsNearlyZero() || (SecondaryGripLocal - PrimaryGripLocal).IsNearlyZero())
	{
		return Base; // Hands (or their grips) coincide — degenerate; hold the one-hand pose.
	}

	// Roll about the hand-to-hand line is the one thing two points cannot determine, so take it
	// from both wrists: twisting either hand rolls the object, as it would in the hand.
	FVector UpReference = (PrimaryGrip.GetRotation().GetUpVector() + SecondaryGrip.GetRotation().GetUpVector()).GetSafeNormal();
	if (UpReference.IsNearlyZero())
	{
		UpReference = FVector::UpVector;
	}

	// Align the object's grip-to-grip axis with the hands' line, then place it so the two midpoints
	// coincide. Symmetric by construction: neither hand is privileged, so either may drive.
	const FQuat WorldFrame = FRotationMatrix::MakeFromXZ(WorldAxis, UpReference).ToQuat();
	const FQuat Rotation = WorldFrame * TwoHandLocalFrame.Inverse();

	const FVector Scale = Base.GetScale3D();
	const FVector MidLocal = (PrimaryGripLocal + SecondaryGripLocal) * 0.5f;
	const FVector MidWorld = (HandA + HandB) * 0.5f;
	const FVector Location = MidWorld - Rotation.RotateVector(MidLocal * Scale);

	return FTransform(Rotation, Location, Scale);
}

bool UFXR_Grab::GetHandAttachTransform(EFXR_HandSide Side, FTransform& OutTransform) const
{
	// The second hand glues to its handle, so it reads as gripping the object rather than hovering
	// beside it. The first hand keeps the controller pose — the object was brought to that hand.
	// Either hand glues to its own grip point. Gluing only the second one made the pair inconsistent:
	// one hand rode the object while the other rode the controller, so they visibly parted as the
	// object turned — and a promoted survivor would snap back to the controller when the other let go.
	const IFXR_Interactor* Interactor = nullptr;
	const UFXR_GripPoint* GripPoint = nullptr;

	if (SecondaryInteractor && SecondaryInteractor->GetHandSide() == Side)
	{
		Interactor = SecondaryInteractor;
		GripPoint = SecondaryGripPoint.Get();
	}
	else if (PrimaryInteractor && PrimaryInteractor->GetHandSide() == Side && bPrimaryAttached)
	{
		// Only once any snap has finished — during a Smooth snap the object is still travelling to
		// the hand, and the hand should wait for it rather than fly out to meet it.
		Interactor = PrimaryInteractor;
		GripPoint = PrimaryGripPoint.Get();
	}

	if (Interactor && GripPoint && GripPoint->GetSnapMode() != EFXR_GripSnapMode::None)
	{
		OutTransform = GripPoint->GetGripTransformFor(Interactor->GetGripTransform().GetLocation());
		return true;
	}
	return false;
}

void UFXR_Grab::ReanchorToPrimary()
{
	const UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven || !PrimaryInteractor)
	{
		return;
	}

	const FTransform PrimaryGrip = PrimaryInteractor->GetGripTransform();

	// Where the object sits right now relative to the surviving hand — the start of the return.
	SnapProceduralOffset = Driven->GetComponentTransform().GetRelativeTransform(PrimaryGrip);

	const UFXR_GripPoint* GripPoint = PrimaryGripPoint.Get();
	const EFXR_GripSnapMode SnapMode = GripPoint ? GripPoint->GetSnapMode() : EFXR_GripSnapMode::None;

	if (!GripPoint || SnapMode == EFXR_GripSnapMode::None)
	{
		// Nothing authored to return to: keep the object where it lies relative to the hand.
		HeldOffset = SnapProceduralOffset;
		SnapAlpha = 1.f;
		return;
	}

	// Re-snap to the surviving hand. Without this the hand inherits whatever offset the other hand
	// had carried the object to, leaving it hanging in space once that hand lets go.
	const FTransform GripPose = GripPoint->GetGripTransformFor(PrimaryGrip.GetLocation());
	SnapTargetOffset = GripPose.GetRelativeTransform(Driven->GetComponentTransform()).Inverse();

	if (SnapMode == EFXR_GripSnapMode::Smooth)
	{
		HeldOffset = SnapProceduralOffset;
		SnapAlpha = 0.f;
		SnapInterpSpeed = GripPoint->GetSnapInterpSpeed();
	}
	else
	{
		HeldOffset = SnapTargetOffset;
		SnapAlpha = 1.f;
	}
}
