// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PressDetails.h"
#include "Interactable/FXR_Press.h"
#include "Interactable/FXR_InteractableBase.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"

TSharedRef<IDetailCustomization> FFXR_PressDetails::MakeInstance()
{
	return MakeShared<FFXR_PressDetails>();
}

void FFXR_PressDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Grab-detection reach is meaningless on a poke button; the fingertip is tested against
	// Face Radius instead. Hidden rather than disabled so the panel stays clean.
	DetailBuilder.HideProperty(TEXT("ActivationRadius"), UFXR_InteractableBase::StaticClass());

	DetailBuilder.GetObjectsBeingCustomized(CustomizedObjects);

	// The preview must re-run when it is toggled and when the travel it depends on changes.
	const TCHAR* PreviewDrivers[] = { TEXT("bPreviewPressed"), TEXT("Travel") };
	for (const TCHAR* PropertyName : PreviewDrivers)
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropertyName, UFXR_Press::StaticClass());
		if (Handle->IsValidHandle())
		{
			Handle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFXR_PressDetails::RefreshPreview));
		}
	}
}

void FFXR_PressDetails::RefreshPreview()
{
	for (const TWeakObjectPtr<UObject>& Object : CustomizedObjects)
	{
		UFXR_Press* Press = Cast<UFXR_Press>(Object.Get());
		if (!Press)
		{
			continue;
		}

		// Idempotent — the component restores the cap before re-applying, so running after the
		// component's own PostEditChangeProperty simply lands on the same result.
		Press->ApplyEditorPreview();

		// A placed instance already shows the move. A Blueprint template does not: the preview
		// actor is built from templates, so ask for the Blueprint's actors to be rebuilt.
		if (const UBlueprintGeneratedClass* BPClass = Press->GetTypedOuter<UBlueprintGeneratedClass>())
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(BPClass->ClassGeneratedBy))
			{
				FBlueprintEditorUtils::PostEditChangeBlueprintActors(Blueprint, /*bComponentEditChange*/ true);
			}
		}
	}
}
