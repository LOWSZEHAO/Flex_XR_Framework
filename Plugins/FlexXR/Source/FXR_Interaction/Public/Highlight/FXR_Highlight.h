// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/FXR_HighlightTypes.h"
#include "FXR_Highlight.generated.h"

/**
 * UFXR_Highlight — optional. Every interactable already highlights from project settings with no
 * setup at all; this exists only to customise one object (design principle 2: components customise,
 * they never switch basics on).
 *
 * Deliberately not fields on UFXR_InteractableBase: putting Style/Colour on the base would grow
 * every Grab, Latch, Press and Socket panel with fields they do not need, and would give colour two
 * homes that can disagree. Adding this component is the opt-in.
 *
 * Add it to the same actor as the interactable it customises.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Highlight : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_Highlight();

	/** The style for a state on this object: an override here, else the project default. */
	EFXR_HighlightStyle ResolveStyle(EFXR_HighlightState State) const;

	/** The colour for a state on this object: an override here, else the project's colour for that state. */
	FLinearColor ResolveColor(EFXR_HighlightState State) const;
	float ResolveIntensity() const;
	float ResolvePulseRate() const;

	EFXR_HighlightScope GetScope() const { return Scope; }

	/** Direction the Sweep band travels, in world space. */
	FVector GetSweepDirection() const { return SweepDirection; }

protected:
	/**
	 * Which primitives light up. Everything glows the object as a whole; Target Mesh glows only the
	 * driven mesh, which is what "pull *this* pin" needs when the pin is one part of an extinguisher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	EFXR_HighlightScope Scope = EFXR_HighlightScope::Everything;

	/** Per-state style overrides. States absent here fall through to project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	TMap<EFXR_HighlightState, EFXR_HighlightStyle> StyleOverrides;

	/**
	 * Tick to art-direct this object's colour; otherwise it follows the project.
	 * Applies to Inner Blink and Sweep, which draw per mesh. The Outline style is one full-screen
	 * pass shared by every outlined object, so it can only vary by state, not per object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight", meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor(1.f, 0.85f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	bool bOverrideIntensity = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight", meta = (ClampMin = "0.0", EditCondition = "bOverrideIntensity"))
	float Intensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	bool bOverridePulseRate = false;

	/** Pulses per second for Inner Blink and Sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight", meta = (ClampMin = "0.0", EditCondition = "bOverridePulseRate"))
	float PulseRate = 1.5f;

	/**
	 * Direction the Sweep band travels. Up suits an upright object being scanned; point it along the
	 * axis a part actually moves for "pull this lever" reads.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Highlight")
	FVector SweepDirection = FVector::UpVector;
};
