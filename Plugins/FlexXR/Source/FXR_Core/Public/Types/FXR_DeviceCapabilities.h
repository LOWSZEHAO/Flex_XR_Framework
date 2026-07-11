// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_DeviceCapabilities.generated.h"

/**
 * Runtime XR device capabilities, resolved once at startup (and on device changes) by
 * UFXR_XRSubsystem. Interaction code reads this to choose the active interactor set;
 * it never queries the OpenXR runtime directly.
 */
USTRUCT(BlueprintType)
struct FFXR_DeviceCapabilities
{
	GENERATED_BODY()

	/** Motion controllers are connected and tracking. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|XR")
	bool bHasMotionControllers = false;

	/** Articulated hand tracking is available (e.g. OpenXR XR_EXT_hand_tracking). */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|XR")
	bool bHasHandTracking = false;

	/** Running on a standalone HMD (e.g. Meta Quest) rather than PCVR. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|XR")
	bool bIsStandalone = false;

	/** The active runtime can composite camera passthrough for MR. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|XR")
	bool bSupportsPassthrough = false;
};
