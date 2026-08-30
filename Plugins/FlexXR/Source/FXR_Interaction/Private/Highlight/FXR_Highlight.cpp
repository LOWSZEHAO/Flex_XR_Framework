// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Highlight/FXR_Highlight.h"
#include "Settings/FXR_InteractionSettings.h"

UFXR_Highlight::UFXR_Highlight()
{
	// Pure configuration read by the highlight subsystem — nothing to tick.
	PrimaryComponentTick.bCanEverTick = false;
}

EFXR_HighlightStyle UFXR_Highlight::ResolveStyle(EFXR_HighlightState State) const
{
	if (const EFXR_HighlightStyle* Override = StyleOverrides.Find(State))
	{
		return *Override;
	}

	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	return Settings ? Settings->GetStyleFor(State) : EFXR_HighlightStyle::None;
}

FLinearColor UFXR_Highlight::ResolveColor() const
{
	if (bOverrideColor)
	{
		return Color;
	}
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	return Settings ? Settings->HighlightColor : FLinearColor::Yellow;
}

float UFXR_Highlight::ResolveIntensity() const
{
	if (bOverrideIntensity)
	{
		return Intensity;
	}
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	return Settings ? Settings->HighlightIntensity : 1.f;
}

float UFXR_Highlight::ResolvePulseRate() const
{
	if (bOverridePulseRate)
	{
		return PulseRate;
	}
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	return Settings ? Settings->HighlightPulseRate : 1.5f;
}
