// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * Viewport gizmo for UFXR_Latch. Drawn from the latch component's own transform — pivot marker,
 * motion axis, and the Min..Max travel range (a line for Linear, a swing arc for Rotational) — so
 * it tracks the component when you move or rotate it. Also shows the grab activation sphere.
 */
class FFXR_LatchVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
