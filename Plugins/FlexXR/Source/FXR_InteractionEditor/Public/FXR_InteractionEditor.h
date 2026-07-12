// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_InteractionEditor — editor-only module hosting FlexXR's viewport gizmos.
 *
 * Registers component visualizers so grip points (axes + radius) and interactable
 * activation radii are visible in the level viewport at author time — not just in play.
 */
class FFXR_InteractionEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
