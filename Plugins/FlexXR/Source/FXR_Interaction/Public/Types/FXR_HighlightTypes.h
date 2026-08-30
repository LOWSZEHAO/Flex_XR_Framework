// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_HighlightTypes.generated.h"

/**
 * What the highlight is *saying*, never how it looks. Training and games name states; the state→style
 * map in project settings decides the visual, so a whole product restyles from one place and no
 * gameplay code ever hardcodes a colour.
 */
UENUM(BlueprintType)
enum class EFXR_HighlightState : uint8
{
	None      UMETA(DisplayName = "None"),

	/** "You can interact with this." Driven automatically by the focus subsystem. */
	Hover     UMETA(DisplayName = "Hover"),

	/** "Interact with this NOW." Set deliberately by a training step or game script, and sticky until cleared. */
	Guidance  UMETA(DisplayName = "Guidance"),

	/** Acted on — held, or ray-selected. Driven automatically by the focus subsystem. */
	Selected  UMETA(DisplayName = "Selected")
};

/** How a highlighted object is drawn. Per-tier implementations sit behind these (design §9). */
UENUM(BlueprintType)
enum class EFXR_HighlightStyle : uint8
{
	None       UMETA(DisplayName = "None"),

	/** Silhouette edge glow. PCVR: custom depth + post-process. Quest: inverted-hull mesh. */
	Outline    UMETA(DisplayName = "Outline"),

	/** Whole-mesh emissive pulse, via the per-mesh overlay material slot. */
	InnerBlink UMETA(DisplayName = "Inner Blink"),

	/** Gradient band travelling across the object; direction is a material parameter. */
	Sweep      UMETA(DisplayName = "Sweep")
};

/** Which primitives light up when an interactable highlights. */
UENUM(BlueprintType)
enum class EFXR_HighlightScope : uint8
{
	/** Every primitive on the owning actor glows as one object — extinguisher plus pin, handle, hose. */
	Everything UMETA(DisplayName = "Everything"),

	/** Only the driven mesh: "look at this *part*", which is what SOP guidance on a safety pin needs. */
	TargetMesh UMETA(DisplayName = "Target Mesh")
};
