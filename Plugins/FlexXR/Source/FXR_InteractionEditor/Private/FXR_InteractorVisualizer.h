// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * Viewport gizmo for UFXR_InteractorComponent (and its concrete subclasses — the visualizer lookup
 * walks up the class hierarchy). Draws the framework query shapes so their local offsets can be
 * tuned against the hand mesh at author time: the near grab sphere and the poke-tip probe that
 * drives FXR_Press.
 */
class FFXR_InteractorVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
