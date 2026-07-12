// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_InteractableVisualizer.h"
#include "Interactable/FXR_InteractableBase.h"
#include "SceneManagement.h"

void FFXR_InteractableVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_InteractableBase* Interactable = Cast<UFXR_InteractableBase>(Component);
	if (!Interactable)
	{
		return;
	}

	DrawWireSphere(PDI, Interactable->GetInteractionLocation(), FColor::Orange, Interactable->GetActivationRadius(), 16, SDPG_World);
}
