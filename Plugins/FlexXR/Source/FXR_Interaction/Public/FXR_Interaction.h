// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Interaction — the heart of FlexXR.
 *
 * The user-facing interaction components (Grab, Latch, Press, Socket, Use, GripPoint,
 * RayTarget, Highlight), the FFXR_ConstraintSolver, the hand-presentation pipeline,
 * and the registry-based detection & focus pipeline. Depends on FXR_Core and knows
 * nothing about FXR_UI or FXR_Training.
 */
class FFXR_InteractionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
