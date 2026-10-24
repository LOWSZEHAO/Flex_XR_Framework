// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FXR_MotionSettings.generated.h"

/**
 * UFXR_MotionSettings - shared animation timings (fades) for the whole framework.
 *
 * Before this, every system picked its own fade time: highlights 0.15 s, the ray beam 0.12 s,
 * the guidance arrow 0.20 s. Putting it in project settings and having the components read it
 * keeps them consistent as new ones get added.
 *
 * In FXR_Core because FXR_Interaction, FXR_UI and FXR_Locomotion all use it.
 *
 * Not included:
 * - Travel animations (socket seating, distance grab): they cover a distance, so one shared
 *   duration doesn't fit.
 * - Teleport fade: a comfort setting, tuned per project, so it stays on FXR_Locomotion.
 * - Haptics: only one call site so far.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FlexXR - Motion"))
class FXR_CORE_API UFXR_MotionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("FlexXR"); }

	/**
	 * Fade in/out duration for highlights, the socket ghost, the ray beam and the guidance arrow.
	 * One value so they all feel the same. At 0 it's a hard cut, which flickers in a headset when
	 * the hand hovers on the edge of an object.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float FadeDuration = 0.15f;

	static const UFXR_MotionSettings* Get();

	/** Fade duration from settings, or the class default if settings are somehow unavailable. */
	static float GetFadeDuration();
};

/** The framework's easing, in one place so new code inherits the curve instead of picking one. */
struct FFXR_Motion
{
	/**
	 * Easing for a 0..1 fade (SmoothStep, so fades ease in and out instead of ramping linearly).
	 * A named function so every system uses the same curve.
	 */
	static float EaseFade(float Alpha)
	{
		return FMath::SmoothStep(0.f, 1.f, Alpha);
	}

	/** Per-frame step for a constant-rate fade over Duration. */
	static float FadeStep(float DeltaTime, float Duration)
	{
		return (Duration > KINDA_SMALL_NUMBER) ? (DeltaTime / Duration) : 1.f;
	}
};
