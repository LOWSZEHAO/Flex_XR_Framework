// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_FocusTypes.generated.h"

/**
 * What a hand is paying attention to, strongest first. Deliberately *semantic*: components declare
 * a state and never a visual, so the state→style map in project settings can restyle a whole product
 * without gameplay or training code naming a colour.
 */
UENUM(BlueprintType)
enum class EFXR_FocusState : uint8
{
	/** Nothing — out of reach and not pointed at. */
	None      UMETA(DisplayName = "None"),

	/** In reach: squeezing right now would take it. Drives the default hover highlight. */
	Hovered   UMETA(DisplayName = "Hovered"),

	/** Pointed at from range down the far ray. Weaker than Hovered — a hand in reach is not also pointing. */
	Focused   UMETA(DisplayName = "Focused"),

	/** Acted on — held, or ray-selected. */
	Selected  UMETA(DisplayName = "Selected")
};
