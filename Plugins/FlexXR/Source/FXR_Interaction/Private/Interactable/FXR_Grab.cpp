// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Grab.h"
#include "Interactor/FXR_Interactor.h"
#include "Components/PrimitiveComponent.h"

void UFXR_Grab::OnBegin(IFXR_Interactor* Interactor)
{
	Super::OnBegin(Interactor);
	if (!Interactor)
	{
		return;
	}

	UPrimitiveComponent* Driven = ResolveDrivenComponent();
	HeldComponent = Driven;
	if (Driven)
	{
		bRestorePhysics = Driven->IsSimulatingPhysics();
		if (bRestorePhysics)
		{
			Driven->SetSimulatePhysics(false);
		}
		// Driven == HeldOffset * Grip, so HeldOffset reconstructs the hold as the grip moves.
		HeldOffset = Driven->GetComponentTransform().GetRelativeTransform(Interactor->GetGripTransform());
	}
}

void UFXR_Grab::OnUpdate(IFXR_Interactor* Interactor, float DeltaTime)
{
	if (!Interactor)
	{
		return;
	}

	if (UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		Driven->SetWorldTransform(HeldOffset * Interactor->GetGripTransform(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UFXR_Grab::OnEnd(EFXR_EndReason Reason)
{
	if (UPrimitiveComponent* Driven = HeldComponent.Get())
	{
		if (bRestorePhysics)
		{
			Driven->SetSimulatePhysics(true);
		}
	}

	HeldComponent = nullptr;
	bRestorePhysics = false;
	Super::OnEnd(Reason);
}
