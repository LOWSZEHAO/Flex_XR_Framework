// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_LatchVisualizer.h"
#include "Interactable/FXR_Latch.h"
#include "Interactable/FXR_GripPoint.h"
#include "Types/FXR_InteractionTypes.h"
#include "GameFramework/Actor.h"
#include "SceneManagement.h"

void FFXR_LatchVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_Latch* Latch = Cast<UFXR_Latch>(Component);
	if (!Latch || !Latch->IsDrawDebugEnabled())
	{
		return;
	}

	const FTransform ComponentXf = Latch->GetComponentTransform();
	const FVector Pivot = ComponentXf.GetLocation();
	const FVector AxisWorld = ComponentXf.GetRotation().RotateVector(Latch->GetMotionAxisLocalUnit()).GetSafeNormal();
	const FVector MeshLoc = Latch->GetInteractionLocation();
	const float Min = Latch->GetMinLimit();
	const float Max = Latch->GetMaxLimit();

	// Color code: white = rest (value 0), red = Min end, green = Max end; arrowheads point
	// from rest toward each limit so the travel direction is readable at a glance.
	const FLinearColor RangeColor(0.1f, 1.f, 0.2f);
	const FLinearColor MinColor(1.f, 0.15f, 0.15f);
	const FLinearColor MaxColor(0.15f, 1.f, 0.15f);

	// Small arrowhead at Tip pointing along Dir (Side = a unit vector perpendicular to Dir, in-plane).
	auto DrawArrowHead = [PDI](const FVector& Tip, const FVector& Dir, const FVector& Side, const FLinearColor& Color)
	{
		const FVector Back = Tip - Dir * 6.f;
		PDI->DrawLine(Tip, Back + Side * 3.f, Color, SDPG_World, 2.f);
		PDI->DrawLine(Tip, Back - Side * 3.f, Color, SDPG_World, 2.f);
	};

	const bool bFull = Latch->IsFullDebug();

	// Grab zone (registry detection): on the latch's grip points when it owns any (ADR-007 —
	// they are then the only grab surface), else the activation radius around the driven mesh.
	if (bFull)
	{
		bool bHasGripPoints = false;
		if (const AActor* OwnerActor = Latch->GetOwner())
		{
			TArray<UFXR_GripPoint*> GripPoints;
			OwnerActor->GetComponents<UFXR_GripPoint>(GripPoints);
			for (const UFXR_GripPoint* Point : GripPoints)
			{
				if (Point && Point->IsOwnedBy(Latch))
				{
					bHasGripPoints = true;
					DrawWireSphere(PDI, Point->GetComponentLocation(), FColor::Orange, Point->GetActivationRadius(), 12, SDPG_World);
				}
			}
		}
		if (!bHasGripPoints)
		{
			DrawWireSphere(PDI, MeshLoc, FColor::Orange, Latch->GetActivationRadius(), 16, SDPG_World);
		}
	}

	// Pivot + axis, drawn from the component so they track when the latch is moved or rotated.
	PDI->DrawPoint(Pivot, FLinearColor::White, 10.f, SDPG_World);
	PDI->DrawLine(Pivot - AxisWorld * 25.f, Pivot + AxisWorld * 25.f, FLinearColor(0.f, 1.f, 1.f), SDPG_World, 1.f);

	if (Latch->GetMotionType() == EFXR_LatchMotion::Linear)
	{
		// Travel range along the axis, from the mesh rest (value 0) to Min and Max.
		const FVector A = MeshLoc + AxisWorld * Min;
		const FVector B = MeshLoc + AxisWorld * Max;
		const FVector Side = FVector::CrossProduct(AxisWorld, FVector::UpVector).GetSafeNormal().IsNearlyZero()
			? FVector::CrossProduct(AxisWorld, FVector::RightVector).GetSafeNormal()
			: FVector::CrossProduct(AxisWorld, FVector::UpVector).GetSafeNormal();

		PDI->DrawLine(A, B, RangeColor, SDPG_World, 2.f);
		PDI->DrawPoint(MeshLoc, FLinearColor::White, 14.f, SDPG_World);
		PDI->DrawPoint(A, MinColor, 12.f, SDPG_World);
		PDI->DrawPoint(B, MaxColor, 12.f, SDPG_World);
		if (bFull && !FMath::IsNearlyZero(Min))
		{
			DrawArrowHead(A, -AxisWorld * FMath::Sign(-Min), Side, MinColor);
		}
		if (bFull && !FMath::IsNearlyZero(Max))
		{
			DrawArrowHead(B, AxisWorld * FMath::Sign(Max), Side, MaxColor);
		}
	}
	else
	{
		// Swing arc about the axis at the pivot, sweeping Min..Max. Angle 0 passes through the mesh rest.
		FVector RefDir = (MeshLoc - Pivot) - AxisWorld * FVector::DotProduct(MeshLoc - Pivot, AxisWorld);
		float Radius = RefDir.Size();
		if (Radius < 5.f)
		{
			// Mesh sits on the axis — fall back to a representative radius perpendicular to the axis.
			RefDir = ComponentXf.GetRotation().RotateVector(FVector::ForwardVector);
			RefDir = RefDir - AxisWorld * FVector::DotProduct(RefDir, AxisWorld);
			Radius = 20.f;
		}
		RefDir = RefDir.GetSafeNormal();

		const int32 Segments = 32;
		FVector Prev = FVector::ZeroVector;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float T = static_cast<float>(i) / static_cast<float>(Segments);
			const float AngleDeg = FMath::Lerp(Min, Max, T);
			const FQuat Rot(AxisWorld, FMath::DegreesToRadians(AngleDeg));
			const FVector P = Pivot + Rot.RotateVector(RefDir) * Radius;
			if (i > 0)
			{
				PDI->DrawLine(Prev, P, RangeColor, SDPG_World, 2.f);
			}
			Prev = P;
		}

		// Rest reference (value 0, through the mesh) in white; Min limit red, Max limit green.
		const FQuat RotMin(AxisWorld, FMath::DegreesToRadians(Min));
		const FQuat RotMax(AxisWorld, FMath::DegreesToRadians(Max));
		PDI->DrawLine(Pivot, Pivot + RefDir * Radius, FLinearColor::White, SDPG_World, 1.5f);
		PDI->DrawLine(Pivot, Pivot + RotMin.RotateVector(RefDir) * Radius, MinColor, SDPG_World, 1.5f);
		PDI->DrawLine(Pivot, Pivot + RotMax.RotateVector(RefDir) * Radius, MaxColor, SDPG_World, 1.5f);

		// Arrowheads at each limit, tangent to the arc, pointing away from rest — the swing direction.
		auto ArcTangent = [&AxisWorld](const FVector& RadialDir)
		{
			return FVector::CrossProduct(AxisWorld, RadialDir).GetSafeNormal();
		};
		if (!FMath::IsNearlyZero(Min))
		{
			const FVector RadialMin = RotMin.RotateVector(RefDir);
			DrawArrowHead(Pivot + RadialMin * Radius, ArcTangent(RadialMin) * FMath::Sign(Min), RadialMin, MinColor);
		}
		if (!FMath::IsNearlyZero(Max))
		{
			const FVector RadialMax = RotMax.RotateVector(RefDir);
			DrawArrowHead(Pivot + RadialMax * Radius, ArcTangent(RadialMax) * FMath::Sign(Max), RadialMax, MaxColor);
		}
	}
}
