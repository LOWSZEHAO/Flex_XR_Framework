// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_TrainingEditor — editor-only module hosting FlexXR's training authoring tools.
 *
 * Registers nothing on startup today: its contents are reached through UFXR_PanelBuilder, which is
 * a function library rather than a registered customization. The module exists so that code which
 * needs UMGEditor and UnrealEd cannot be linked into a shipped runtime by accident.
 */
class FFXR_TrainingEditorModule : public IModuleInterface
{
};
