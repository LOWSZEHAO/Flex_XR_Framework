// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

/**
 * Detail panel customization for UFXR_Press: hides inherited fields that do nothing for a poke
 * button. A press is never claimed by grab detection (IsGrabTarget is false), so the base's
 * Activation Radius is dead weight — Face Radius is the size that matters.
 */
class FFXR_PressDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
