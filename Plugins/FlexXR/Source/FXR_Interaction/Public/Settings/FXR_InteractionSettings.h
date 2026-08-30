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

	/** Emissive multiplier for interactables that do not override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightIntensity = 2.f;

	/** Pulses per second for Inner Blink and Sweep, where not overridden. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightPulseRate = 1.5f;

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

	/** The style for a state, falling back to None when the map has no entry. */
	EFXR_HighlightStyle GetStyleFor(EFXR_HighlightState State) const;

	/** The colour for a state, falling back to white when the map has no entry. */
	FLinearColor GetColorFor(EFXR_HighlightState State) const;

	static const UFXR_InteractionSettings* Get();
};
