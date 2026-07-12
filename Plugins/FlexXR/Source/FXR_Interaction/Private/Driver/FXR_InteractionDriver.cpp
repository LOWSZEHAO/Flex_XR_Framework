// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Driver/FXR_InteractionDriver.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Interactor/FXR_Interactor.h"
#include "Rig/FXR_Pawn.h"

UFXR_InteractionDriver::UFXR_InteractionDriver()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_InteractionDriver::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DriveHand(EFXR_HandSide::Left, LeftHeld, DeltaTime);
	DriveHand(EFXR_HandSide::Right, RightHeld, DeltaTime);
}

IFXR_Interactor* UFXR_InteractionDriver::GetActiveInteractor(EFXR_HandSide Side) const
{
	if (AFXR_Pawn* Pawn = Cast<AFXR_Pawn>(GetOwner()))
	{
		return Pawn->GetActiveInteractor(Side);
	}
	return nullptr;
}

UFXR_InteractableBase* UFXR_InteractionDriver::GetHeldInteractable(EFXR_HandSide Side) const
{
	return (Side == EFXR_HandSide::Left ? LeftHeld : RightHeld).Get();
}

void UFXR_InteractionDriver::DriveHand(EFXR_HandSide Side, TWeakObjectPtr<UFXR_InteractableBase>& Held, float DeltaTime)
{
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	if (!Interactor)
	{
		return;
	}

	const float Select = Interactor->GetSelectValue();

	if (UFXR_InteractableBase* Current = Held.Get())
	{
		if (!Current->IsHeld())
		{
			// The interactable ended the hold itself (ForceRelease, or disabled mid-grab) — just drop it.
			Held = nullptr;
		}
		else if (Select < ReleaseThreshold)
		{
			Current->OnEnd(EFXR_EndReason::Released);
			Held = nullptr;
		}
		else
		{
			Current->OnUpdate(Interactor, DeltaTime);
		}
		return;
	}

	if (Select >= GrabThreshold)
	{
		if (UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this))
		{
			FVector GrabCenter;
			float GrabRadius = 0.f;
			Interactor->GetGrabSphere(GrabCenter, GrabRadius);

			if (UFXR_InteractableBase* Candidate = Subsystem->FindBestCandidate(GrabCenter, GrabRadius))
			{
				if (Candidate->CanBegin(Interactor))
				{
					Candidate->OnBegin(Interactor);
					Held = Candidate;
				}
			}
		}
	}
}
