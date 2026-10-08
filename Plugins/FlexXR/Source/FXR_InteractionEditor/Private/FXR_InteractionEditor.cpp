// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_InteractionEditor.h"
#include "FXR_GripPointVisualizer.h"
#include "FXR_InteractableVisualizer.h"
#include "FXR_LatchVisualizer.h"
#include "FXR_PressVisualizer.h"
#include "FXR_InteractorVisualizer.h"
#include "FXR_RayOriginVisualizer.h"
#include "FXR_PressDetails.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Interactor/FXR_RayOrigin.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_Latch.h"
#include "Interactable/FXR_Press.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FXR_InteractionEditor"

void FFXR_InteractionEditorModule::StartupModule()
{
	{
		// Detail customizations run independently of GUnrealEd.
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			UFXR_Press::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FFXR_PressDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	if (!GUnrealEd)
	{
		return;
	}

	{
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_GripPointVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_GripPoint::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
	{
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_InteractableVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_Grab::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
	{
		// The latch gets its own gizmo (pivot + axis + Min..Max range), drawn from the component.
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_LatchVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_Latch::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
	{
		// The press draws its face disc + travel/click depths.
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_PressVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_Press::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
	{
		// Interactor query shapes (grab sphere + poke tip) so their offsets can be tuned visually.
		// Registered on the base: visualizer lookup walks up the class hierarchy, so the controller,
		// tracked-hand and desktop-sim interactors all inherit it.
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_InteractorVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_InteractorComponent::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
	{
		// The per-hand ray origin draws its own beam, so it stays visible while it is the thing being
		// dragged — selecting a component hides every other component's visualizer.
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_RayOriginVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_RayOrigin::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
}

void FFXR_InteractionEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomClassLayout(UFXR_Press::StaticClass()->GetFName());
	}

	if (!GUnrealEd)
	{
		return;
	}

	GUnrealEd->UnregisterComponentVisualizer(UFXR_GripPoint::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_Grab::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_Latch::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_Press::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_InteractorComponent::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_RayOrigin::StaticClass()->GetFName());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFXR_InteractionEditorModule, FXR_InteractionEditor)
