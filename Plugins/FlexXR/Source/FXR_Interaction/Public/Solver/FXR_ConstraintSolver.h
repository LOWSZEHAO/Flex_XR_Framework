// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FFXR_ConstraintSolver — pure geometric projection for constrained motion (ADR-001, design 5.1).
 *
 * Plain C++: no UObject, no world, no state. Deterministic and unit-testable in isolation.
 * FXR_Latch uses it to project the hand onto a rotational or linear constraint manifold.
 * All axis parameters must be unit length.
 */
struct FXR_INTERACTION_API FFXR_ConstraintSolver
{
	/**
	 * Hand offset from the pivot projected onto the plane perpendicular to Axis (through the pivot).
	 * OutLeverArm receives the projected length (the rotational lever arm) — callers guard the
	 * axis singularity by rejecting values below a minimum.
	 */
	static FVector ProjectToRotationPlane(const FVector& PivotLocation, const FVector& Axis, const FVector& HandLocation, float& OutLeverArm);

	/**
	 * Signed angle in radians, about Axis, from FromDir to ToDir (both in the plane perpendicular
	 * to Axis; need not be normalized). Result in [-PI, PI].
	 */
	static float SignedAngleAroundAxis(const FVector& FromDir, const FVector& ToDir, const FVector& Axis);

	/** Signed distance of the hand along Axis from the pivot. */
	static float ProjectToAxis(const FVector& PivotLocation, const FVector& Axis, const FVector& HandLocation);
};
