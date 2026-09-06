// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Settings/FXR_MotionSettings.h"

const UFXR_MotionSettings* UFXR_MotionSettings::Get()
{
	return GetDefault<UFXR_MotionSettings>();
}

float UFXR_MotionSettings::GetFadeDuration()
{
	const UFXR_MotionSettings* Settings = Get();
	return Settings ? Settings->FadeDuration : 0.15f;
}
