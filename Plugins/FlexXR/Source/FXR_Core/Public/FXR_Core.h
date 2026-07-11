// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Core — FlexXR platform layer.
 *
 * The lowest module in the FlexXR dependency chain
 * (FXR_Training -> FXR_UI -> FXR_Interaction -> FXR_Core). Home of the OpenXR
 * abstraction, runtime device-capability detection, the IFXR_Interactor input
 * interface, input mapping, the EFXR_Mode (VR / MR) flags, and the FXR event bus.
 * References nothing above it.
 */
class FFXR_CoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
