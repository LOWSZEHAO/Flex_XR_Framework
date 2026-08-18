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
	// A rail is drawn as its grabbable extent — a sphere at each end plus the spine between them —
	// so its length and orientation read at a glance. A point grip is just the one sphere.
	if (GripPoint->IsRail())
	{
		const FVector Axis = Transform.GetUnitAxis(EAxis::X);
		const FVector HalfSpan = Axis * (GripPoint->GetRailLength() * 0.5f);
		const FVector Start = Transform.GetLocation() - HalfSpan;
		const FVector End = Transform.GetLocation() + HalfSpan;

		DrawWireSphere(PDI, Start, FColor::Magenta, GripPoint->GetActivationRadius(), 12, SDPG_World);
		DrawWireSphere(PDI, End, FColor::Magenta, GripPoint->GetActivationRadius(), 12, SDPG_World);
		PDI->DrawLine(Start, End, FLinearColor(1.f, 0.f, 1.f), SDPG_World, 2.f);
	}
	else
	{
		DrawWireSphere(PDI, Transform.GetLocation(), FColor::Magenta, GripPoint->GetActivationRadius(), 16, SDPG_World);
	}
}
