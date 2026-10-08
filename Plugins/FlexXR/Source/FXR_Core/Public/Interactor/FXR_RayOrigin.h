// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FXR_RayOrigin.generated.h"

/**
 * UFXR_RayOrigin — marks where a hand's far-interaction ray leaves it, and which way it points.
 *
 * Deliberately has no settings: its transform is the setting. Drag it in the viewport, or type
 * numbers into the transform panel, and the beam follows. AFXR_Pawn ships one per hand (Left Ray /
 * Right Ray) already wired to that hand's interactors.
 *
 * Its transform is read *relative to its parent* and composed onto the interactor's tracked pose —
 * never used as a world transform. A tracked hand's pose comes from joint data rather than from
 * where a component sits in the rig, so a component read as a world pose would simply not follow
 * the hand.
 *
 * A plain SceneComponent rather than an ArrowComponent, which would have drawn its own arrow for
 * free: Epic guards those behind WITH_EDITORONLY_DATA, so the offset would exist in the editor and
 * vanish from a packaged build. The arrow comes from a component visualizer instead.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_CORE_API UFXR_RayOrigin : public USceneComponent
{
	GENERATED_BODY()
};
