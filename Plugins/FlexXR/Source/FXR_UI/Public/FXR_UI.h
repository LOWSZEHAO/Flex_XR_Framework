// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_UI — spatial UI and presentation.
 *
 * The spatial UI kit (panels, buttons, sliders, keypads), the single framework-wide
 * motion-design spec, the ray-targeting & focus manager, per-tier highlight rendering,
 * and diegetic guidance primitives. Depends on FXR_Interaction; unaware of FXR_Training.
 */
class FFXR_UIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
