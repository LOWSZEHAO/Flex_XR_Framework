// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactor/FXR_InteractorComponent.h"

UFXR_InteractorComponent::UFXR_InteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FTransform UFXR_InteractorComponent::GetTrackedTransform() const
{
	return GetComponentTransform();
}

FTransform UFXR_InteractorComponent::GetGripTransform() const
{
	const FTransform Source = GetTrackedTransform();
	return FTransform(Source.GetRotation(), Source.TransformPositionNoScale(GripLocalOffset));
}

FTransform UFXR_InteractorComponent::GetAimTransform() const
{
	const FTransform Source = GetTrackedTransform();
	if (!AimSource)
	{
		return Source;
	}

	// The aim component is authored by dragging it, but its *relative* transform is what counts: the
	// ray has to leave the hand wherever the hand actually is, and a tracked hand's pose comes from
	// joint data rather than from where a component sits in the rig. Scale is dropped because a ray
	// has no size — only an origin and a direction.
	FTransform Offset = AimSource->GetRelativeTransform();
	Offset.SetScale3D(FVector::OneVector);
	return Offset * Source;
}


FTransform UFXR_InteractorComponent::GetPalmTransform() const
{
	const FTransform Source = GetTrackedTransform();
	return FTransform(Source.GetRotation(), Source.TransformPositionNoScale(PalmLocalOffset));
}

void UFXR_InteractorComponent::GetGrabSphere(FVector& OutCenter, float& OutRadius) const
{
	OutCenter = GetTrackedTransform().TransformPositionNoScale(GrabSphereLocalOffset);
	OutRadius = GrabSphereRadius;
}

void UFXR_InteractorComponent::GetPokeTip(FVector& OutLocation, float& OutRadius) const
{
	OutLocation = GetTrackedTransform().TransformPositionNoScale(PokeLocalOffset);
	OutRadius = PokeRadius;
}

void UFXR_InteractorComponent::GetFarRay(FVector& OutOrigin, FVector& OutDirection) const
{
	const FTransform Aim = GetAimTransform();
	OutOrigin = Aim.GetLocation();
	OutDirection = Aim.GetUnitAxis(EAxis::X);
}
