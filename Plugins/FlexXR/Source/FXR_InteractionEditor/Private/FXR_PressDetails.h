// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;

/**
 * Detail panel customization for UFXR_Press.
 *
 * Two jobs:
 * 1. Hides inherited fields that do nothing for a poke button — a press is never claimed by grab
 *    detection (IsGrabTarget is false), so Activation Radius is dead weight; Face Radius is the
 *    size that matters.
 * 2. Drives the editor press preview. The component can move the cap by itself, but on a Blueprint
 *    template that alone changes nothing on screen: the editor's preview actor was already built
 *    from the old templates. Rebuilding it is editor-only work, so it lives here.
 */
class FFXR_PressDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Re-apply the preview and refresh any Blueprint the edited presses belong to. */
	void RefreshPreview();

	TArray<TWeakObjectPtr<UObject>> CustomizedObjects;
};
