// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Settings/FXR_InteractionSettings.h"

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

const UFXR_InteractionSettings* UFXR_InteractionSettings::Get()
{
	return GetDefault<UFXR_InteractionSettings>();
}
