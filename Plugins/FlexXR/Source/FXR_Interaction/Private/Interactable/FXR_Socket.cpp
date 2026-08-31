// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Socket.h"
#include "Interactable/FXR_Grab.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

UFXR_Socket::UFXR_Socket()
{
	// Able to tick, but not ticking: it wakes only while the seat animation or the ghost fade is
	// running, so a level full of idle sockets costs nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Nothing here is a grab surface, so the base's activation radius plays no part; Socket Radius
	// is this component's reach instead.
	ActivationRadius = 1.f;

	// Ships with the plugin so a dropped-in socket previews without any material hunting. Referenced
	// by path rather than loaded here: a hard reference from this CDO roots the asset, which makes it
	// unrebuildable from tooling and loads a preview material into every project that never shows one.
	GhostMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_Ghost.M_FXR_Ghost")));
}

void UFXR_Socket::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RefreshGhost();
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
		RefreshGhost();
		OnHoverStart.Broadcast(Object);
	}
}

void UFXR_Socket::EndPreview()
{
	UFXR_Grab* Was = Preview.Get();
	Preview = nullptr;
	RefreshGhost();

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

		// Always mode advertises an empty socket, so emptying one has to bring the ghost back.
		RefreshGhost();
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
	// Debug draw keeps it ticking in the editor too, so the reach sphere and facing are visible while
	// placing the socket rather than only once the game is running.
	const bool bBusy = bSeating || !FMath::IsNearlyEqual(GhostAlpha, GhostTarget) || IsDrawDebugEnabled();
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

void UFXR_Socket::BeginPlay()
{
	Super::BeginPlay();

	// Always mode has to show without anything having happened yet.
	RefreshGhost();
}

void UFXR_Socket::OnRegister()
{
	Super::OnRegister();

	RefreshTickState();
}

#if WITH_EDITOR
void UFXR_Socket::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Takes effect as Debug Draw is changed, rather than at the next BeginPlay.
	RefreshTickState();
}
#endif

void UFXR_Socket::DrawInteractionDebug() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Same colour language the base uses: green occupied, amber free, red disabled.
	const FColor Color = Socketed.IsValid() ? FColor::Green
		: (IsInteractionEnabled() ? FColor::Orange : FColor::Red);

	const FVector Origin = GetComponentLocation();
	DrawDebugSphere(World, Origin, SocketRadius, 16, Color, false, -1.f, SDPG_World, 0.5f);

	// The seat facing, so a socket that requires alignment can be aimed by eye rather than by trial.
	DrawDebugDirectionalArrow(World, Origin, Origin + GetForwardVector() * SocketRadius,
		SocketRadius * 0.25f, Color, false, -1.f, SDPG_World, 0.75f);

	if (IsFullDebug() && bRequireAlignment)
	{
		// The tolerance itself, so "why won't it seat?" is answerable by looking.
		const float Angle = FMath::DegreesToRadians(AlignmentTolerance);
		DrawDebugCone(World, Origin, GetForwardVector(), SocketRadius, Angle, Angle, 16,
			FColor::Cyan, false, -1.f, SDPG_World, 0.5f);
	}
}

void UFXR_Socket::RefreshGhost()
{
	// Editor worlds get the debug shapes but no ghost: spawning transient components outside play is
	// a reliable way to corrupt a Blueprint's component list.
	const UWorld* World = GetWorld();
	const bool bPlaying = World && World->IsGameWorld();

	UFXR_Grab* Approaching = Preview.Get();
	const bool bWanted = bPlaying && GhostMode != EFXR_SocketGhostMode::Off &&
		(GhostMode == EFXR_SocketGhostMode::Always
			? (IsInteractionEnabled() && !Socketed.IsValid())
			: Approaching != nullptr);

	if (!bWanted)
	{
		GhostTarget = 0.f;
		RefreshTickState();
		return;
	}

	// The carried object's own shape when there is one — the preview is then literally the thing
	// being placed. Always mode has nothing to borrow, so it falls back to the authored mesh.
	const UStaticMeshComponent* Source = Cast<UStaticMeshComponent>(
		Approaching ? Approaching->GetDrivenComponent() : nullptr);
	UStaticMesh* Mesh = Source ? Source->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		Mesh = GhostMesh.LoadSynchronous();
	}
	if (!Mesh)
	{
		GhostTarget = 0.f;
		RefreshTickState();
		return;
	}

	UMaterialInterface* GhostSource = GhostMaterial.LoadSynchronous();
	if (!GhostSource)
	{
		GhostTarget = 0.f;
		RefreshTickState();
		return; // cleared on purpose disables the preview
	}

	if (!Ghost)
	{
		Ghost = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Ghost->SetupAttachment(this);
		Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ghost->SetCastShadow(false);
		Ghost->RegisterComponent();
	}

	Ghost->SetStaticMesh(Mesh);

	// One instance so the fade can be driven per frame without touching the shared material.
	if (!GhostMID)
	{
		GhostMID = UMaterialInstanceDynamic::Create(GhostSource, this);
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
