// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_TrainingEditor.h"

#include "Data/FXR_TrainingGM.h"
#include "FXR_StepActionCustomization.h"
#include "PropertyEditorModule.h"

void FFXR_TrainingEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FFXR_StepAction::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FFXR_StepActionCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FFXR_TrainingEditorModule::ShutdownModule()
{
	// The property editor can be gone first on editor exit, and asking for it then would load it.
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FFXR_StepAction::StaticStruct()->GetFName());
	}
}

IMPLEMENT_MODULE(FFXR_TrainingEditorModule, FXR_TrainingEditor)
