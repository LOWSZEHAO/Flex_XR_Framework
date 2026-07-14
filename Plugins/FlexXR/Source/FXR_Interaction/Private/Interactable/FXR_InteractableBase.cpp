// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_InteractableBase.h"
#include "Interactable/FXR_GripPoint.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Interactor/FXR_Interactor.h"
#include "Events/FXR_EventBus.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

UFXR_InteractableBase::UFXR_InteractableBase()
{
	// Tick exists only for the optional debug draw; disabled unless bDrawDebugRadius is set.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_InteractableBase::BeginPlay()
{
	Super::BeginPlay();

	if (UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this))
	{
		Subsystem->RegisterInteractable(this);
	}

	SetComponentTickEnabled(bDrawDebugRadius);
}

void UFXR_InteractableBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDebugRadius)
	{
		if (const UWorld* World = GetWorld())
		{
			const FColor Color = bHeld ? FColor::Green : (bInteractionEnabled ? FColor::Orange : FColor::Red);
			if (HasOwnedGripPoints())
			{
				// Grip points are the only grab surface (ADR-007) — show state on them, not a
				// misleading mesh radius.
				for (const TWeakObjectPtr<UFXR_GripPoint>& WeakPoint : OwnedGripPoints)
				{
					if (const UFXR_GripPoint* Point = WeakPoint.Get())
					{
						DrawDebugSphere(World, Point->GetComponentLocation(), Point->GetActivationRadius(), 12, Color, false, -1.f, 0, 0.5f);
					}
				}
			}
			else
			{
				DrawDebugSphere(World, GetInteractionLocation(), ActivationRadius, 16, Color, false, -1.f, 0, 0.5f);
			}
		}
	}
}

void UFXR_InteractableBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this))
	{
		Subsystem->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UFXR_InteractableBase::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;

	// Disabled mid-hold: FinishNaturally leaves the current hold alone (re-grab already blocked by
	// CanBegin); ForceRelease rips it from the hand now.
	if (!bEnabled && bHeld && AlreadyHeldPolicy == EFXR_AlreadyHeldPolicy::ForceRelease)
	{
		ForceRelease();
	}
}

void UFXR_InteractableBase::ForceRelease()
{
	if (bHeld)
	{
		// Ends the hold via the same lifecycle as a normal release; the interaction driver sees
		// IsHeld() flip false next tick and drops its reference (no double OnEnd).
		OnEnd(EFXR_EndReason::ForceReleased);
	}
}

bool UFXR_InteractableBase::CanBegin(IFXR_Interactor* Interactor) const
{
	return bInteractionEnabled && !bHeld;
}

void UFXR_InteractableBase::OnBegin(IFXR_Interactor* Interactor)
{
	bHeld = true;
	BroadcastInteractionEvent(EFXR_InteractionPhase::Began, Interactor);
}

void UFXR_InteractableBase::OnUpdate(IFXR_Interactor* Interactor, float DeltaTime)
{
}

void UFXR_InteractableBase::OnEnd(EFXR_EndReason Reason)
{
	bHeld = false;
	BroadcastInteractionEvent(EFXR_InteractionPhase::Ended, nullptr);
}

FVector UFXR_InteractableBase::GetInteractionLocation() const
{
	if (const UPrimitiveComponent* Driven = ResolveDrivenComponent())
	{
		return Driven->GetComponentLocation();
	}
	return GetComponentLocation();
}

bool UFXR_InteractableBase::IsInGrabReach(const FVector& GrabCenter, float GrabRadius, EFXR_HandSide HandSide, float& OutDistanceSq) const
{
	// Presence is the switch (ADR-007): owned grip points are the only grab surface; the mesh
	// path runs only when the interactable owns none (procedural grip on the collision).
	if (HasOwnedGripPoints())
	{
		bool bInReach = false;
		OutDistanceSq = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<UFXR_GripPoint>& WeakPoint : OwnedGripPoints)
		{
			const UFXR_GripPoint* Point = WeakPoint.Get();
			if (!Point || !Point->AcceptsHand(HandSide))
			{
				continue;
			}
			const float DistanceSq = FVector::DistSquared(GrabCenter, Point->GetComponentLocation());
			const float Reach = Point->GetActivationRadius() + GrabRadius;
			if (DistanceSq <= FMath::Square(Reach) && DistanceSq < OutDistanceSq)
			{
				OutDistanceSq = DistanceSq;
				bInReach = true;
			}
		}
		return bInReach;
	}

	const float Reach = ActivationRadius + GrabRadius;
	OutDistanceSq = FVector::DistSquared(GrabCenter, GetInteractionLocation());
	return OutDistanceSq <= FMath::Square(Reach);
}

void UFXR_InteractableBase::RegisterGripPoint(UFXR_GripPoint* GripPoint)
{
	if (GripPoint)
	{
		OwnedGripPoints.AddUnique(GripPoint);
	}
}

void UFXR_InteractableBase::UnregisterGripPoint(UFXR_GripPoint* GripPoint)
{
	OwnedGripPoints.RemoveSingleSwap(GripPoint);
}

UFXR_GripPoint* UFXR_InteractableBase::SelectGripPoint(IFXR_Interactor* Interactor) const
{
	if (!Interactor || OwnedGripPoints.Num() == 0)
	{
		return nullptr;
	}

	const EFXR_HandSide Side = Interactor->GetHandSide();

	// A grip point is in reach when the hand's grab sphere overlaps its activation sphere —
	// same convention as the detection broad phase (Reach = point radius + grab radius).
	FVector GrabCenter;
	float GrabRadius = 0.f;
	Interactor->GetGrabSphere(GrabCenter, GrabRadius);

	UFXR_GripPoint* Best = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Min();
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<UFXR_GripPoint>& WeakPoint : OwnedGripPoints)
	{
		UFXR_GripPoint* Point = WeakPoint.Get();
		if (!Point || !Point->AcceptsHand(Side))
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(GrabCenter, Point->GetComponentLocation());
		const float Reach = Point->GetActivationRadius() + GrabRadius;
		if (DistanceSq > FMath::Square(Reach))
		{
			continue;
		}
		if (Point->GetPriority() > BestPriority ||
			(Point->GetPriority() == BestPriority && DistanceSq < BestDistanceSq))
		{
			Best = Point;
			BestPriority = Point->GetPriority();
			BestDistanceSq = DistanceSq;
		}
	}

	return Best;
}

UPrimitiveComponent* UFXR_InteractableBase::ResolveDrivenComponent() const
{
	if (UPrimitiveComponent* AttachPrimitive = Cast<UPrimitiveComponent>(GetAttachParent()))
	{
		return AttachPrimitive;
	}
	if (const AActor* OwnerActor = GetOwner())
	{
		return Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	}
	return nullptr;
}

void UFXR_InteractableBase::BroadcastInteractionEvent(EFXR_InteractionPhase Phase, IFXR_Interactor* Interactor)
{
	if (!bExposeToTraining || InteractionId.IsNone())
	{
		return;
	}

	if (UFXR_EventBus* EventBus = UFXR_EventBus::Get(this))
	{
		FFXR_InteractionEvent Event;
		Event.InteractionId = InteractionId;
		Event.Phase = Phase;
		Event.HandSide = Interactor ? Interactor->GetHandSide() : EFXR_HandSide::Right;
		Event.Instigator = GetOwner();
		EventBus->Broadcast(Event);
	}
}
