// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
#include "DrawDebugHelpers.h"

UFXR_HandPose* UFXR_GripPoint::GetHandPose() const
{
	return HandPose;
}

UFXR_GripPoint::UFXR_GripPoint()
{
	// Tick only exists for the optional debug draw; disabled unless bDrawDebug is set.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_GripPoint::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(bDrawDebug);
}

void UFXR_GripPoint::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDebug)
	{
		if (const UWorld* World = GetWorld())
		{
			const FTransform Transform = GetComponentTransform();
			DrawDebugCoordinateSystem(World, Transform.GetLocation(), Transform.Rotator(), 5.f, false, -1.f, 0, 0.3f);
			DrawDebugSphere(World, Transform.GetLocation(), ActivationRadius, 12, FColor::Magenta, false, -1.f, 0, 0.3f);
		}
	}
}

bool UFXR_GripPoint::AcceptsHand(EFXR_HandSide Side) const
{
	switch (Handedness)
	{
	case EFXR_GripHandedness::LeftOnly:  return Side == EFXR_HandSide::Left;
	case EFXR_GripHandedness::RightOnly: return Side == EFXR_HandSide::Right;
	default:                             return true;
	}
}
