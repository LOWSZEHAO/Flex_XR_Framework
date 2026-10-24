// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

class IPropertyHandle;

/**
 * Details panel for a GM sub-step: the Sub Step Id is a dropdown of the parent step's own
 * sub-steps. Same reason as the event picker: GetOptions can't see which step row it's under.
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
	/** Step Id of the step this row belongs to, or None. */
	FName GetOwningStepId() const;

	/** Rebuild the list from the parent step's current sub-steps. */
	void RefreshOptions();

	FText GetSubStepText() const;
	TSharedRef<SWidget> MakeOption(TSharedPtr<FName> Option) const;
	void OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How);

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> SubStepIdHandle;
	TArray<TSharedPtr<FName>> Options;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> Combo;
};
