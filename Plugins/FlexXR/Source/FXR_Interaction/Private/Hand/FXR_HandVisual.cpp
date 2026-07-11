// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Hand/FXR_HandVisual.h"
#include "Interactor/FXR_Interactor.h"
#include "Rig/FXR_Pawn.h"

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
		// Follow location + rotation only, preserving this mesh's authored scale — a negative
		// scale (mirroring the right-hand mesh into a left hand) must survive the follow.
		const FTransform Target = GripPoseOffset * Interactor->GetGripTransform();
		SetWorldLocationAndRotation(Target.GetLocation(), Target.GetRotation());
	}

	GripAlpha = FMath::FInterpTo(GripAlpha, Interactor->GetSelectValue(), DeltaTime, BlendSpeed);
	TriggerAlpha = FMath::FInterpTo(TriggerAlpha, Interactor->GetUseValue(), DeltaTime, BlendSpeed);
}

IFXR_Interactor* UFXR_HandVisual::ResolveActiveInteractor() const
{
	if (AFXR_Pawn* Pawn = Cast<AFXR_Pawn>(GetOwner()))
	{
		return Pawn->GetActiveInteractor(HandSide);
	}
	return nullptr;
}
