// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Socket.h"
#include "Interactable/FXR_Grab.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

UFXR_Socket::UFXR_Socket()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Nothing here is a grab surface, so the base's activation radius plays no part; Socket Radius
	// is this component's reach instead.
	ActivationRadius = 1.f;

	// Ships with the plugin so a dropped-in socket previews without any material hunting.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultGhost(TEXT("/FlexXR/Materials/M_FXR_Ghost.M_FXR_Ghost"));
	if (DefaultGhost.Succeeded())
	{
		GhostMaterial = DefaultGhost.Object;
	}
}

void UFXR_Socket::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideGhost();
	Super::EndPlay(EndPlayReason);
}

bool UFXR_Socket::CanAccept(const UFXR_Grab* Object, float& OutDistanceSq) const
{
	OutDistanceSq = MAX_flt;
	if (!Object || Socketed.IsValid() || !IsInteractionEnabled())
	{
		return false;
	}

	const UPrimitiveComponent* Driven = Object->GetDrivenComponent();
	const AActor* Owner = Object->GetOwner();
	if (!Driven || !Owner)
	{
		return false;
	}

	// Tag filter: empty means "anything", which suits a generic shelf and not a labelled mount.
	if (AcceptedTags.Num() > 0)
	{
		bool bTagged = false;
		for (const FName& Tag : AcceptedTags)
		{
			if (Owner->ActorHasTag(Tag))
			{
				bTagged = true;
				break;
			}
		}
		if (!bTagged)
		{
			return false;
		}
	}

	const float DistanceSq = FVector::DistSquared(Driven->GetComponentLocation(), GetComponentLocation());
	if (DistanceSq > FMath::Square(SocketRadius))
	{
		return false;
	}

	// Facing check, so a plug has to be presented the right way round rather than seating backwards.
	if (bRequireAlignment)
	{
		const float Cosine = FVector::DotProduct(Driven->GetForwardVector(), GetForwardVector());
		if (Cosine < FMath::Cos(FMath::DegreesToRadians(AlignmentTolerance)))
		{
			return false;
		}
	}

	OutDistanceSq = DistanceSq;
	return true;
}

void UFXR_Socket::BeginPreview(UFXR_Grab* Object)
{
	if (Preview.Get() == Object)
	{
		return;
	}

	Preview = Object;
	if (Object)
	{
		ShowGhost(Object);
		OnHoverStart.Broadcast(Object);
	}
}

void UFXR_Socket::EndPreview()
{
	UFXR_Grab* Was = Preview.Get();
	Preview = nullptr;
	HideGhost();

	if (Was)
	{
		OnHoverEnd.Broadcast(Was);
	}
}

void UFXR_Socket::Seat(UFXR_Grab* Object)
{
	if (!Object || Socketed.IsValid())
	{
		return;
	}

	UPrimitiveComponent* Driven = Object->GetDrivenComponent();
	if (!Driven)
	{
		return;
	}

	// Auto-snap takes it straight out of the hand; on release the hold has already ended.
	if (Object->IsHeld())
	{
		Object->ForceRelease();
	}

	bSocketedPhysics = Driven->IsSimulatingPhysics();
	if (bSocketedPhysics)
	{
		Driven->SetSimulatePhysics(false);
	}
	// The socket's own transform is the seat pose, so a designer places the component where the
	// object should end up rather than authoring an offset.
	Driven->SetWorldTransform(GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);

	// Hand the pre-seat physics state over, or a later grab reads the parked body and the object can
	// never fall again once released.
	Object->NotifyParkedPhysics(bSocketedPhysics);

	Socketed = Object;
	EndPreview();

	// Lock In takes the object out of reach entirely; Eject is the only way back.
	if (bLockIn)
	{
		Object->SetInteractionEnabled(false);
	}

	BroadcastInteractionEvent(EFXR_InteractionPhase::Began, nullptr);
	OnSocketed.Broadcast(Object);
}

void UFXR_Socket::RefreshOccupancy()
{
	UFXR_Grab* Object = Socketed.Get();
	if (!Object)
	{
		// Destroyed out from under us — drop the reference without claiming it was removed.
		Socketed = nullptr;
		return;
	}

	// Grabbed back out. The grab itself restores physics on its own release, using the state handed
	// over when it was seated.
	if (Object->IsHeld())
	{
		Socketed = nullptr;
		BroadcastInteractionEvent(EFXR_InteractionPhase::Ended, nullptr);
		OnRemoved.Broadcast(Object);
	}
}

void UFXR_Socket::Eject()
{
	UFXR_Grab* Object = Socketed.Get();
	Socketed = nullptr;
	if (!Object)
	{
		return;
	}

	if (bLockIn)
	{
		Object->SetInteractionEnabled(true);
	}

	// Nobody is holding it, so this socket puts physics back itself.
	if (UPrimitiveComponent* Driven = Object->GetDrivenComponent())
	{
		if (bSocketedPhysics)
		{
			Driven->SetSimulatePhysics(true);
		}
	}

	BroadcastInteractionEvent(EFXR_InteractionPhase::Ended, nullptr);
	OnRemoved.Broadcast(Object);
}

void UFXR_Socket::ShowGhost(const UFXR_Grab* Object)
{
	const UStaticMeshComponent* Source = Cast<UStaticMeshComponent>(Object ? Object->GetDrivenComponent() : nullptr);
	if (!bShowGhost || !GhostMaterial || !Source || !Source->GetStaticMesh())
	{
		return;
	}

	if (!Ghost)
	{
		Ghost = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Ghost->SetupAttachment(this);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->RegisterComponent();
	}

	Ghost->SetStaticMesh(Source->GetStaticMesh());
	for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
	{
		Ghost->SetMaterial(Slot, GhostMaterial);
	}
	// Drawn at the seat pose, scaled like the object, so the preview is literally where it lands.
	Ghost->SetWorldTransform(GetComponentTransform());
	Ghost->SetWorldScale3D(Source->GetComponentScale());
	Ghost->SetVisibility(true);
}

void UFXR_Socket::HideGhost()
{
	if (Ghost)
	{
		Ghost->SetVisibility(false);
	}
}
