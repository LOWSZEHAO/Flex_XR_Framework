// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/** Draws a UFXR_InteractableBase's activation radius in the level viewport (selected actor). */
class FFXR_InteractableVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
