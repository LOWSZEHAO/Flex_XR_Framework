// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Training — the optional SOP layer.
 *
 * A data-driven step graph that *watches* InteractionId events from the FXR event bus
 * and validates them: scoring, mistake analytics, session reports, and replay. The top
 * of the dependency chain — games simply never load it. (Runtime per ADR-004:
 * a custom FFXR_StepRunner over a compiled step array, authored via UFXR_StepGraph.)
 */
class FFXR_TrainingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
