// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_InteractionTypes.generated.h"

/** Why an interaction ended — passed to UFXR_InteractableBase::OnEnd. */
UENUM(BlueprintType)
enum class EFXR_EndReason : uint8
{
	Released      UMETA(DisplayName = "Released"),
	ForceReleased UMETA(DisplayName = "Force Released"),
	Disabled      UMETA(DisplayName = "Disabled"),
	Broken        UMETA(DisplayName = "Broken")
};

/** Which hand(s) a grip point accepts. */
UENUM(BlueprintType)
enum class EFXR_GripHandedness : uint8
{
	Both      UMETA(DisplayName = "Both"),
	LeftOnly  UMETA(DisplayName = "Left Only"),
	RightOnly UMETA(DisplayName = "Right Only")
};

/**
 * Skeleton-agnostic finger pose: per-finger curl (0 = open, 1 = fully curled) + thumb
 * opposition. Not bone rotations — a hand Anim BP / Control Rig maps these to the active
 * skeleton's finger bones (the per-skeleton "retarget profile"), so one pose works on any mesh.
 */
USTRUCT(BlueprintType)
struct FFXR_FingerCurls
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Thumb = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Index = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Middle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Ring = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Pinky = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Hand", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ThumbOpposition = 0.f;
};
