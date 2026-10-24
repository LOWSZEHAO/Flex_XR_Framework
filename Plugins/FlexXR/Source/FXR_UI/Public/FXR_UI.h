// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_UI - spatial UI and presentation: world-space panels and the hand pointer for them,
 * focus/ray targeting, highlight rendering and the guidance arrow. Depends on FXR_Interaction,
 * doesn't know about FXR_Training.
 */
class FFXR_UIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
