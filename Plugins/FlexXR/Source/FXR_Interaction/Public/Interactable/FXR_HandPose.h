// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_HandPose.generated.h"

/**
 * UFXR_HandPose — a reusable, skeleton-agnostic hand pose (design 5.3).
 *
 * Stores the *idea* of the pose (per-finger curls + thumb opposition), not bone rotations,
 * so one asset works on any hand mesh. A grip point references one to shape the hand for
 * the object; the per-skeleton Anim BP / Control Rig maps the curls onto that skeleton's
 * finger bones. Author "Power Grip", "Pinch", "Trigger Grip", "Flat Palm", etc.
 */
UCLASS(BlueprintType)
class FXR_INTERACTION_API UFXR_HandPose : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The pose's per-finger curls + thumb opposition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|HandPose")
	FFXR_FingerCurls Curls;
};
