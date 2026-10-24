// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Training - optional SOP / training layer.
 *
 * A procedure is two assets: the steps (UFXR_TrainingSteps) and the GM (UFXR_TrainingGM). A
 * session runs it in Training or Exam mode, fires the GM's events, judges interaction events from
 * the event bus and builds a report. Top of the module chain; games don't need to load it.
 * See ADR-004 and ADR-011.
 */
class FFXR_TrainingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
