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
	DrawWireSphere(PDI, Transform.GetLocation(), FColor::Magenta, GripPoint->GetActivationRadius(), 16, SDPG_World);
}
