// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_TrainingEditor — editor-only module hosting FlexXR's training authoring tools.
 *
 * Holds UFXR_PanelBuilder, which generates the default panel, and registers the details
 * customization that turns a GM event's name into a dropdown of the chosen Blueprint's events. The
 * module exists so that code which needs UMGEditor, UnrealEd and PropertyEditor cannot be linked
 * into a shipped runtime by accident.
 */
class FFXR_TrainingEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
