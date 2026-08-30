// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_ClimbHold.h"

UFXR_ClimbHold::UFXR_ClimbHold()
{
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
