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

/** What happens to an in-progress hold when its interactable is disabled mid-grab (§4.0). */
UENUM(BlueprintType)
enum class EFXR_AlreadyHeldPolicy : uint8
{
	// Keep the current hold to its natural end; only block re-grabbing afterwards.
	FinishNaturally UMETA(DisplayName = "Finish Naturally"),
	// End the hold immediately — the object is ripped from the hand (disarm, stun, cutscene).
	ForceRelease    UMETA(DisplayName = "Force Release")
};

/** When a hand draws its far-interaction pointer beam. */
UENUM(BlueprintType)
enum class EFXR_RayVisibility : uint8
{
	// No beam. Far interaction still works — the hover highlight says what a press would take.
	Never    UMETA(DisplayName = "Never"),
	// Only while the hand is aimed at something that will answer. The beam becomes a statement
	// rather than a fixture, and it never points at nothing.
	OnTarget UMETA(DisplayName = "On Target"),
	// Whenever the hand is free. Reads as a menu pointer; useful for far UI-heavy scenes.
	Always   UMETA(DisplayName = "Always")
};

/** How much authoring debug an interactable draws (viewport gizmo and runtime). */
UENUM(BlueprintType)
enum class EFXR_DebugDraw : uint8
{
	Off   UMETA(DisplayName = "Off"),
	// The defining shape only — grab radius, latch arc, press face.
	Basic UMETA(DisplayName = "Basic (shape only)"),
	// Adds thresholds, limits, direction markers and live state.
	Full  UMETA(DisplayName = "Full (+ thresholds and live state)")
};

/** Which hand(s) a grip point accepts. */
UENUM(BlueprintType)
enum class EFXR_GripHandedness : uint8
{
	Both      UMETA(DisplayName = "Both"),
	LeftOnly  UMETA(DisplayName = "Left Only"),
	RightOnly UMETA(DisplayName = "Right Only")
};

/** How a grabbed object arrives at a grip point's pose. */
UENUM(BlueprintType)
enum class EFXR_GripSnapMode : uint8
{
	None    UMETA(DisplayName = "None (hold where grabbed)"),
	Snap    UMETA(DisplayName = "Snap (instant)"),
	Smooth  UMETA(DisplayName = "Smooth (interpolate)")
};

/** What a second hand does to a two-handed hold. */
UENUM(BlueprintType)
enum class EFXR_TwoHandMode : uint8
{
	// Both hands steer: the object's grip-to-grip axis follows the line between the hands, so
	// either hand can drive it — rifles, brooms, hoses, wheels.
	Shared  UMETA(DisplayName = "Shared (both hands steer)"),
	// The second hand attaches to its grip but the first hand alone poses the object — for props
	// where a second hand reorienting things would read as a glitch (crates, panels).
	Support UMETA(DisplayName = "Support (second hand does not steer)")
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

/** Constrained-motion primitive for FXR_Latch. */
UENUM(BlueprintType)
enum class EFXR_LatchMotion : uint8
{
	Rotational UMETA(DisplayName = "Rotational"),
	Linear     UMETA(DisplayName = "Linear")
};

/** Local axis of the latch component the motion is about (rotational) or along (linear). */
UENUM(BlueprintType)
enum class EFXR_LatchAxis : uint8
{
	X UMETA(DisplayName = "Local X"),
	Y UMETA(DisplayName = "Local Y"),
	Z UMETA(DisplayName = "Local Z")
};

/** When a socket shows its placement preview. */
UENUM(BlueprintType)
enum class EFXR_SocketGhostMode : uint8
{
	/** No preview at all. */
	Off        UMETA(DisplayName = "Off"),

	/**
	 * Only while an accepted object is carried within range. The preview answers "will this go
	 * here?" at the moment the question is being asked, and the world stays quiet otherwise.
	 */
	OnApproach UMETA(DisplayName = "On Approach"),

	/**
	 * Whenever the socket is enabled and empty. The mount advertises what belongs in it — right for
	 * a labelled bracket with an obviously missing extinguisher, and noisy anywhere else. Needs a
	 * Ghost Mesh, since nothing is being carried to borrow a shape from.
	 */
	Always     UMETA(DisplayName = "Always")
};
