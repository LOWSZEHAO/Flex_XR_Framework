// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_InteractableVisualizer.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Interactable/FXR_GripPoint.h"
#include "GameFramework/Actor.h"
#include "SceneManagement.h"

void FFXR_InteractableVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_InteractableBase* Interactable = Cast<UFXR_InteractableBase>(Component);
	if (!Interactable || !Interactable->IsDrawDebugEnabled())
	{
		return;
	}

	// Grip points are the only grab surface when the interactable owns any (ADR-007) — draw the
	// grab zone on the points, not a misleading mesh radius. (Editor-time: resolve by ownership.)
	bool bHasGripPoints = false;
	if (const AActor* OwnerActor = Interactable->GetOwner())
	{
		TArray<UFXR_GripPoint*> GripPoints;
		OwnerActor->GetComponents<UFXR_GripPoint>(GripPoints);
		for (const UFXR_GripPoint* Point : GripPoints)
		{
			if (Point && Point->IsOwnedBy(Interactable))
			{
				bHasGripPoints = true;
				DrawWireSphere(PDI, Point->GetComponentLocation(), FColor::Orange, Point->GetActivationRadius(), 12, SDPG_World);
			}
		}
	}

	if (!bHasGripPoints)
	{
		DrawWireSphere(PDI, Interactable->GetInteractionLocation(), FColor::Orange, Interactable->GetActivationRadius(), 16, SDPG_World);
	}
}
