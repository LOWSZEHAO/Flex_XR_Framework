// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
#include "Interactor/FXR_Interactor.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

void UFXR_Grab::OnBegin(IFXR_Interactor* Interactor)
{
	Super::OnBegin(Interactor);
	if (!Interactor)
	{
		return;
	}

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
	if (!Interactor)
	{
		return;
	}

	UPrimitiveComponent* Driven = HeldComponent.Get();
	if (!Driven)
	{
		return;
	}

	// Smooth grip mode: ease the hold from where it was grabbed toward the snapped pose.
	if (SnapAlpha < 1.f)
	{
		SnapAlpha = FMath::Min(SnapAlpha + DeltaTime * SnapInterpSpeed, 1.f);
		HeldOffset.Blend(SnapProceduralOffset, SnapTargetOffset, SnapAlpha);
	}

	Driven->SetWorldTransform(HeldOffset * Interactor->GetGripTransform(), false, nullptr, ETeleportType::TeleportPhysics);

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

	HeldComponent = nullptr;
	bRestorePhysics = false;
	ActiveHandPose = nullptr;
	TrackedLinearVelocity = FVector::ZeroVector;
	TrackedAngularVelocity = FVector::ZeroVector;
	Super::OnEnd(Reason);
}

UFXR_HandPose* UFXR_Grab::GetActiveHandPose() const
{
	return ActiveHandPose.Get();
}

UFXR_GripPoint* UFXR_Grab::SelectGripPoint(IFXR_Interactor* Interactor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !Interactor)
	{
		return nullptr;
	}

	TArray<UFXR_GripPoint*> GripPoints;
	OwnerActor->GetComponents<UFXR_GripPoint>(GripPoints);
	if (GripPoints.Num() == 0)
	{
		return nullptr;
	}

	const EFXR_HandSide Side = Interactor->GetHandSide();
	const FVector GripLocation = Interactor->GetGripTransform().GetLocation();

	UFXR_GripPoint* Best = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Min();
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (UFXR_GripPoint* Point : GripPoints)
	{
		if (!Point || !Point->AcceptsHand(Side))
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(GripLocation, Point->GetComponentLocation());
		if (DistanceSq > FMath::Square(Point->GetActivationRadius()))
		{
			continue;
		}
		if (Point->GetPriority() > BestPriority ||
			(Point->GetPriority() == BestPriority && DistanceSq < BestDistanceSq))
		{
			Best = Point;
			BestPriority = Point->GetPriority();
			BestDistanceSq = DistanceSq;
		}
	}

	return Best;
}
