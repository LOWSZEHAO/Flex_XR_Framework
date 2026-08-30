// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FXR_Locomotion — moving the player.
 *
 * One pawn component (UFXR_Locomotion, ADR-005) arbitrating teleport, smooth move, turn, and
 * climbing, plus FXR_TeleportAnchor / FXR_TeleportBlocker world components. A sibling of FXR_UI,
 * above FXR_Interaction: it reuses FXR_Grab (climbing) and yields to any hand owning an
 * interaction. Never referenced by FXR_Interaction or below.
 */
class FFXR_LocomotionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
