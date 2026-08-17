// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PressVisualizer.h"
#include "Interactable/FXR_Press.h"
#include "Components/PrimitiveComponent.h"
#include "SceneManagement.h"

void FFXR_PressVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UFXR_Press* Press = Cast<UFXR_Press>(Component);
	if (!Press)
	{
		return;
	}

	// Preview ghost: outline the cap where full travel would put it. Drawn rather than moving the
	// mesh, because a visualizer runs against real component instances (including the Blueprint
	// editor's preview actor), whereas moving a component template does not reach the viewport.
	if (Press->IsPreviewPressed())
	{
		if (const UPrimitiveComponent* Cap = Cast<UPrimitiveComponent>(Press->GetAttachParent()))
		{
			const FBoxSphereBounds LocalBounds = Cap->CalcBounds(FTransform::Identity);
			const FBox LocalBox(LocalBounds.Origin - LocalBounds.BoxExtent, LocalBounds.Origin + LocalBounds.BoxExtent);

			FMatrix GhostMatrix = Cap->GetComponentTransform().ToMatrixWithScale();
			GhostMatrix.SetOrigin(GhostMatrix.GetOrigin() - Press->GetComponentTransform().GetUnitAxis(EAxis::Z) * Press->GetTravel());
			DrawWireBox(PDI, GhostMatrix, LocalBox, FLinearColor(0.2f, 0.85f, 1.f), SDPG_World);
		}
	}

	if (!Press->IsDrawDebugEnabled())
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

	// Basic: the pressable disc (Face Radius) + the approach normal.
	DrawCircle(PDI, Face, AxisX, AxisY, FLinearColor::White, Radius, 24, SDPG_World, 1.5f);
	PDI->DrawLine(Face, Face + Normal * 4.f, FLinearColor(0.f, 1.f, 1.f), SDPG_World, 1.5f);

	if (!Press->IsFullDebug())
	{
		return;
	}

	// Full: travel depth and the click threshold where OnPressed fires.
	DrawCircle(PDI, Face - Normal * Travel, AxisX, AxisY, FLinearColor(0.15f, 1.f, 0.15f), Radius, 24, SDPG_World, 1.f);
	PDI->DrawLine(Face, Face - Normal * Travel, FLinearColor(0.15f, 1.f, 0.15f), SDPG_World, 1.5f);
	DrawCircle(PDI, Face - Normal * (Travel * Press->GetActivationFraction()), AxisX, AxisY, FLinearColor(1.f, 0.9f, 0.f), Radius * 0.9f, 24, SDPG_World, 1.f);
}
