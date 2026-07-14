// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Solver/FXR_ConstraintSolver.h"

FVector FFXR_ConstraintSolver::ProjectToRotationPlane(const FVector& PivotLocation, const FVector& Axis, const FVector& HandLocation, float& OutLeverArm)
{
	const FVector ToHand = HandLocation - PivotLocation;
	const FVector InPlane = ToHand - Axis * FVector::DotProduct(ToHand, Axis);
	OutLeverArm = InPlane.Size();
	return InPlane;
}

float FFXR_ConstraintSolver::SignedAngleAroundAxis(const FVector& FromDir, const FVector& ToDir, const FVector& Axis)
{
	const FVector From = FromDir.GetSafeNormal();
	const FVector To = ToDir.GetSafeNormal();
	const float Cross = FVector::DotProduct(FVector::CrossProduct(From, To), Axis);
	const float Dot = FVector::DotProduct(From, To);
	return FMath::Atan2(Cross, Dot);
}

float FFXR_ConstraintSolver::ProjectToAxis(const FVector& PivotLocation, const FVector& Axis, const FVector& HandLocation)
{
	return FVector::DotProduct(HandLocation - PivotLocation, Axis);
}
