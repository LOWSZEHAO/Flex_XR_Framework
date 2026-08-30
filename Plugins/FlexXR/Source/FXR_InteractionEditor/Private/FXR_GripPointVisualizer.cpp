// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_GripPointVisualizer.h"
#include "Interactable/FXR_GripPoint.h"
#include "SceneManagement.h"

void FFXR_GripPointVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_GripPoint* GripPoint = Cast<UFXR_GripPoint>(Component);
	if (!GripPoint || !GripPoint->IsDrawDebugEnabled())
	{
		return;
	}

	const FTransform Transform = GripPoint->GetComponentTransform();

	// Grip pose axes (the "shaped like this" orientation) + activation radius.
	DrawCoordinateSystem(PDI, Transform.GetLocation(), Transform.Rotator(), 5.f, SDPG_Foreground);
	const float Radius = GripPoint->GetActivationRadius();
	if (GripPoint->IsRail())
	{
		// One capsule, not two spheres: a hand may take the rail anywhere inside this volume, so
		// drawing only the end caps would wrongly read as two separate grab spots.
		const FVector RailAxis = Transform.GetUnitAxis(EAxis::X);
		const float HalfHeight = GripPoint->GetRailLength() * 0.5f + Radius;

		DrawWireCapsule(PDI, Transform.GetLocation(),
			Transform.GetUnitAxis(EAxis::Y), Transform.GetUnitAxis(EAxis::Z), RailAxis,
			FLinearColor(1.f, 0.f, 1.f), Radius, HalfHeight, 16, SDPG_World);
	}
	else
	{
		DrawWireSphere(PDI, Transform.GetLocation(), FColor::Magenta, Radius, 16, SDPG_World);
	}
}
