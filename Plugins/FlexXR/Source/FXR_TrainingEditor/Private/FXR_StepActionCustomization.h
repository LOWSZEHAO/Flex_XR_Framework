// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class IPropertyHandle;

/**
 * Details panel for a GM event: pick a Blueprint, then pick one of its events from a dropdown.
 *
 * The GM calls the event by name through reflection, so a typo would only show up at runtime. A
 * customization is needed (not GetOptions) because the list depends on the Blueprint picked in the
 * same row, and GetOptions only sees the asset.
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
	/** Rebuild the list from the currently selected Blueprint. */
	void RefreshOptions();

	FText GetEventText() const;
	TSharedRef<SWidget> MakeOption(TSharedPtr<FName> Option) const;
	void OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How);

	TSharedPtr<IPropertyHandle> BlueprintHandle;
	TSharedPtr<IPropertyHandle> EventHandle;
	TArray<TSharedPtr<FName>> Options;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> Combo;
};
