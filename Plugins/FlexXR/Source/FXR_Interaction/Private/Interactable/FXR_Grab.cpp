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
		// Not re-read when a distance grab already captured it: by now the body is disabled, and
		// reading it here would decide the object should stay kinematic forever after release.
		if (!bPhysicsCaptured)
		{
			bRestorePhysics = Driven->IsSimulatingPhysics();
			bPhysicsCaptured = true;
		}
		// Everything the hold moves is parked, not just the driven mesh: a sibling body left
		// simulating would fight the hold and tear the object apart.
		ParkPhysics();

		UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
		PrimaryGripPoint = GripPoint;
		ActiveHandPose = GripPoint ? GripPoint->GetHandPose() : nullptr;

		// HeldOffset relates the object to the grip: Driven == HeldOffset * Grip, followed each update.
		SnapProceduralOffset = GetHeldTransform().GetRelativeTransform(Interactor->GetGripTransform());
		const EFXR_GripSnapMode SnapMode = GripPoint ? GripPoint->GetSnapMode() : EFXR_GripSnapMode::None;

		if (GripPoint && SnapMode != EFXR_GripSnapMode::None)
		{
			// Offset that aligns the grip point to the hand's grip pose. On a rail the alignment
			// point slides to wherever the hand took hold, so a long object is not yanked to centre.
			const FTransform GripPose = GripPoint->GetGripTransformFor(Interactor->GetGripTransform().GetLocation());
			SnapTargetOffset = GripPose.GetRelativeTransform(GetHeldTransform()).Inverse();

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
				SetHeldTransform(HeldOffset * Interactor->GetGripTransform());
			}
		}
		else
		{
			// Procedural hold: object stays where it was grabbed, relative to the grip.
			HeldOffset = SnapProceduralOffset;
			SnapAlpha = 1.f;
		}

		LastLocation = GetHeldTransform().GetLocation();
		LastRotation = GetHeldTransform().GetRotation();
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

	// Still on its way in from a distance grab: fly it, and hand over to the ordinary hold on arrival.
	if (bFlying)
	{
		TickDistanceGrab(Interactor, DeltaTime);
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

		SetHeldTransform(Aimed);
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

		SetHeldTransform(HeldOffset * Interactor->GetGripTransform());
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
		const FVector NewLocation = GetHeldTransform().GetLocation();
		const FQuat NewRotation = GetHeldTransform().GetRotation();

		// While the framework is settling the object — a grip snap, the two-hand join, or the return
		// to a promoted hand — the object moves on its own. That is not a throw, and handing that
		// speed to physics on release would fling it away.
		const bool bSettling = (SnapAlpha < 1.f) || (TwoHandBlend < 1.f);
		if (bSettling)
		{
			TrackedLinearVelocity = FVector::ZeroVector;
			TrackedAngularVelocity = FVector::ZeroVector;
		}
		else
		{
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
		}

		LastLocation = NewLocation;
		LastRotation = NewRotation;
	}
}

void UFXR_Grab::OnEnd(EFXR_EndReason Reason)
{
	// Every body this hold parked comes back, so a multi-part object falls as a whole rather than
	// leaving its loose pieces frozen in mid-air.
	RestorePhysics();

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
	bPhysicsCaptured = false;
	bFlying = false;
	FlightElapsed = 0.f;
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
		TwoHandJoinOffset = GetHeldTransform().GetRelativeTransform(MakeTwoHandTransform());
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

	const FTransform DrivenTransform = GetHeldTransform();

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
	SnapProceduralOffset = GetHeldTransform().GetRelativeTransform(PrimaryGrip);

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
	SnapTargetOffset = GripPose.GetRelativeTransform(GetHeldTransform()).Inverse();

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

void UFXR_Grab::BeginDistanceGrab(IFXR_Interactor* Interactor)
{
	UPrimitiveComponent* Driven = ResolveDrivenComponent();
	if (!Interactor || !Driven || !bDistanceGrab)
	{
		return;
	}

	PrimaryInteractor = Interactor;
	HeldComponent = Driven;

	// Captured before the body is disabled, so release still restores simulation. OnBegin honours
	// this on arrival rather than re-reading an already-kinematic body.
	bRestorePhysics = Driven->IsSimulatingPhysics();
	bPhysicsCaptured = true;
	ParkPhysics();

	bFlying = true;
	FlightElapsed = 0.f;
	FlightStart = GetHeldTransform();

	// Held from the moment the hand commits, so nothing else can claim the object mid-flight and the
	// driver keeps updating it. The Began event waits for arrival, when it is genuinely in hand.
	bHeld = true;
}

FTransform UFXR_Grab::ComputeDistanceGrabTarget(IFXR_Interactor* Interactor) const
{
	const FTransform Grip = Interactor->GetGripTransform();
	const UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven)
	{
		return Grip;
	}

	// Aim the flight at the pose the object would be held in, so it arrives already seated and the
	// handover to the ordinary hold is invisible rather than a snap at the end.
	if (UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor))
	{
		const FTransform PointRelative = GripPoint->GetComponentTransform().GetRelativeTransform(GetHeldTransform());
		return PointRelative.Inverse() * Grip;
	}
	return Grip;
}

