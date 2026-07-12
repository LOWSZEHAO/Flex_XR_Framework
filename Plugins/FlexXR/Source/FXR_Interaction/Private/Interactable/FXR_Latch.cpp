// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Latch.h"
#include "Interactable/FXR_GripPoint.h"
#include "Solver/FXR_ConstraintSolver.h"
#include "Interactor/FXR_Interactor.h"
#include "Types/FXR_LogChannels.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

void UFXR_Latch::BeginPlay()
{
	Super::BeginPlay();

	// The component's own transform is the pivot; cache it (and the driven rest) at t=0 so a latch
	// parented under the driven mesh doesn't orbit its own moving transform.
	Driven = ResolveDrivenComponent();
	PivotRestWorld = GetComponentTransform();
	if (Driven.IsValid())
	{
		DrivenRestWorld = Driven->GetComponentTransform();
	}
	CurrentValue = 0.f;
	LastValidValue = 0.f;
}

void UFXR_Latch::OnBegin(IFXR_Interactor* Interactor)
{
	Super::OnBegin(Interactor);
	if (!Interactor)
	{
		return;
	}

	ValueAtGrab = CurrentValue;
	LastValidValue = CurrentValue;

	const FVector HandLocation = Interactor->GetGripTransform().GetLocation();
	if (MotionType == EFXR_LatchMotion::Rotational)
	{
		float LeverArm = 0.f;
		HandRefInPlaneDir = FFXR_ConstraintSolver::ProjectToRotationPlane(GetPivotLocation(), GetAxisWorld(), HandLocation, LeverArm);
	}
	else
	{
		HandRefAxisProj = FFXR_ConstraintSolver::ProjectToAxis(GetPivotLocation(), GetAxisWorld(), HandLocation);
	}
}

void UFXR_Latch::OnUpdate(IFXR_Interactor* Interactor, float DeltaTime)
{
	if (!Interactor || !Driven.IsValid())
	{
		return;
	}

	const FVector HandLocation = Interactor->GetGripTransform().GetLocation();
	const FVector Pivot = GetPivotLocation();
	const FVector Axis = GetAxisWorld();

	if (MotionType == EFXR_LatchMotion::Rotational)
	{
		float LeverArm = 0.f;
		const FVector InPlane = FFXR_ConstraintSolver::ProjectToRotationPlane(Pivot, Axis, HandLocation, LeverArm);
		if (LeverArm < MinLeverArm)
		{
			// Singularity guard: hand too close to the axis — hold the last valid angle.
			CurrentValue = LastValidValue;
		}
		else
		{
			const float DeltaRadians = FFXR_ConstraintSolver::SignedAngleAroundAxis(HandRefInPlaneDir, InPlane, Axis);
			CurrentValue = FMath::Clamp(ValueAtGrab + FMath::RadiansToDegrees(DeltaRadians), MinLimit, MaxLimit);
			LastValidValue = CurrentValue;
		}
	}
	else
	{
		const float AxisProj = FFXR_ConstraintSolver::ProjectToAxis(Pivot, Axis, HandLocation);
		CurrentValue = FMath::Clamp(ValueAtGrab + (AxisProj - HandRefAxisProj), MinLimit, MaxLimit);
	}

	ApplyValue();
}

void UFXR_Latch::ApplyValue()
{
	UPrimitiveComponent* DrivenComponent = Driven.Get();
	if (!DrivenComponent)
	{
		return;
	}

	const FVector Axis = GetAxisWorld();
	const FVector Pivot = GetPivotLocation();

	if (MotionType == EFXR_LatchMotion::Rotational)
	{
		const FQuat Rotation(Axis, FMath::DegreesToRadians(CurrentValue));
		const FVector NewLocation = Pivot + Rotation.RotateVector(DrivenRestWorld.GetLocation() - Pivot);
		const FQuat NewRotation = Rotation * DrivenRestWorld.GetRotation();
		DrivenComponent->SetWorldTransform(FTransform(NewRotation, NewLocation, DrivenRestWorld.GetScale3D()), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		const FVector NewLocation = DrivenRestWorld.GetLocation() + Axis * CurrentValue;
		DrivenComponent->SetWorldTransform(FTransform(DrivenRestWorld.GetRotation(), NewLocation, DrivenRestWorld.GetScale3D()), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FVector UFXR_Latch::AxisUnitFor(EFXR_LatchAxis Axis) const
{
	switch (Axis)
	{
	case EFXR_LatchAxis::X: return FVector::ForwardVector;
	case EFXR_LatchAxis::Y: return FVector::RightVector;
	default:               return FVector::UpVector;
	}
}

FVector UFXR_Latch::GetAxisWorld() const
{
	return PivotRestWorld.GetRotation().RotateVector(AxisUnitFor(MotionAxis)).GetSafeNormal();
}

FVector UFXR_Latch::GetPivotLocation() const
{
	return PivotRestWorld.GetLocation();
}

#if WITH_EDITOR
void UFXR_Latch::CheckForErrors()
{
	Super::CheckForErrors();

	if (MotionType != EFXR_LatchMotion::Rotational)
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// At author time the component's live transform is the pivot (nothing has driven it yet).
	const FTransform PivotWorld = GetComponentTransform();
	const FVector Axis = PivotWorld.GetRotation().RotateVector(AxisUnitFor(MotionAxis)).GetSafeNormal();
	const FVector Pivot = PivotWorld.GetLocation();

	TArray<UFXR_GripPoint*> GripPoints;
	OwnerActor->GetComponents<UFXR_GripPoint>(GripPoints);
	for (const UFXR_GripPoint* GripPoint : GripPoints)
	{
		if (!GripPoint)
		{
			continue;
		}
		float LeverArm = 0.f;
		FFXR_ConstraintSolver::ProjectToRotationPlane(Pivot, Axis, GripPoint->GetComponentLocation(), LeverArm);
		if (LeverArm < MinLeverArm)
		{
			UE_LOG(LogFXR, Warning,
				TEXT("FXR_Latch '%s' on '%s': grip point '%s' is %.1f cm from the rotation axis, within MinLeverArm (%.1f cm) — gripping there will jitter near the rotation singularity. Move the grip point outward."),
				*GetName(), *OwnerActor->GetName(), *GripPoint->GetName(), LeverArm, MinLeverArm);
		}
	}
}
#endif
