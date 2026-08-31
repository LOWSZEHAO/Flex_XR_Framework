// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_RayOriginVisualizer.h"
#include "Interactor/FXR_RayOrigin.h"
#include "SceneManagement.h"

void FFXR_RayOriginVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_RayOrigin* Ray = Cast<UFXR_RayOrigin>(Component);
	if (!Ray)
	{
		return;
	}

	// Always drawn, with no enable flag: this component exists only to aim the beam, so selecting it
	// is already the request to see where the beam goes.
	//
	// A fixed preview length rather than the driver's reach — this is for aiming, and a 20 m line
	// laid across the level would only be in the way.
	constexpr float PreviewLength = 120.f;
	const FLinearColor BeamColor(0.45f, 0.8f, 1.f);

	DrawDirectionalArrow(PDI, Ray->GetComponentTransform().ToMatrixNoScale(), BeamColor, PreviewLength, 6.f, SDPG_Foreground, 1.5f);
	PDI->DrawPoint(Ray->GetComponentLocation(), BeamColor, 12.f, SDPG_Foreground);
}
