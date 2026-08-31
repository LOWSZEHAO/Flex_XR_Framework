// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Socket.h"
#include "Interactable/FXR_Grab.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

UFXR_Socket::UFXR_Socket()
{
	// Able to tick, but not ticking: it wakes only while the seat animation or the ghost fade is
	// running, so a level full of idle sockets costs nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

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
	// Eased into place rather than teleported: the seat pose is where it lands, not how it gets there.
	SeatStart = Driven->GetComponentTransform();
	SeatTarget = GetSeatTransform(Driven);
	SeatElapsed = 0.f;
	bSeating = SeatDuration > KINDA_SMALL_NUMBER;
	if (!bSeating)
	{
		Driven->SetWorldTransform(SeatTarget, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Hand the pre-seat physics state over, or a later grab reads the parked body and the object can
	// never fall again once released.
	Object->NotifyParkedPhysics(bSocketedPhysics);

	Socketed = Object;
	Seating = Object;
	EndPreview();
	RefreshTickState();

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

	// One instance so the fade can be driven per frame without touching the shared material.
	if (!GhostMID)
	{
		GhostMID = UMaterialInstanceDynamic::Create(GhostMaterial, this);
	}
	if (GhostMID)
	{
		for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
		{
			Ghost->SetMaterial(Slot, GhostMID);
		}
	}

	// Drawn at the seat pose the object will actually take, scale included, so the preview is
	// literally where it lands rather than an approximation of it.
	Ghost->SetWorldTransform(GetSeatTransform(Source));

	GhostTarget = 1.f;
	ApplyGhostAlpha();
	RefreshTickState();
}

void UFXR_Socket::HideGhost()
{
	// Faded out rather than switched off; the tick hides it once it reaches zero.
	GhostTarget = 0.f;
	RefreshTickState();
}

FTransform UFXR_Socket::GetSeatTransform(const UPrimitiveComponent* Driven) const
{
	FTransform Seat = GetComponentTransform();

	// Position and facing come from the socket; scale stays the object's own. Taking the socket's
	// scale would resize whatever it receives, so an object deliberately scaled in the level would
	// snap back to its default size the moment it docked.
	if (Driven)
	{
		Seat.SetScale3D(Driven->GetComponentScale());
	}
	return Seat;
}

void UFXR_Socket::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSeating)
	{
		UFXR_Grab* Object = Seating.Get();
		UPrimitiveComponent* Driven = Object ? Object->GetDrivenComponent() : nullptr;
		if (!Driven || Object->IsHeld())
		{
			// Grabbed straight back out of the seat animation — stop driving it.
			bSeating = false;
		}
		else
		{
			SeatElapsed += DeltaTime;
			const float Alpha = FMath::Clamp(SeatElapsed / FMath::Max(SeatDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

			// Eased so it settles into the mount instead of arriving at full speed.
			FTransform Current;
			Current.Blend(SeatStart, SeatTarget, FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f));
			Driven->SetWorldTransform(Current, false, nullptr, ETeleportType::TeleportPhysics);

			if (Alpha >= 1.f)
			{
				bSeating = false;
				Seating = nullptr;
			}
		}
	}

	// Ghost fades rather than blinking on: a preview that pops reads as a rendering fault.
	if (!FMath::IsNearlyEqual(GhostAlpha, GhostTarget))
	{
		const float Step = DeltaTime / FMath::Max(GhostFadeTime, KINDA_SMALL_NUMBER);
		GhostAlpha = FMath::FInterpConstantTo(GhostAlpha, GhostTarget, 1.f, Step);
		ApplyGhostAlpha();
	}

	RefreshTickState();
}

void UFXR_Socket::RefreshTickState()
{
	// Ticks only while something is actually moving — a socket sitting idle costs nothing.
	const bool bBusy = bSeating || !FMath::IsNearlyEqual(GhostAlpha, GhostTarget);
	SetComponentTickEnabled(bBusy);
}

void UFXR_Socket::ApplyGhostAlpha()
{
	if (!Ghost)
	{
		return;
	}

	if (GhostMID)
	{
		GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), FMath::SmoothStep(0.f, 1.f, GhostAlpha));
	}
	// Hidden outright at zero so a fully faded ghost costs no draw call.
	Ghost->SetVisibility(GhostAlpha > KINDA_SMALL_NUMBER);
}
