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
			// Offset that aligns the grip point to the hand's grip pose.
			SnapTargetOffset = GripPoint->GetComponentTransform().GetRelativeTransform(Driven->GetComponentTransform()).Inverse();

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
		// Two-hand hold: position rides the primary hand, aim follows the hand-to-hand line.
		Driven->SetWorldTransform(TwoHandOffset * MakeTwoHandFrame(), false, nullptr, ETeleportType::TeleportPhysics);
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

	// The second hand's grip point (hand-side filtered — e.g. a LeftOnly foregrip) shapes it.
	const UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
	SecondaryHandPose = GripPoint ? GripPoint->GetHandPose() : nullptr;

	// Capture the object relative to the two-hand frame as it is right now — joining never pops.
	if (const UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		TwoHandOffset = Driven->GetComponentTransform().GetRelativeTransform(MakeTwoHandFrame());
	}
	SnapAlpha = 1.f;
}

void UFXR_Grab::ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason)
{
	// Second hand off: back to a one-hand hold, anchored where the object is now.
	if (Interactor && Interactor == SecondaryInteractor)
	{
		SecondaryInteractor = nullptr;
		SecondaryHandPose = nullptr;
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

FTransform UFXR_Grab::MakeTwoHandFrame() const
{
	const FTransform PrimaryGrip = PrimaryInteractor ? PrimaryInteractor->GetGripTransform() : FTransform::Identity;
	if (!SecondaryInteractor)
	{
		return PrimaryGrip;
	}

	const FVector From = PrimaryGrip.GetLocation();
	const FVector To = SecondaryInteractor->GetGripTransform().GetLocation();
	FVector Aim = To - From;
	if (Aim.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return PrimaryGrip; // Hands coincide — degenerate; hold the primary frame.
	}

	// X aims along the hand-to-hand line; the primary grip's up resolves the roll.
	const FQuat Rotation = FRotationMatrix::MakeFromXZ(Aim, PrimaryGrip.GetRotation().GetUpVector()).ToQuat();
	return FTransform(Rotation, From);
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
