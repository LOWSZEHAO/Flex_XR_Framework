// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_RayTarget.h"

UFXR_RayTarget::UFXR_RayTarget()
{
	// Aiming is driven by the rig's interaction driver, so there is nothing to tick per target.
	PrimaryComponentTick.bCanEverTick = false;

	// Nothing here is a grab surface, so the activation radius the base uses for the grab broad
	// phase is irrelevant; MaxRayDistance is this component's reach instead.
	ActivationRadius = 1.f;
}

void UFXR_RayTarget::NotifyRayEnter(EFXR_HandSide Hand)
{
	if (IsInteractionEnabled())
	{
		OnRayEnter.Broadcast(Hand);
	}
}

void UFXR_RayTarget::NotifyRayExit(EFXR_HandSide Hand)
{
	// Broadcast even when disabled: a target switched off mid-aim still has to let listeners undo
	// whatever they did on enter, or a highlight or prompt is stranded on.
	OnRayExit.Broadcast(Hand);
}

void UFXR_RayTarget::NotifyRaySelected(EFXR_HandSide Hand)
{
	if (IsInteractionEnabled())
	{
		OnRaySelected.Broadcast(Hand);
	}
}
