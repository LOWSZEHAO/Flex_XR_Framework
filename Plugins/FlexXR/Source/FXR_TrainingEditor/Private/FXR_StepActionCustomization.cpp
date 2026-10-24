// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_StepActionCustomization.h"

#include "Data/FXR_TrainingGM.h"
#include "DetailWidgetRow.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FXR_StepActionCustomization"

namespace
{
	/**
	 * Functions the Blueprint compiler generates (event graph, construction script, bound
	 * component/input events, engine event overrides). Not something you'd call from a procedure.
	 */
	bool IsCompilerMade(const FString& Name)
	{
		static const TCHAR* const Prefixes[] = {
			TEXT("ExecuteUbergraph"),
			TEXT("UserConstructionScript"),
			TEXT("BndEvt__"),
			TEXT("InpActEvt_"),
			TEXT("InpAxisEvt_"),
			TEXT("InpAxisKeyEvt_"),
			TEXT("Receive"),
		};

		for (const TCHAR* Prefix : Prefixes)
		{
			if (Name.StartsWith(Prefix))
			{
				return true;
			}
		}

		return false;
	}
}

TSharedRef<IPropertyTypeCustomization> FFXR_StepActionCustomization::MakeInstance()
{
	return MakeShared<FFXR_StepActionCustomization>();
}

void FFXR_StepActionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& Utils)
{
	HeaderRow
		.NameContent()
		[
			StructHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructHandle->CreatePropertyValueWidget(false)
		];
}

void FFXR_StepActionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& Utils)
{
	BlueprintHandle = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_StepAction, Blueprint));
	EventHandle = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_StepAction, EventName));
	const TSharedPtr<IPropertyHandle> TagHandle = StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_StepAction, Tag));
	const TSharedPtr<IPropertyHandle> AutoStartHandle =
		StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_StepAction, bAutoStartInExam));
	const TSharedPtr<IPropertyHandle> ExamOnlyHandle =
		StructHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FFXR_StepAction, bExamOnly));

	if (!BlueprintHandle.IsValid() || !EventHandle.IsValid() || !TagHandle.IsValid()
		|| !AutoStartHandle.IsValid() || !ExamOnlyHandle.IsValid())
	{
		return;
	}

	ChildBuilder.AddProperty(BlueprintHandle.ToSharedRef());
	ChildBuilder.AddProperty(TagHandle.ToSharedRef());

	// A different Blueprint means a different list.
	BlueprintHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FFXR_StepActionCustomization::RefreshOptions));
	RefreshOptions();

	ChildBuilder.AddCustomRow(EventHandle->GetPropertyDisplayName())
		.NameContent()
		[
			EventHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(220.f)
		[
			SAssignNew(Combo, SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&Options)
			.OnGenerateWidget(this, &FFXR_StepActionCustomization::MakeOption)
			.OnSelectionChanged(this, &FFXR_StepActionCustomization::OnOptionChosen)
			// Also rebuilt on open, so newly added events show up without reopening the asset.
			.OnComboBoxOpening(this, &FFXR_StepActionCustomization::RefreshOptions)
			[
				SNew(STextBlock)
				.Text(this, &FFXR_StepActionCustomization::GetEventText)
				.Font(Utils.GetRegularFont())
			]
		];

	ChildBuilder.AddProperty(AutoStartHandle.ToSharedRef());
	ChildBuilder.AddProperty(ExamOnlyHandle.ToSharedRef());
}

void FFXR_StepActionCustomization::RefreshOptions()
{
	Options.Reset();

	FString Path;
	if (BlueprintHandle.IsValid()
		&& BlueprintHandle->GetValueAsFormattedString(Path) == FPropertyAccess::Success
		&& !Path.IsEmpty() && Path != TEXT("None"))
	{
		if (UClass* Class = TSoftClassPtr<AActor>(FSoftObjectPath(Path)).LoadSynchronous())
		{
			for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const UFunction* Function = *It;

				// Only functions the Blueprint author wrote; the engine's own would flood the list.
				if (!Cast<UBlueprintGeneratedClass>(Function->GetOwnerClass())
					|| Function->HasAnyFunctionFlags(FUNC_Delegate)
					|| Function->NumParms > 0
					|| IsCompilerMade(Function->GetName()))
				{
					continue;
				}

				Options.Add(MakeShared<FName>(Function->GetFName()));
			}

			Options.Sort([](const TSharedPtr<FName>& A, const TSharedPtr<FName>& B)
				{ return A->LexicalLess(*B); });
		}
	}

	if (Combo.IsValid())
	{
		Combo->RefreshOptions();
	}
}

FText FFXR_StepActionCustomization::GetEventText() const
{
	FName Value;
	if (EventHandle.IsValid() && EventHandle->GetValue(Value) == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("Multiple", "Multiple Values");
	}

	if (!Value.IsNone())
	{
		return FText::FromName(Value);
	}

	FString Path;
	const bool bHasBlueprint = BlueprintHandle.IsValid()
		&& BlueprintHandle->GetValueAsFormattedString(Path) == FPropertyAccess::Success
		&& !Path.IsEmpty() && Path != TEXT("None");

	return bHasBlueprint ? LOCTEXT("PickEvent", "Pick an event") : LOCTEXT("PickBlueprint", "Pick a Blueprint first");
}

TSharedRef<SWidget> FFXR_StepActionCustomization::MakeOption(TSharedPtr<FName> Option) const
{
	return SNew(STextBlock).Text(Option.IsValid() ? FText::FromName(*Option) : FText::GetEmpty());
}

void FFXR_StepActionCustomization::OnOptionChosen(TSharedPtr<FName> Option, ESelectInfo::Type How)
{
	// Direct = set from code, not picked by the user.
	if (Option.IsValid() && EventHandle.IsValid() && How != ESelectInfo::Direct)
	{
		EventHandle->SetValue(*Option);
	}
}

#undef LOCTEXT_NAMESPACE
