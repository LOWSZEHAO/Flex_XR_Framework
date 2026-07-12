// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_GripPoint.h"
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
		if (const UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor); GripPoint && GripPoint->ShouldSnap())
		{
			// Snap: move the object so the chosen grip point aligns to the hand's grip pose.
			const FTransform GripPointInDriven = GripPoint->GetComponentTransform().GetRelativeTransform(Driven->GetComponentTransform());
			HeldOffset = GripPointInDriven.Inverse();
			Driven->SetWorldTransform(HeldOffset * Interactor->GetGripTransform(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			// Procedural hold: Driven == HeldOffset * Grip, so the object stays where it was grabbed as the grip moves.
			HeldOffset = Driven->GetComponentTransform().GetRelativeTransform(Interactor->GetGripTransform());
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
	TrackedLinearVelocity = FVector::ZeroVector;
	TrackedAngularVelocity = FVector::ZeroVector;
	Super::OnEnd(Reason);
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
