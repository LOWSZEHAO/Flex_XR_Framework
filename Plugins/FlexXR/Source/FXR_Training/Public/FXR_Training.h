// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Training — the optional SOP layer.
 *
 * A procedure authored as two assets - the steps the trainee reads (UFXR_TrainingSteps) and the
 * GM that says what happens at each (UFXR_TrainingGM) - run in Training or Exam by a session that
 * fires the GM's events, judges InteractionId events from the FXR event bus, and reports. The top of
 * the dependency chain; games simply never load it. Runtime per ADR-004 and ADR-011: an engine-free
 * FFXR_StepRunner over a compiled step array.
 */
class FFXR_TrainingModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