void UFXR_Grab::TickDistanceGrab(IFXR_Interactor* Interactor, float DeltaTime)
{
	UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven)
	{
		bFlying = false;
		return;
	}

	FlightElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(FlightElapsed / FMath::Max(DistanceGrabDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	// Eased rather than linear: a constant-velocity slide reads as a conveyor belt, while easing out
	// lets the object settle into the hand. Re-aimed every frame, so it tracks a hand that moves.
	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
	FTransform Current;
	Current.Blend(FlightStart, ComputeDistanceGrabTarget(Interactor), Eased);
	SetHeldTransform(Current);

	if (Alpha >= 1.f)
	{
		// Arrived: hand over to the ordinary hold, which is what makes grip points, pose blending,
		// two-hand and throw behave identically whether the object was reached for or summoned.
		bFlying = false;
		OnBegin(Interactor);
	}
}

void UFXR_Grab::NotifyParkedPhysics(bool bWasSimulating)
{
	// Same guard the distance-grab flight uses: whoever parked the body knows what it was doing
	// before, and OnBegin must trust that rather than reading the parked state.
	bRestorePhysics = bWasSimulating;
	bPhysicsCaptured = true;
}

FTransform UFXR_Grab::GetHeldTransform() const
{
	// Whole Actor is the frame everything else is expressed in: grip offsets, the two-hand solve, the
	// distance-grab flight and a socket's seat pose all read and write through here, so they stay
	// consistent whichever scope is set.
	if (GrabScope == EFXR_GrabScope::WholeActor)
	{
		if (const AActor* Owner = GetOwner())
		{
			return Owner->GetActorTransform();
		}
	}

	const UPrimitiveComponent* Driven = HeldComponent.IsValid() ? HeldComponent.Get() : ResolveDrivenComponent();
	return Driven ? Driven->GetComponentTransform() : GetComponentTransform();
}

void UFXR_Grab::SetHeldTransform(const FTransform& NewTransform)
{
	if (GrabScope == EFXR_GrabScope::WholeActor)
	{
		if (AActor* Owner = GetOwner())
		{
			Owner->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);

			// A component that has simulated stops following its parent by attachment even once it is
			// kinematic again, so moving the actor leaves it behind. Parked bodies are driven from the
			// same frame instead — which is what the solver does for everything else anyway.
			PlaceParkedBodies(NewTransform);
			return;
		}
	}

	if (UPrimitiveComponent* Driven = HeldComponent.IsValid() ? HeldComponent.Get() : ResolveDrivenComponent())
	{
		Driven->SetWorldTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool UFXR_Grab::MovesComponent(const USceneComponent* Component) const
{
	if (!Component)
	{
		return false;
	}

	if (GrabScope == EFXR_GrabScope::WholeActor)
	{
		return Component->GetOwner() == GetOwner();
	}

	// Driven Mesh takes whatever hangs beneath it too — a handle parented to the door travels with
	// the door, which is the whole point of parenting it there.
	const USceneComponent* Driven = HeldComponent.IsValid() ? HeldComponent.Get() : ResolveDrivenComponent();
	return Driven && (Component == Driven || Component->IsAttachedTo(Driven));
}

void UFXR_Grab::ParkPhysics()
{
	ParkedBodies.Reset();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FTransform HeldFrame = GetHeldTransform();

	TArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		// Only what actually simulates, and only what this hold moves: parking a body the hold does
		// not carry would freeze part of the level for no reason.
		if (Primitive && Primitive->IsSimulatingPhysics() && MovesComponent(Primitive))
		{
			// Offset captured before parking, so the body can be driven against the held frame from
			// here on rather than relying on attachment, which it no longer follows.
			ParkedBodies.Add({ Primitive, Primitive->GetComponentTransform().GetRelativeTransform(HeldFrame) });
			Primitive->SetSimulatePhysics(false);
		}
	}
}

void UFXR_Grab::RestorePhysics()
{
	for (const FParkedBody& Parked : ParkedBodies)
	{
		if (UPrimitiveComponent* Primitive = Parked.Body.Get())
		{
			// Re-placed after simulation resumes, so the body carries on from where the hold left it
			// rather than snapping back to wherever it was picked up.
			const FTransform Placed = Primitive->GetComponentTransform();
			Primitive->SetSimulatePhysics(true);
			Primitive->SetWorldTransform(Placed, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	ParkedBodies.Reset();
}

void UFXR_Grab::PlaceParkedBodies(const FTransform& HeldFrame)
{
	for (const FParkedBody& Parked : ParkedBodies)
	{
		if (UPrimitiveComponent* Body = Parked.Body.Get())
		{
			Body->SetWorldTransform(Parked.RelativeToHeld * HeldFrame, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}
