// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FXR_MotionSettings.generated.h"

/**
 * UFXR_MotionSettings — the motion-design spec, as settings rather than a document.
 *
 * Design §3.3 promises "one motion-design spec, enforced framework-wide". A document is not enforced
 * — and the framework proved it: highlights faded over 0.15 s, the far-ray beam over 0.12, the
 * guidance arrow over 0.20, each number picked in isolation by whoever wrote that system. Nobody
 * chose those differences; they simply accumulated. So the spec lives here and the components read
 * it, which is the only version of "framework-wide" that survives the next component being added.
 *
 * In FXR_Core, not FXR_UI where the design doc files it: FXR_Interaction, FXR_UI and FXR_Locomotion
 * all need these values, and Core is the only module beneath all three. Motion is a vocabulary, and
 * vocabulary belongs at the bottom.
 *
 * Deliberately not covered:
 * - **Travel** — a socket seating an object, a distance grab crossing a room. Those are movements
 *   with a distance to cover, not appearances; one shared duration would be wrong for both.
 * - **Teleport fade** — comfort, tuned per project and sometimes per player, and it stays on
 *   FXR_Locomotion where a designer looks for it.
 * - **Haptics** — one call site today. A spec governing a single caller is ceremony; it lands here
 *   when there is a second one to disagree with it.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FlexXR — Motion"))
class FXR_CORE_API UFXR_MotionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("FlexXR"); }

	/**
	 * How long anything takes to fade in or out: highlights, the socket ghost, the far-ray beam, the
	 * guidance arrow. One value, because these are one thing — something the framework is showing or
	 * withdrawing — and a player who sees two of them at once should not be able to tell they were
	 * written by different hands.
	 *
	 * Nothing pops. At zero this is a hard cut, which reads as a flicker in a headset where the hand
	 * is never quite still on the edge of a hover.
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
	 * Ease a 0..1 fade. SmoothStep rather than a linear ramp: an edge that swells in reads as
	 * arriving, where a linear one reads as a value being set. Named rather than inlined so the
	 * convention is greppable — three systems had already chosen three different curves.
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
