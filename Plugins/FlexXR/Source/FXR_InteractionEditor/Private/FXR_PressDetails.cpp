// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PressDetails.h"
#include "Interactable/FXR_InteractableBase.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FFXR_PressDetails::MakeInstance()
{
	return MakeShared<FFXR_PressDetails>();
}

void FFXR_PressDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Grab-detection reach is meaningless on a poke button; the fingertip is tested against
	// Face Radius instead. Hidden rather than disabled so the panel stays clean.
	DetailBuilder.HideProperty(TEXT("ActivationRadius"), UFXR_InteractableBase::StaticClass());
}
