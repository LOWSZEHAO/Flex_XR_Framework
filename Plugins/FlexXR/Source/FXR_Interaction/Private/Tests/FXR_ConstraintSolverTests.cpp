// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Solver/FXR_ConstraintSolver.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FFXR_ConstraintSolver is pure, stateless C++ (ADR-001) — these tests pin down its geometry
// and its determinism, the property SOP replay depends on.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SolverProjectToAxisTest,
	"FlexXR.Solver.ProjectToAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SolverProjectToAxisTest::RunTest(const FString& Parameters)
{
	// Height along a vertical axis is just the Z offset from the pivot.
	{
		const float Distance = FFXR_ConstraintSolver::ProjectToAxis(FVector::ZeroVector, FVector::UpVector, FVector(3.f, 4.f, 7.f));
		TestNearlyEqual(TEXT("Point above the pivot projects to its height"), Distance, 7.f, KINDA_SMALL_NUMBER);
	}

	// Below the pivot the distance is signed negative.
	{
		const float Distance = FFXR_ConstraintSolver::ProjectToAxis(FVector::ZeroVector, FVector::UpVector, FVector(1.f, -2.f, -5.f));
		TestNearlyEqual(TEXT("Point below the pivot projects negative"), Distance, -5.f, KINDA_SMALL_NUMBER);
	}

	// A pivot offset shifts the zero, not the scale.
	{
		const float Distance = FFXR_ConstraintSolver::ProjectToAxis(FVector(0.f, 0.f, 10.f), FVector::UpVector, FVector(0.f, 0.f, 17.f));
		TestNearlyEqual(TEXT("Pivot offset shifts the zero"), Distance, 7.f, KINDA_SMALL_NUMBER);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SolverProjectToRotationPlaneTest,
	"FlexXR.Solver.ProjectToRotationPlane",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SolverProjectToRotationPlaneTest::RunTest(const FString& Parameters)
{
	// The axis component is removed; the lever arm is the in-plane length (3-4-5 triangle).
	{
		float LeverArm = 0.f;
		const FVector InPlane = FFXR_ConstraintSolver::ProjectToRotationPlane(FVector::ZeroVector, FVector::UpVector, FVector(3.f, 4.f, 25.f), LeverArm);
		TestNearlyEqual(TEXT("Lever arm is the in-plane distance"), LeverArm, 5.f, KINDA_SMALL_NUMBER);
		TestNearlyEqual(TEXT("Projected Z is stripped"), static_cast<float>(InPlane.Z), 0.f, KINDA_SMALL_NUMBER);
	}

	// A hand exactly on the axis has no lever arm — the singularity the MinLeverArm guard rejects.
	{
		float LeverArm = -1.f;
		FFXR_ConstraintSolver::ProjectToRotationPlane(FVector::ZeroVector, FVector::UpVector, FVector(0.f, 0.f, 40.f), LeverArm);
		TestNearlyEqual(TEXT("On-axis hand yields a zero lever arm"), LeverArm, 0.f, KINDA_SMALL_NUMBER);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SolverSignedAngleTest,
	"FlexXR.Solver.SignedAngleAroundAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SolverSignedAngleTest::RunTest(const FString& Parameters)
{
	const FVector Axis = FVector::UpVector;

	// Right-hand rule around +Z: X toward Y is +90 degrees.
	TestNearlyEqual(TEXT("X to Y around Z is +90 deg"),
		FFXR_ConstraintSolver::SignedAngleAroundAxis(FVector::ForwardVector, FVector::RightVector, Axis), HALF_PI, KINDA_SMALL_NUMBER);

	// And the reverse swing is negative.
	TestNearlyEqual(TEXT("Y to X around Z is -90 deg"),
		FFXR_ConstraintSolver::SignedAngleAroundAxis(FVector::RightVector, FVector::ForwardVector, Axis), -HALF_PI, KINDA_SMALL_NUMBER);

	// No rotation reads zero.
	TestNearlyEqual(TEXT("Identical directions read zero"),
		FFXR_ConstraintSolver::SignedAngleAroundAxis(FVector::ForwardVector, FVector::ForwardVector, Axis), 0.f, KINDA_SMALL_NUMBER);

	// The result stays in [-PI, PI]: a half turn reads PI in magnitude.
	TestNearlyEqual(TEXT("Opposite directions read a half turn"),
		FMath::Abs(FFXR_ConstraintSolver::SignedAngleAroundAxis(FVector::ForwardVector, -FVector::ForwardVector, Axis)), PI, KINDA_SMALL_NUMBER);

	// Unnormalized inputs are accepted (the contract says "need not be normalized").
	TestNearlyEqual(TEXT("Magnitude does not skew the angle"),
		FFXR_ConstraintSolver::SignedAngleAroundAxis(FVector(10.f, 0.f, 0.f), FVector(0.f, 0.25f, 0.f), Axis), HALF_PI, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_SolverDeterminismTest,
	"FlexXR.Solver.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_SolverDeterminismTest::RunTest(const FString& Parameters)
{
	// Identical inputs must produce bitwise-identical outputs across repeated calls — the
	// deterministic-solver guarantee SOP replay is built on (ADR-001).
	const FVector Pivot(12.5f, -3.75f, 90.f);
	const FVector Axis = FVector(0.3f, -0.4f, 0.86f).GetSafeNormal();
	const FVector Hand(47.1f, 22.9f, 101.3f);
	const FVector From(0.9f, 0.1f, -0.2f);
	const FVector To(-0.4f, 0.8f, 0.3f);

	float FirstLeverArm = 0.f;
	const FVector FirstInPlane = FFXR_ConstraintSolver::ProjectToRotationPlane(Pivot, Axis, Hand, FirstLeverArm);
	const float FirstAngle = FFXR_ConstraintSolver::SignedAngleAroundAxis(From, To, Axis);
	const float FirstAxisProj = FFXR_ConstraintSolver::ProjectToAxis(Pivot, Axis, Hand);

	for (int32 Index = 0; Index < 100; ++Index)
	{
		float LeverArm = 0.f;
		const FVector InPlane = FFXR_ConstraintSolver::ProjectToRotationPlane(Pivot, Axis, Hand, LeverArm);
		if (InPlane != FirstInPlane || LeverArm != FirstLeverArm ||
			FFXR_ConstraintSolver::SignedAngleAroundAxis(From, To, Axis) != FirstAngle ||
			FFXR_ConstraintSolver::ProjectToAxis(Pivot, Axis, Hand) != FirstAxisProj)
		{
			AddError(FString::Printf(TEXT("Solver output diverged on repeat call %d — determinism broken."), Index));
			return false;
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
