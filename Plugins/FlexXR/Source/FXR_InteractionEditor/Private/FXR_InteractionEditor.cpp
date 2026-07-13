// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_InteractionEditor.h"
#include "FXR_GripPointVisualizer.h"
#include "FXR_InteractableVisualizer.h"
#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_Latch.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FXR_InteractionEditor"

void FFXR_InteractionEditorModule::StartupModule()
{
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
		// The activation-radius gizmo applies to any interactable — register it for the latch too.
		TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FFXR_InteractableVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UFXR_Latch::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
}

void FFXR_InteractionEditorModule::ShutdownModule()
{
	if (!GUnrealEd)
	{
		return;
	}

	GUnrealEd->UnregisterComponentVisualizer(UFXR_GripPoint::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_Grab::StaticClass()->GetFName());
	GUnrealEd->UnregisterComponentVisualizer(UFXR_Latch::StaticClass()->GetFName());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFXR_InteractionEditorModule, FXR_InteractionEditor)
