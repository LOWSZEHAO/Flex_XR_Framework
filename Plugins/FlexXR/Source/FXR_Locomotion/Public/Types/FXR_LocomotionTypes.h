// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_LocomotionTypes.generated.h"

/** How the view transitions across a committed teleport. */
UENUM(BlueprintType)
enum class EFXR_TeleportTransition : uint8
{
	Fade    UMETA(DisplayName = "Fade"),
	Blink   UMETA(DisplayName = "Blink"),
	Dash    UMETA(DisplayName = "Dash"),
	Instant UMETA(DisplayName = "Instant")
};

/** Teleport aim shape. */
UENUM(BlueprintType)
enum class EFXR_TeleportAim : uint8
{
	ProjectileArc UMETA(DisplayName = "Projectile Arc"),
	StraightRay   UMETA(DisplayName = "Straight Ray")
};

/** How a teleport destination is judged valid. */
UENUM(BlueprintType)
enum class EFXR_TeleportValidation : uint8
{
	NavMesh       UMETA(DisplayName = "NavMesh"),
	SurfaceAngle  UMETA(DisplayName = "Surface Angle"),
	AnchorsOnly   UMETA(DisplayName = "Anchors Only"),
	CustomChannel UMETA(DisplayName = "Custom Channel")
};

/** Facing applied on landing (rotated about the HMD, not the pawn root — ADR-006). */
UENUM(BlueprintType)
enum class EFXR_LandingRotation : uint8
{
	KeepFacing       UMETA(DisplayName = "Keep Facing"),
	ThumbstickChoose UMETA(DisplayName = "Thumbstick Choose"),
	FaceArc          UMETA(DisplayName = "Face Arc")
};

/**
 * What one hand's stick forward axis does. Teleport and smooth move both claim it, so a hand can
 * only have one of them — choosing per hand rather than per mode makes that clash impossible to
 * express instead of merely warned about.
 */
UENUM(BlueprintType)
enum class EFXR_HandMovement : uint8
{
	None       UMETA(DisplayName = "None"),
	Teleport   UMETA(DisplayName = "Teleport"),
	SmoothMove UMETA(DisplayName = "Smooth Move")
};

/** What one hand's stick sideways axis does. None frees it to strafe, if that hand smooth-moves. */
UENUM(BlueprintType)
enum class EFXR_TurnMode : uint8
{
	None   UMETA(DisplayName = "None"),
	Snap   UMETA(DisplayName = "Snap"),
	Smooth UMETA(DisplayName = "Smooth")
};

/** Comfort vignette behaviour. */
UENUM(BlueprintType)
enum class EFXR_VignetteMode : uint8
{
	Off     UMETA(DisplayName = "Off"),
	Dynamic UMETA(DisplayName = "Dynamic"),
	Always  UMETA(DisplayName = "Always")
};

/** Frame smooth movement is relative to. */
UENUM(BlueprintType)
enum class EFXR_MoveDirectionSource : uint8
{
	HeadRelative UMETA(DisplayName = "Head Relative"),
	HandRelative UMETA(DisplayName = "Hand Relative"),
	HipRelative  UMETA(DisplayName = "Hip Relative")
};
