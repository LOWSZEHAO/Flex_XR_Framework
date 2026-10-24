// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_TrainingEditor - editor-only tools for FXR_Training: the panel builder and the GM details
 * customizations. Separate module so UMGEditor / UnrealEd / PropertyEditor never end up in a
 * runtime build.
 */
class FFXR_TrainingEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
