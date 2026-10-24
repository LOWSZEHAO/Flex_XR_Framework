// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Core - FlexXR platform layer, the bottom of the module chain
 * (FXR_Training -> FXR_UI / FXR_Locomotion -> FXR_Interaction -> FXR_Core).
 *
 * OpenXR abstraction, device-capability detection, the IFXR_Interactor input interface, input
 * mapping, the VR / MR mode flags and the event bus. Doesn't reference anything above it.
 */
class FFXR_CoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
