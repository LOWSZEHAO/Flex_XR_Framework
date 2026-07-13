// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Latch.h"
#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
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

		// A latch is a constrained mechanism (door/drawer/lever) — always kinematically driven along
		// its rail, never free-falling. Force physics off so gravity can't fight the solver or drop the
		// mesh before it's grabbed (drop-a-component-works; no manual "uncheck Simulate Physics" step).
		if (Driven->IsSimulatingPhysics())
		{
			Driven->SetSimulatePhysics(false);
		}
	}
	else
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_Latch '%s' on '%s': no driven mesh resolved — attach the latch UNDER the mesh it should move (the mesh must be the latch's parent, or the actor's root). It will not respond to grabs."),
			*GetName(), *GetNameSafe(GetOwner()));
	}
	// Start pose: begin at the authored StartValue (0 = rest), snapped to a state if detented.
	CurrentValue = FMath::Clamp(StartValue, MinLimit, MaxLimit);
	if (bSnapToStates)
	{
		CurrentValue = StateValue(NearestStateIndex(CurrentValue));
	}
	LastValidValue = CurrentValue;
	CurrentState = NearestStateIndex(CurrentValue);
	LastBroadcastValue = GetLatchValue();

	// Drive the mesh to the start pose if it isn't the authored rest.
	if (Driven.IsValid() && !FMath::IsNearlyZero(CurrentValue))
	{
		ApplyValue();
	}
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

	// Pose the hand around the nearest grip point (e.g. a lever handle) while driving the latch;
	// keep the grip point as the reference for the too-far auto-detach.
	UFXR_GripPoint* GripPoint = SelectGripPoint(Interactor);
	ActiveGripPoint = GripPoint;
	ActiveHandPose = GripPoint ? GripPoint->GetHandPose() : nullptr;

	// Hand-to-handle snap follows the grip point's snap mode: Smooth eases from the grabbed pose,
	// Snap arrives instantly, None keeps the hand on the controller.
	HandSnapMode = GripPoint ? GripPoint->GetSnapMode() : EFXR_GripSnapMode::None;
	HandSnapSpeed = GripPoint ? GripPoint->GetSnapInterpSpeed() : 10.f;
	HandSnapAlpha = (HandSnapMode == EFXR_GripSnapMode::Smooth) ? 0.f : 1.f;
	if (GripPoint)
	{
		EasedHandTransform.Blend(Interactor->GetGripTransform(), GripPoint->GetComponentTransform(), HandSnapAlpha);
	}

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

	// Auto-detach: the latch can't follow the hand, so if the hand strays too far from the handle
	// (grip point, else the driven mesh), release it rather than driving from an implausible reach.
	if (DetachDistance > 0.f)
	{
		const FVector RefLocation = ActiveGripPoint.IsValid() ? ActiveGripPoint->GetComponentLocation() : GetInteractionLocation();
		if (FVector::DistSquared(HandLocation, RefLocation) > FMath::Square(DetachDistance))
		{
			ForceRelease();
			return;
		}
	}

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
	BroadcastValueAndState();

	// Advance the hand-to-handle ease (Smooth) and track the moving handle for the hand mesh to follow.
	if (ActiveGripPoint.IsValid())
	{
		if (HandSnapMode == EFXR_GripSnapMode::Smooth && HandSnapAlpha < 1.f)
		{
			HandSnapAlpha = FMath::Min(HandSnapAlpha + DeltaTime * HandSnapSpeed, 1.f);
		}
		EasedHandTransform.Blend(Interactor->GetGripTransform(), ActiveGripPoint->GetComponentTransform(), HandSnapAlpha);
	}
}

void UFXR_Latch::OnEnd(EFXR_EndReason Reason)
{
	// Detented release: settle on the nearest state so a switch/lever lands cleanly on a position.
	if (bSnapToStates && Driven.IsValid())
	{
		CurrentValue = StateValue(NearestStateIndex(CurrentValue));
		LastValidValue = CurrentValue;
		ApplyValue();
		BroadcastValueAndState();
	}

	ActiveHandPose = nullptr;
	ActiveGripPoint = nullptr;
	Super::OnEnd(Reason);
}

UFXR_HandPose* UFXR_Latch::GetActiveHandPose() const
{
	return ActiveHandPose.Get();
}

bool UFXR_Latch::GetHandAttachTransform(FTransform& OutTransform) const
{
	// Pin the hand to the handle (eased per the grip point's snap mode) so it tracks the object as
	// the latch moves. Snap mode None — or no grip point — keeps the hand on the controller.
	if (ActiveGripPoint.IsValid() && HandSnapMode != EFXR_GripSnapMode::None)
	{
		OutTransform = EasedHandTransform;
		return true;
	}
	return false;
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

float UFXR_Latch::GetLatchValue() const
{
	const float Range = MaxLimit - MinLimit;
	return FMath::IsNearlyZero(Range) ? 0.f : FMath::Clamp((CurrentValue - MinLimit) / Range, 0.f, 1.f);
}

float UFXR_Latch::StateValue(int32 Index) const
{
	if (NumStates <= 1)
	{
		return MinLimit;
	}
	const float T = static_cast<float>(FMath::Clamp(Index, 0, NumStates - 1)) / static_cast<float>(NumStates - 1);
	return FMath::Lerp(MinLimit, MaxLimit, T);
}

int32 UFXR_Latch::NearestStateIndex(float Value) const
{
	const float Range = MaxLimit - MinLimit;
	if (NumStates <= 1 || FMath::IsNearlyZero(Range))
	{
		return 0;
	}
	const float T = (Value - MinLimit) / Range;
	return FMath::Clamp(FMath::RoundToInt(T * static_cast<float>(NumStates - 1)), 0, NumStates - 1);
}

void UFXR_Latch::BroadcastValueAndState()
{
	const float NewLatchValue = GetLatchValue();
	if (!FMath::IsNearlyEqual(NewLatchValue, LastBroadcastValue, 1e-3f))
	{
		LastBroadcastValue = NewLatchValue;
		OnLatchValueChanged.Broadcast(NewLatchValue);
	}

	if (bSnapToStates)
	{
		const int32 NewState = NearestStateIndex(CurrentValue);
		if (NewState != CurrentState)
		{
			CurrentState = NewState;
			OnStateChanged.Broadcast(CurrentState);
		}
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

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// Catches the most common setup mistake: the mesh parented *under* the latch (the rule looks up,
	// not down), so nothing gets driven. The latch must be a child of the mesh, or the mesh the root.
	if (!ResolveDrivenComponent())
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_Latch '%s' on '%s': no driven mesh — attach the latch under the mesh it moves (mesh = the latch's parent, or the actor root). Nothing will move."),
			*GetName(), *OwnerActor->GetName());
	}

	if (MinLimit >= MaxLimit)
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_Latch '%s' on '%s': Min Limit (%.1f) >= Max Limit (%.1f) — no travel range."),
			*GetName(), *OwnerActor->GetName(), MinLimit, MaxLimit);
	}

	if (MotionType != EFXR_LatchMotion::Rotational)
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
