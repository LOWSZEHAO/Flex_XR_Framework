// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Hand/FXR_HandVisual.h"
#include "Interactor/FXR_Interactor.h"
#include "Rig/FXR_Pawn.h"
#include "Driver/FXR_InteractionDriver.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Interactable/FXR_HandPose.h"
#include "GameFramework/Actor.h"

UFXR_HandVisual::UFXR_HandVisual()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_HandVisual::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	IFXR_Interactor* Interactor = ResolveActiveInteractor();
	if (!Interactor)
	{
		return;
	}

	if (bFollowInteractorPose)
	{
		// Controllers and hand tracking hand back grip poses in different frames (motion-controller
		// grip vs OpenXR palm), so each input source gets its own mesh-alignment offset.
		const bool bHandTracking = (Interactor->GetInteractorType() == EFXR_InteractorType::TrackedHand);
		const FTransform& Offset = bHandTracking ? HandTrackingPoseOffset : GripPoseOffset;

		// Follow location + rotation only, preserving this mesh's authored scale — a negative
		// scale (mirroring the right-hand mesh into a left hand) must survive the follow.
		const FTransform Target = Offset * Interactor->GetGripTransform();
		SetWorldLocationAndRotation(Target.GetLocation(), Target.GetRotation());
	}

	GripAlpha = FMath::FInterpTo(GripAlpha, Interactor->GetSelectValue(), DeltaTime, BlendSpeed);
	TriggerAlpha = FMath::FInterpTo(TriggerAlpha, Interactor->GetUseValue(), DeltaTime, BlendSpeed);

	// Finger curls: the held grip point's authored pose, else a uniform grip-driven curl.
	FFXR_FingerCurls Target;
	if (const UFXR_HandPose* Pose = ResolveHeldHandPose())
	{
		Target = Pose->Curls;
	}
	else
	{
		Target.Thumb = Target.Index = Target.Middle = Target.Ring = Target.Pinky = GripAlpha;
	}
	FingerCurls.Thumb = FMath::FInterpTo(FingerCurls.Thumb, Target.Thumb, DeltaTime, BlendSpeed);
	FingerCurls.Index = FMath::FInterpTo(FingerCurls.Index, Target.Index, DeltaTime, BlendSpeed);
	FingerCurls.Middle = FMath::FInterpTo(FingerCurls.Middle, Target.Middle, DeltaTime, BlendSpeed);
	FingerCurls.Ring = FMath::FInterpTo(FingerCurls.Ring, Target.Ring, DeltaTime, BlendSpeed);
	FingerCurls.Pinky = FMath::FInterpTo(FingerCurls.Pinky, Target.Pinky, DeltaTime, BlendSpeed);
	FingerCurls.ThumbOpposition = FMath::FInterpTo(FingerCurls.ThumbOpposition, Target.ThumbOpposition, DeltaTime, BlendSpeed);
}

const UFXR_HandPose* UFXR_HandVisual::ResolveHeldHandPose() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const UFXR_InteractionDriver* Driver = OwnerActor->FindComponentByClass<UFXR_InteractionDriver>();
	if (!Driver)
	{
		return nullptr;
	}

	if (const UFXR_InteractableBase* Held = Driver->GetHeldInteractable(HandSide))
	{
		// Any interactable can offer a pose (Grab, Latch, ...) via the base virtual.
		return Held->GetActiveHandPose();
	}
	return nullptr;
}

float UFXR_HandVisual::GetGrasp() const
{
	return FMath::Max(FingerCurls.Index, FMath::Max(FingerCurls.Middle, FMath::Max(FingerCurls.Ring, FingerCurls.Pinky)));
}

IFXR_Interactor* UFXR_HandVisual::ResolveActiveInteractor() const
{
	if (AFXR_Pawn* Pawn = Cast<AFXR_Pawn>(GetOwner()))
	{
		return Pawn->GetActiveInteractor(HandSide);
	}
	return nullptr;
}
