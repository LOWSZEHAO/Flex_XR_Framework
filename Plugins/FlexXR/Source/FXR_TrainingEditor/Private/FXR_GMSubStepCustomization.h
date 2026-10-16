// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class IPropertyHandle;

/**
 * Details for one sub-step's setup in the GM: its Sub Step Id is picked from a list of the step's
 * own sub-steps.
 *
 * A customization rather than a GetOptions for the same reason as the event picker: the list
 * depends on the row this sits in - the step it is under - and GetOptions can only see the asset.
 * A list of every sub-step in the procedure would let one be set up under the wrong step.
 */
class FFXR_GMSubStepCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& Utils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& Utils) override;

private:
	/** The Step Id of the step this row sits under, or None. */
	FName GetOwningStepId() const;

	/** Rebuild the list from the owning step's sub-steps as they are now. */
	void RefreshOptions();

	FText GetSubStepText() const;
	TSharedRef<SWidget> MakeOption(TSharedPtr<FName> Option) const;
	void OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How);

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> SubStepIdHandle;
	TArray<TSharedPtr<FName>> Options;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> Combo;
};
