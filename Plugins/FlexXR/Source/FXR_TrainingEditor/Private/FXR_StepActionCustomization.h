// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class IPropertyHandle;

/**
 * Details for one GM event: pick a Blueprint, then pick one of its events from a list.
 *
 * The event is a name the GM calls by reflection, so a typed one is a typo waiting to surface at
 * step nine. A dropdown built from the chosen Blueprint makes the wrong name impossible to enter -
 * which is why it is worth a customization rather than a GetOptions on the GM: the list depends on
 * the Blueprint picked in the same row, and GetOptions can only see the asset, not the row.
 */
class FFXR_StepActionCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& Utils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& Utils) override;

private:
	/** Rebuild the list from whatever Blueprint is chosen now. */
	void RefreshOptions();

	FText GetEventText() const;
	TSharedRef<SWidget> MakeOption(TSharedPtr<FName> Option) const;
	void OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How);

	TSharedPtr<IPropertyHandle> BlueprintHandle;
	TSharedPtr<IPropertyHandle> EventHandle;
	TArray<TSharedPtr<FName>> Options;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> Combo;
};
