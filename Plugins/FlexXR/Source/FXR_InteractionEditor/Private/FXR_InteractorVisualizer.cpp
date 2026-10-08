// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_InteractorVisualizer.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "SceneManagement.h"

void FFXR_InteractorVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_InteractorComponent* Interactor = Cast<UFXR_InteractorComponent>(Component);
	if (!Interactor || !Interactor->IsEditorGizmoEnabled())
	{
		return;
	}

	const FVector Origin = Interactor->GetComponentLocation();

	// Near grab sphere (yellow) — the broad-phase reach for grabbing.
	FVector GrabCenter;
	float GrabRadius = 0.f;
	Interactor->GetGrabSphere(GrabCenter, GrabRadius);
	DrawWireSphere(PDI, GrabCenter, FColor::Yellow, GrabRadius, 16, SDPG_World);

	// Poke tip (green) — the fingertip probe that presses FXR_Press. Drag Poke Local Offset until
	// this sphere sits on the hand mesh's index fingertip; the line shows the offset from the origin.
	FVector PokeLocation;
	float PokeRadius = 0.f;
	Interactor->GetPokeTip(PokeLocation, PokeRadius);
	DrawWireSphere(PDI, PokeLocation, FColor::Green, PokeRadius, 12, SDPG_World);
	PDI->DrawLine(Origin, PokeLocation, FLinearColor(0.15f, 1.f, 0.15f), SDPG_World, 0.75f);

	// Far ray — drawn in the beam colour, at a fixed preview length rather than the driver's reach:
	// this is for aiming, and a 20 m line across the level would only be in the way. Shown here so
	// the beam is visible while any interactor offset is being tuned; the hand's Ray Origin draws
	// the same arrow while it is the component selected, which is where it actually gets aimed.
	const FTransform Aim = Interactor->GetAimTransform();
	constexpr float PreviewLength = 120.f;
	DrawDirectionalArrow(PDI, Aim.ToMatrixNoScale(), FLinearColor(0.45f, 0.8f, 1.f), PreviewLength, 6.f, SDPG_World, 1.5f);
	PDI->DrawPoint(Aim.GetLocation(), FLinearColor(0.45f, 0.8f, 1.f), 10.f, SDPG_World);

	// Grip / palm anchors (white / cyan) — the other tunable local offsets, for reference.
	PDI->DrawPoint(Interactor->GetGripTransform().GetLocation(), FLinearColor::White, 10.f, SDPG_World);
	PDI->DrawPoint(Interactor->GetPalmTransform().GetLocation(), FLinearColor(0.f, 1.f, 1.f), 8.f, SDPG_World);
}
