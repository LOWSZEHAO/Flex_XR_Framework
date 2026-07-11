// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_InteractableBase.h"
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
			DrawDebugSphere(World, GetInteractionLocation(), ActivationRadius, 16, Color, false, -1.f, 0, 0.5f);
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
	// TODO(FXR_Interaction): Already-Held policy (FinishNaturally / ForceRelease) when disabled mid-hold.
	bInteractionEnabled = bEnabled;
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
