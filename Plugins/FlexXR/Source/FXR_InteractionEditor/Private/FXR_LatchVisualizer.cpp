// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_LatchVisualizer.h"
#include "Interactable/FXR_Latch.h"
#include "Types/FXR_InteractionTypes.h"
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

	const FLinearColor RangeColor(0.1f, 1.f, 0.2f);

	// Grab zone (registry detection) — centered on the driven mesh, not the pivot.
	DrawWireSphere(PDI, MeshLoc, FColor::Orange, Latch->GetActivationRadius(), 16, SDPG_World);

	// Pivot + axis, drawn from the component so they track when the latch is moved or rotated.
	PDI->DrawPoint(Pivot, FLinearColor::White, 10.f, SDPG_World);
	PDI->DrawLine(Pivot - AxisWorld * 25.f, Pivot + AxisWorld * 25.f, FLinearColor(0.f, 1.f, 1.f), SDPG_World, 1.f);

	if (Latch->GetMotionType() == EFXR_LatchMotion::Linear)
	{
		// Travel range along the axis, from the mesh rest (value 0) to Min and Max.
		const FVector A = MeshLoc + AxisWorld * Min;
		const FVector B = MeshLoc + AxisWorld * Max;
		PDI->DrawLine(A, B, RangeColor, SDPG_World, 2.f);
		PDI->DrawPoint(A, RangeColor, 12.f, SDPG_World);
		PDI->DrawPoint(B, RangeColor, 12.f, SDPG_World);
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

		// Radius lines at the two limits.
		const FQuat RotMin(AxisWorld, FMath::DegreesToRadians(Min));
		const FQuat RotMax(AxisWorld, FMath::DegreesToRadians(Max));
		PDI->DrawLine(Pivot, Pivot + RotMin.RotateVector(RefDir) * Radius, RangeColor, SDPG_World, 1.5f);
		PDI->DrawLine(Pivot, Pivot + RotMax.RotateVector(RefDir) * Radius, RangeColor, SDPG_World, 1.5f);
	}
}
