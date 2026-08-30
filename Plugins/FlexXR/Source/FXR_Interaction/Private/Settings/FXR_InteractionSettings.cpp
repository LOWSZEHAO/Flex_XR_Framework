// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Settings/FXR_InteractionSettings.h"

UFXR_InteractionSettings::UFXR_InteractionSettings()
{
	// The defaults from design §4: hover reads as "you can touch this", guidance demands attention,
	// selection confirms. A project may remap all three without touching gameplay code.
	StateStyles.Add(EFXR_HighlightState::Hover, EFXR_HighlightStyle::Outline);
	StateStyles.Add(EFXR_HighlightState::Guidance, EFXR_HighlightStyle::InnerBlink);
	StateStyles.Add(EFXR_HighlightState::Selected, EFXR_HighlightStyle::Sweep);
}

EFXR_HighlightStyle UFXR_InteractionSettings::GetStyleFor(EFXR_HighlightState State) const
{
	const EFXR_HighlightStyle* Found = StateStyles.Find(State);
	return Found ? *Found : EFXR_HighlightStyle::None;
}

const UFXR_InteractionSettings* UFXR_InteractionSettings::Get()
{
	return GetDefault<UFXR_InteractionSettings>();
}
