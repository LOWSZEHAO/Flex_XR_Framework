// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_CoreTypes.generated.h"

/**
 * FlexXR shared enumerations (FXR_Core).
 *
 * Small, framework-wide enums grouped in one header by exception to the one-type-per-header
 * rule; substantial types (structs, interfaces, subsystems) each get their own header.
 */

/** Presentation mode for the FlexXR rig. MR is architecturally supported from day one; MR features land in a later phase. */
UENUM(BlueprintType)
enum class EFXR_Mode : uint8
{
	VR UMETA(DisplayName = "VR"),
	MR UMETA(DisplayName = "MR (Passthrough)")
};

/** Which hand an interactor, grip point, or event belongs to. */
UENUM(BlueprintType)
enum class EFXR_HandSide : uint8
{
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

/** The concrete input source behind an IFXR_Interactor. Interaction logic never branches on this — it exists for diagnostics and capability UI only. */
UENUM(BlueprintType)
enum class EFXR_InteractorType : uint8
{
	MotionController UMETA(DisplayName = "Motion Controller"),
	TrackedHand      UMETA(DisplayName = "Tracked Hand"),
	DesktopSim       UMETA(DisplayName = "Desktop Simulator")
};

/** Lifecycle phase of a broadcast interaction event. */
UENUM(BlueprintType)
enum class EFXR_InteractionPhase : uint8
{
	Began   UMETA(DisplayName = "Began"),
	Updated UMETA(DisplayName = "Updated"),
	Ended   UMETA(DisplayName = "Ended")
};

/** Override for which interactor set AFXR_Pawn activates. Auto decides from device capabilities; the Force modes aid testing. */
UENUM(BlueprintType)
enum class EFXR_InteractorPreference : uint8
{
	Auto             UMETA(DisplayName = "Auto (by capabilities)"),
	ForceControllers UMETA(DisplayName = "Force Controllers"),
	ForceHands       UMETA(DisplayName = "Force Tracked Hands"),
	ForceDesktop     UMETA(DisplayName = "Force Desktop Sim")
};
