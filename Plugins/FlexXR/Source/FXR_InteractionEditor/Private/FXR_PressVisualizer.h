// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * Viewport gizmo for UFXR_Press. Drawn from the press component's own transform — the pressable
 * face disc (+Z outward), the travel depth, and the activation (click) depth — so it tracks the
 * component as it is placed on the button cap.
 */
class FFXR_PressVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
