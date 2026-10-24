// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Interaction - the interaction layer.
 *
 * The interaction components (Grab, Latch, Press, Socket, GripPoint, RayTarget, Highlight), the
 * constraint solver, the hand visual, and registry-based detection and focus. Depends only on
 * FXR_Core; doesn't know about FXR_UI, FXR_Locomotion or FXR_Training.
 */
class FFXR_InteractionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
