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
	return GetTrackedTransform();
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
