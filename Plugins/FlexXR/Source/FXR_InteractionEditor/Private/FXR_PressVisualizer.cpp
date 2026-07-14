// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PressVisualizer.h"
#include "Interactable/FXR_Press.h"
#include "SceneManagement.h"

void FFXR_PressVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_Press* Press = Cast<UFXR_Press>(Component);
	if (!Press || !Press->IsDrawDebugEnabled())
	{
		return;
	}

	const FTransform Xf = Press->GetComponentTransform();
	const FVector Face = Xf.GetLocation();
	const FVector Normal = Xf.GetUnitAxis(EAxis::Z);
	const FVector AxisX = Xf.GetUnitAxis(EAxis::X);
	const FVector AxisY = Xf.GetUnitAxis(EAxis::Y);
	const float Radius = Press->GetFaceRadius();
	const float Travel = Press->GetTravel();

	// Face disc at rest (white) and the fully-pressed depth (green); normal arrow shows approach.
	DrawCircle(PDI, Face, AxisX, AxisY, FLinearColor::White, Radius, 24, SDPG_World, 1.5f);
	DrawCircle(PDI, Face - Normal * Travel, AxisX, AxisY, FLinearColor(0.15f, 1.f, 0.15f), Radius, 24, SDPG_World, 1.f);
	PDI->DrawLine(Face, Face + Normal * 4.f, FLinearColor(0.f, 1.f, 1.f), SDPG_World, 1.5f);
	PDI->DrawLine(Face, Face - Normal * Travel, FLinearColor(0.15f, 1.f, 0.15f), SDPG_World, 1.5f);
}
