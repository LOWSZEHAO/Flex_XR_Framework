// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Settings/FXR_InteractionSettings.h"
#include "Engine/World.h"

UFXR_InteractionSettings::UFXR_InteractionSettings()
{
	// The defaults from design §4: hover reads as "you can touch this", guidance demands attention,
	// selection confirms. A project may remap all three without touching gameplay code.
	StateStyles.Add(EFXR_HighlightState::Hover, EFXR_HighlightStyle::Outline);
	StateStyles.Add(EFXR_HighlightState::Guidance, EFXR_HighlightStyle::InnerBlink);
	StateStyles.Add(EFXR_HighlightState::Selected, EFXR_HighlightStyle::Sweep);

	// Neutral for hover so it reads as affordance rather than instruction; amber for guidance because
	// it has to win attention; green for selected to confirm.
	StateColors.Add(EFXR_HighlightState::Hover, FLinearColor(1.f, 1.f, 1.f, 1.f));
	StateColors.Add(EFXR_HighlightState::Guidance, FLinearColor(1.f, 0.75f, 0.15f, 1.f));
	StateColors.Add(EFXR_HighlightState::Selected, FLinearColor(0.2f, 1.f, 0.4f, 1.f));

	OutlineMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_Outline.M_FXR_Outline")));
	OverlayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_HighlightOverlay.M_FXR_HighlightOverlay")));
	OutlineHullMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_OutlineHull.M_FXR_OutlineHull")));
}

EFXR_HighlightStyle UFXR_InteractionSettings::GetStyleFor(EFXR_HighlightState State) const
{
	const EFXR_HighlightStyle* Found = StateStyles.Find(State);
	return Found ? *Found : EFXR_HighlightStyle::None;
}

FLinearColor UFXR_InteractionSettings::GetColorFor(EFXR_HighlightState State) const
{
	const FLinearColor* Found = StateColors.Find(State);
	return Found ? *Found : FLinearColor::White;
}

EFXR_HighlightTier UFXR_InteractionSettings::ResolveTier(const UWorld* World) const
{
	if (HighlightTier != EFXR_HighlightTier::Auto)
	{
		return HighlightTier;
	}

	// Feature level rather than platform: it is the thing that actually decides whether the outline
	// pass is affordable, and it follows the editor's mobile preview — so the Quest path can be
	// looked at on a desktop instead of discovered on device.
	const bool bMobile = World && World->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1;
	return bMobile ? EFXR_HighlightTier::MeshHull : EFXR_HighlightTier::PostProcess;
}

const UFXR_InteractionSettings* UFXR_InteractionSettings::Get()
{
	return GetDefault<UFXR_InteractionSettings>();
}
