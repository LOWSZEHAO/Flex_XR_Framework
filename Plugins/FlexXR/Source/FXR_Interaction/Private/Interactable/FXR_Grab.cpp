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
		const UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
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
		UpdateSecondaryGripLocal();
		Driven->SetWorldTransform(MakeTwoHandTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Smooth grip mode: ease the hold from where it was grabbed toward the snapped pose.
		if (SnapAlpha < 1.f)
		{
			SnapAlpha = FMath::Min(SnapAlpha + DeltaTime * SnapInterpSpeed, 1.f);
			HeldOffset.Blend(SnapProceduralOffset, SnapTargetOffset, SnapAlpha);
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

	// Remember where that handle sits on the object, so the aim survives the object moving. A rail
	// re-resolves each frame instead (see UpdateSecondaryGripLocal), letting the hand slide along it.
	bHasSecondaryGrip = false;
	if (GripPoint)
	{
		UpdateSecondaryGripLocal();
	}

	// Finish any in-flight snap: from here the two-hand solve owns the pose.
	SnapAlpha = 1.f;
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

		PrimaryInteractor = SecondaryInteractor;
		ActiveHandPose = SecondaryHandPose;
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

void UFXR_Grab::UpdateSecondaryGripLocal()
{
	const UFXR_GripPoint* GripPoint = SecondaryGripPoint.Get();
	const UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!GripPoint || !Driven || !SecondaryInteractor)
	{
		return;
	}

	// On a rail the attach point follows the hand along the handguard; a point grip is fixed, so
	// resolving it every frame simply returns the same spot.
	const FVector HandLocation = SecondaryInteractor->GetGripTransform().GetLocation();
	SecondaryGripLocal = Driven->GetComponentTransform().InverseTransformPosition(GripPoint->GetClosestPointTo(HandLocation));
	bHasSecondaryGrip = true;
}

FTransform UFXR_Grab::MakeTwoHandTransform() const
{
	// Start from the one-hand hold: the first hand keeps the object's position and roll.
	const FTransform PrimaryGrip = PrimaryInteractor ? PrimaryInteractor->GetGripTransform() : FTransform::Identity;
	const FTransform Base = HeldOffset * PrimaryGrip;

	if (!SecondaryInteractor || !bHasSecondaryGrip)
	{
		return Base;
	}

	// Then pivot about the first hand so the authored secondary grip point swings onto the second
	// hand — the object points its foregrip at you, rather than an arbitrary local axis.
	const FVector Pivot = PrimaryGrip.GetLocation();
	const FVector CurrentDir = (Base.TransformPosition(SecondaryGripLocal) - Pivot).GetSafeNormal();
	const FVector DesiredDir = (SecondaryInteractor->GetGripTransform().GetLocation() - Pivot).GetSafeNormal();

	if (CurrentDir.IsNearlyZero() || DesiredDir.IsNearlyZero())
	{
		return Base; // Hands coincide with the grip — degenerate; hold the one-hand pose.
	}

	const FQuat Swing = FQuat::FindBetweenNormals(CurrentDir, DesiredDir);
	const FQuat NewRotation = Swing * Base.GetRotation();
	const FVector NewLocation = Pivot + Swing.RotateVector(Base.GetLocation() - Pivot);
	return FTransform(NewRotation, NewLocation, Base.GetScale3D());
}

bool UFXR_Grab::GetHandAttachTransform(EFXR_HandSide Side, FTransform& OutTransform) const
{
	// The second hand glues to its handle, so it reads as gripping the object rather than hovering
	// beside it. The first hand keeps the controller pose — the object was brought to that hand.
	if (SecondaryInteractor && SecondaryInteractor->GetHandSide() == Side)
	{
		if (const UFXR_GripPoint* GripPoint = SecondaryGripPoint.Get())
		{
			OutTransform = GripPoint->GetGripTransformFor(SecondaryInteractor->GetGripTransform().GetLocation());
			return true;
		}
	}
	return false;
}

void UFXR_Grab::ReanchorToPrimary()
{
	if (const UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		if (PrimaryInteractor)
		{
			HeldOffset = Driven->GetComponentTransform().GetRelativeTransform(PrimaryInteractor->GetGripTransform());
			SnapAlpha = 1.f;
		}
	}
}
