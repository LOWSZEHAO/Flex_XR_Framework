// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Types/FXR_HighlightTypes.h"
#include "FXR_InteractionSettings.generated.h"

class UMaterialInterface;

/**
 * UFXR_InteractionSettings — project-wide interaction defaults, under Project Settings → FlexXR.
 *
 * The state→style map lives here rather than on interactables so a project restyles every highlight
 * from one place: FXR_Training only ever says "highlight the pin, Guidance state" and never learns
 * that Guidance currently means a yellow pulse.
 *
 * Per-module settings rather than one monolithic FXR_ProjectSettings: the modules are strictly
 * one-way, and a shared settings object low enough for all of them to read would have to carry
 * types from all of them.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FlexXR — Interaction"))
class FXR_INTERACTION_API UFXR_InteractionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFXR_InteractionSettings();

	virtual FName GetCategoryName() const override { return TEXT("FlexXR"); }

	/** The style each semantic state is drawn with. Editing this restyles every interactable in the project. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight")
	TMap<EFXR_HighlightState, EFXR_HighlightStyle> StateStyles;

	/**
	 * The colour each state is drawn in. Per state rather than one colour, because the Outline style
	 * is a single full-screen pass: it reads the state out of the stencil, so state is the only axis
	 * along which it can vary colour.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight")
	TMap<EFXR_HighlightState, FLinearColor> StateColors;

	/**
	 * Emissive multiplier for Inner Blink and Sweep, where not overridden. Kept at 1 by default:
	 * these draw unlit, so a higher value pushes the colour past white and every highlight looks the
	 * same regardless of the colour picked. The Outline pass has its own, below.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightIntensity = 1.f;

	/**
	 * How long a highlight takes to fade in or out. Nothing pops: an outline that appears at full
	 * strength on one frame and vanishes the next reads as a flicker, and in a headset the hand is
	 * never quite still on the edge of a hover.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float HighlightFadeTime = 0.15f;

	/**
	 * Interactables begin to glow as a hand approaches and reach full strength at grab range.
	 *
	 * The alternative — every interactable lit all the time — reads as a tutorial level rather than a
	 * product, and in a training sim it quietly removes the competency being tested: a trainee who
	 * never has to *find* the extinguisher has not been assessed on finding it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Proximity")
	bool bProximityHighlight = true;

	/** How far beyond grab reach the approach ramp begins. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Proximity", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bProximityHighlight"))
	float ProximityRange = 35.f;

	/**
	 * Strongest an approach glow gets before the hand is actually in reach. Held below 1 on purpose:
	 * the step up to full strength is what tells the player they can now take it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Proximity", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bProximityHighlight"))
	float ProximityMaxAlpha = 0.45f;

	/** Pulses per second for Inner Blink and Sweep, where not overridden. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightPulseRate = 1.5f;

	/**
	 * Emissive multiplier for the Outline pass. Higher than the overlay default on purpose: the band
	 * is thin and composited against the scene, so it needs headroom to read as a glow.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Outline", meta = (ClampMin = "0.0"))
	float OutlineIntensity = 3.f;

	/** Outline band width in pixels, held constant across render resolutions. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Outline", meta = (ClampMin = "0.5", ClampMax = "16.0"))
	float OutlineThickness = 2.f;

	/**
	 * Full-screen pass that draws the Outline style. Cleared to disable outlines entirely, or
	 * repointed at a project's own material — the stencil contract (1 Hover, 2 Guidance, 3 Selected)
	 * is all a replacement has to honour.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Outline", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UMaterialInterface> OutlineMaterial;

	/**
	 * Drawn per mesh through the overlay slot for the Inner Blink and Sweep styles. Cleared to
	 * disable both, or repointed at a project's own material.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight|Overlay", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UMaterialInterface> OverlayMaterial;

	/** The style for a state, falling back to None when the map has no entry. */
	EFXR_HighlightStyle GetStyleFor(EFXR_HighlightState State) const;

	/** The colour for a state, falling back to white when the map has no entry. */
	FLinearColor GetColorFor(EFXR_HighlightState State) const;

	static const UFXR_InteractionSettings* Get();
};
