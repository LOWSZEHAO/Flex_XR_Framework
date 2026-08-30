// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Driver/FXR_InteractionDriver.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Interactable/FXR_Press.h"
#include "Interactor/FXR_Interactor.h"
#include "Rig/FXR_Pawn.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UFXR_InteractionDriver::UFXR_InteractionDriver()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_InteractionDriver::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_InteractionDriver_Tick);

	DriveHand(EFXR_HandSide::Left, LeftHeld, LeftPrevSelect, DeltaTime);
	DriveHand(EFXR_HandSide::Right, RightHeld, RightPrevSelect, DeltaTime);
	DrivePokes(EFXR_HandSide::Left);
	DrivePokes(EFXR_HandSide::Right);
}

void UFXR_InteractionDriver::DrivePokes(EFXR_HandSide Side)
{
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	if (!Interactor)
	{
		return;
	}

	// A hand that owns a hold isn't a poking finger — its fingers are wrapped around something.
	if (GetHeldInteractable(Side))
	{
		return;
	}

	UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	FVector TipLocation;
	float TipRadius = 0.f;
	Interactor->GetPokeTip(TipLocation, TipRadius);

	// Linear pass over the registry (first-pass detection, design 5.4); presses cull precisely.
	for (const TObjectPtr<UFXR_InteractableBase>& Interactable : Subsystem->GetRegistered())
	{
		if (UFXR_Press* Press = Cast<UFXR_Press>(Interactable.Get()))
		{
			Press->NotifyPoke(TipLocation, TipRadius, Interactor);
		}
	}
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

void UFXR_InteractionDriver::DriveHand(EFXR_HandSide Side, TWeakObjectPtr<UFXR_InteractableBase>& Held, float& PrevSelect, float DeltaTime)
{
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	if (!Interactor)
	{
		PrevSelect = 0.f;
		return;
	}

	const float Select = Interactor->GetSelectValue();

	// Claiming needs the rising edge, not merely a held grip: otherwise a closed hand moving through
	// the world picks up everything it touches.
	const bool bGrabPressed = (Select >= GrabThreshold) && (PrevSelect < GrabThreshold);
	PrevSelect = Select;

	if (UFXR_InteractableBase* Current = Held.Get())
	{
		if (!Current->IsHeld())
		{
			// The interactable ended the hold itself (ForceRelease, or disabled mid-grab) — just drop it.
			Held = nullptr;
		}
		else if (Select < ReleaseThreshold)
		{
			// Role-aware: a two-hand hold detaches just this hand (or promotes the survivor);
			// single-hand ends the interaction. The interactable decides — the driver has no roles.
			Current->ReleaseHand(Interactor, EFXR_EndReason::Released);
			Held = nullptr;
		}
		else
		{
			Current->OnUpdate(Interactor, DeltaTime);
		}
		return;
	}

	if (bGrabPressed)
	{
		FVector GrabCenter;
		float GrabRadius = 0.f;
		Interactor->GetGrabSphere(GrabCenter, GrabRadius);

		// Joining the other hand's hold wins over starting a fresh grab — reaching for an object
		// you are already holding is nearly always intentional (rifle foregrip, two-hand carry).
		TWeakObjectPtr<UFXR_InteractableBase>& OtherHeld = (Side == EFXR_HandSide::Left) ? RightHeld : LeftHeld;
		if (UFXR_InteractableBase* Other = OtherHeld.Get())
		{
			float DistanceSq = 0.f;
			if (Other->CanBeginSecondary(Interactor) && Other->IsInGrabReach(GrabCenter, GrabRadius, Side, DistanceSq))
			{
				Other->OnBeginSecondary(Interactor);
				Held = Other;
				return;
			}
		}

		if (UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this))
		{
			if (UFXR_InteractableBase* Candidate = Subsystem->FindBestCandidate(GrabCenter, GrabRadius, Side))
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
