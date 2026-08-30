// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Types/FXR_HighlightTypes.h"
#include "FXR_InteractionSettings.generated.h"

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

	/** Highlight colour for interactables that do not override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight")
	FLinearColor HighlightColor = FLinearColor(1.f, 0.85f, 0.1f, 1.f);

	/** Emissive multiplier for interactables that do not override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightIntensity = 1.f;

	/** Pulses per second for Inner Blink and Sweep, where not overridden. */
	UPROPERTY(Config, EditAnywhere, Category = "Highlight", meta = (ClampMin = "0.0"))
	float HighlightPulseRate = 1.5f;

	/** The style for a state, falling back to None when the map has no entry. */
	EFXR_HighlightStyle GetStyleFor(EFXR_HighlightState State) const;

	static const UFXR_InteractionSettings* Get();
};
