// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_GMSubStepCustomization.h"

#include "Data/FXR_TrainingGM.h"
#include "Data/FXR_TrainingSteps.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FXR_GMSubStepCustomization"

TSharedRef<IPropertyTypeCustomization> FFXR_GMSubStepCustomization::MakeInstance()
{
	return MakeShared<FFXR_GMSubStepCustomization>();
}

void FFXR_GMSubStepCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils)
{
	HeaderRow
		.NameContent()
		[
			InStructHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			InStructHandle->CreatePropertyValueWidget(false)
		];
}

void FFXR_GMSubStepCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& Utils)
{
	StructHandle = InStructHandle;
	SubStepIdHandle = InStructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_GMSubStep, SubStepId));

	uint32 NumChildren = 0;
	InStructHandle->GetNumChildren(NumChildren);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		const TSharedPtr<IPropertyHandle> Child = InStructHandle->GetChildHandle(Index);
		if (!Child.IsValid())
		{
			continue;
		}

		const FProperty* Property = Child->GetProperty();
		if (!Property || Property->GetFName() != GET_MEMBER_NAME_CHECKED(FFXR_GMSubStep, SubStepId))
		{
			ChildBuilder.AddProperty(Child.ToSharedRef());
			continue;
		}

		RefreshOptions();

		ChildBuilder.AddCustomRow(Child->GetPropertyDisplayName())
			.NameContent()
			[
				Child->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MinDesiredWidth(220.f)
			[
				SAssignNew(Combo, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&Options)
				.OnGenerateWidget(this, &FFXR_GMSubStepCustomization::MakeOption)
				.OnSelectionChanged(this, &FFXR_GMSubStepCustomization::OnOptionChosen)
				// Rebuilt on opening, so a changed Step Id above, or a sub-step added to the Steps
				// asset since, shows without reopening the GM.
				.OnComboBoxOpening(this, &FFXR_GMSubStepCustomization::RefreshOptions)
				[
					SNew(STextBlock)
					.Text(this, &FFXR_GMSubStepCustomization::GetSubStepText)
					.Font(Utils.GetRegularFont())
				]
			];
	}
}

FName FFXR_GMSubStepCustomization::GetOwningStepId() const
{
	// This row is an element of its step's Sub Steps array, so the step is two handles up.
	const TSharedPtr<IPropertyHandle> Array = StructHandle.IsValid() ? StructHandle->GetParentHandle() : nullptr;
	const TSharedPtr<IPropertyHandle> Step = Array.IsValid() ? Array->GetParentHandle() : nullptr;
	const TSharedPtr<IPropertyHandle> StepIdHandle =
		Step.IsValid() ? Step->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_GMStep, StepId)) : nullptr;

	FName StepId;
	if (!StepIdHandle.IsValid() || StepIdHandle->GetValue(StepId) != FPropertyAccess::Success)
	{
		return NAME_None;
	}

	return StepId;
}

void FFXR_GMSubStepCustomization::RefreshOptions()
{
	Options.Reset();

	const FName StepId = GetOwningStepId();

	TArray<UObject*> Outers;
	if (StructHandle.IsValid() && !StepId.IsNone())
	{
		StructHandle->GetOuterObjects(Outers);
	}

	for (const UObject* Outer : Outers)
	{
		const UFXR_TrainingGM* GM = Cast<UFXR_TrainingGM>(Outer);
		if (GM && GM->Steps)
		{
			for (const FString& Id : GM->Steps->GetSubStepIdStrings(StepId))
			{
				Options.Add(MakeShared<FName>(*Id));
			}
			break;
		}
	}

	if (Combo.IsValid())
	{
		Combo->RefreshOptions();
	}
}

FText FFXR_GMSubStepCustomization::GetSubStepText() const
{
	FName Value;
	if (SubStepIdHandle.IsValid() && SubStepIdHandle->GetValue(Value) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("Multiple", "Multiple Values");
	}

	if (!Value.IsNone())
	{
		return FText::FromName(Value);
	}

	if (GetOwningStepId().IsNone())
	{
		return LOCTEXT("PickStep", "Pick the step's Step Id first");
	}

	return Options.IsEmpty() ? LOCTEXT("NoSubSteps", "This step has no sub-steps") : LOCTEXT("PickSubStep", "Pick a sub-step");
}

TSharedRef<SWidget> FFXR_GMSubStepCustomization::MakeOption(TSharedPtr<FName> Option) const
{
	return SNew(STextBlock).Text(Option.IsValid() ? FText::FromName(*Option) : FText::GetEmpty());
}

void FFXR_GMSubStepCustomization::OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How)
{
	// Direct is the combo being set from code, not by the author choosing something.
	if (Option.IsValid() && SubStepIdHandle.IsValid() && How != ESelectInfo::Direct)
	{
		SubStepIdHandle->SetValue(*Option);
	}
}

#undef LOCTEXT_NAMESPACE
