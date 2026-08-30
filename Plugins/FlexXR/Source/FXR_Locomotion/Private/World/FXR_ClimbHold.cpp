// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_ClimbHold.h"

UFXR_ClimbHold::UFXR_ClimbHold()
{
	// The base ticks only to draw its debug shape, and only enables that at BeginPlay. A climb hold
	// is positioned by reaching for it, so its reach sphere has to be visible while placing it.
	bTickInEditor = true;
}

void UFXR_ClimbHold::OnRegister()
{
	Super::OnRegister();

	RefreshTickState();
}

#if WITH_EDITOR
void UFXR_ClimbHold::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Takes effect the moment Debug Draw changes, rather than at the next BeginPlay.
	RefreshTickState();
}
#endif

void UFXR_ClimbHold::RefreshTickState()
{
	SetComponentTickEnabled(IsDrawDebugEnabled());
}

void UFXR_ClimbHold::OnBegin(IFXR_Interactor* Interactor)
{
	Super::OnBegin(Interactor);

	HandCount = 1;
	OnGrabbed.Broadcast();
}

void UFXR_ClimbHold::OnBeginSecondary(IFXR_Interactor* Interactor)
{
	Super::OnBeginSecondary(Interactor);

	++HandCount;
	OnGrabbed.Broadcast();
}

void UFXR_ClimbHold::ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason)
{
	OnReleased.Broadcast();

	// Only the last hand off ends the hold — letting go with one hand while hanging by the other is
	// the ordinary case, not the end of the climb.
	if (--HandCount > 0)
	{
		return;
	}

	OnEnd(Reason);
}

void UFXR_ClimbHold::OnEnd(EFXR_EndReason Reason)
{
	HandCount = 0;
	Super::OnEnd(Reason);
}
