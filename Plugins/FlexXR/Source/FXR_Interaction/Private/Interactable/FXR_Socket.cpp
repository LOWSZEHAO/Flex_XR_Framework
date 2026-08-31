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
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"

UFXR_Socket::UFXR_Socket()
{
	// Starts ticking and then switches itself off on the first tick if there is nothing to do, rather
	// than starting disabled. Registering with the tick disabled leaves it that way — enabling it
	// afterwards does not stick — which is what kept the editor debug from ever drawing.
	PrimaryComponentTick.bCanEverTick = true;

	// A socket is placed by eye, so its reach and facing have to be visible while authoring rather
	// than only once the game is running.
	bTickInEditor = true;

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

	// The same set the hold would park, not just the driven mesh: park less and a loose part keeps
	// simulating and falls off whatever it was docked to.
	Object->ParkPhysics();
	// Eased into place rather than teleported: the seat pose is where it lands, not how it gets there.
	SeatStart = Object->GetHeldTransform();
	SeatTarget = GetSeatTransform(Object);
	SeatElapsed = 0.f;
	bSeating = SeatDuration > KINDA_SMALL_NUMBER;
	if (!bSeating)
	{
		Object->SetHeldTransform(SeatTarget);
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

	// Nobody is holding it, so this socket hands back exactly what it parked.
	Object->RestorePhysics();

	BroadcastInteractionEvent(EFXR_InteractionPhase::Ended, nullptr);
	OnRemoved.Broadcast(Object);
}


FTransform UFXR_Socket::GetSeatTransform(const UFXR_Grab* Object) const
{
	FTransform Seat = GetComponentTransform();

	// Position and facing come from the socket; scale stays the object's own. Taking the socket's
	// scale would resize whatever it receives, so an object deliberately scaled in the level would
	// snap back to its default size the moment it docked. With no object — an Always-mode ghost —
	// unit scale, for the same reason: a scaled socket must not resize what it advertises.
	Seat.SetScale3D(Object ? Object->GetHeldTransform().GetScale3D() : FVector::OneVector);
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
			Object->SetHeldTransform(Current);

			if (Alpha >= 1.f)
			{
				bSeating = false;
				Seating = nullptr;
			}
		}
	}

	// Outside play the ghost is rebuilt each tick so it stays glued to the socket while it is dragged
	// around the viewport, which is the whole point of showing it at author time.
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		RefreshGhost();
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
	// One instance drives every piece, so the whole preview fades as a single object rather than
	// each mesh arriving on its own.
	if (GhostMID)
	{
		GhostMID->SetScalarParameterValue(TEXT("GhostOpacity"), FMath::SmoothStep(0.f, 1.f, GhostAlpha));
	}

	// Hidden outright at zero so a fully faded ghost costs no draw calls.
	const bool bGhostVisible = GhostAlpha > KINDA_SMALL_NUMBER;
	for (UStaticMeshComponent* Part : GhostParts)
	{
		if (Part)
		{
			Part->SetVisibility(bGhostVisible);
		}
	}
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

	// Takes effect as Debug Draw or the Ghost Actor changes, rather than at the next BeginPlay.
	RefreshTickState();
	RefreshGhost();
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


bool UFXR_Socket::ShouldGhost(const UStaticMeshComponent* Mesh) const
{
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return false;
	}

	// Already hidden on the real object, so it has no business appearing on the preview. This covers
	// most of what would otherwise need tagging: collision proxies, spawners, editor-only markers.
	if (!Mesh->IsVisible())
	{
		return false;
	}

	for (const FName& Tag : GhostIgnoreTags)
	{
		if (Mesh->ComponentHasTag(Tag))
		{
			return false;
		}
	}
	return true;
}

void UFXR_Socket::GatherFromActor(const UFXR_Grab* Object, TArray<FGhostPart>& OutParts) const
{
	const AActor* Actor = Object ? Object->GetOwner() : nullptr;
	if (!Actor)
	{
		return;
	}

	// Relative to what the hold actually moves, so the preview sits exactly where the object lands.
	const FTransform HeldFrame = Object->GetHeldTransform();

	TArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents<UStaticMeshComponent>(Meshes);
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		// Only what travels with the hold: a preview showing parts that will stay behind is a lie.
		if (!ShouldGhost(Mesh) || !Object->MovesComponent(Mesh))
		{
			continue;
		}
		OutParts.Add({ Mesh->GetStaticMesh(), Mesh->GetComponentTransform().GetRelativeTransform(HeldFrame) });
	}
}

void UFXR_Socket::GatherFromClass(UClass* Class, TArray<FGhostPart>& OutParts) const
{
	const AActor* CDO = Class ? Cast<AActor>(Class->GetDefaultObject()) : nullptr;
	if (!CDO)
	{
		return;
	}

	// Read from the class rather than spawning one: a real extinguisher would run its construction
	// script, register interactables and start simulating, all to draw a translucent shape.
	const FTransform RootToActor = CDO->GetRootComponent()
		? CDO->GetRootComponent()->GetRelativeTransform()
		: FTransform::Identity;

	TArray<UStaticMeshComponent*> Native;
	CDO->GetComponents<UStaticMeshComponent>(Native);
	for (const UStaticMeshComponent* Mesh : Native)
	{
		if (ShouldGhost(Mesh))
		{
			OutParts.Add({ Mesh->GetStaticMesh(), Mesh->GetRelativeTransform() });
		}
	}

	// Blueprint-added components never exist on the CDO — they live in the construction script, and
	// for most props that is where every mesh actually is. Walked from the roots so each one's
	// transform accumulates down its attachment chain.
	for (UClass* Current = Class; Current; Current = Current->GetSuperClass())
	{
		const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Current);
		const USimpleConstructionScript* Script = Generated ? Generated->SimpleConstructionScript : nullptr;
		if (!Script)
		{
			continue;
		}

		TArray<TPair<const USCS_Node*, FTransform>> Pending;
		for (const USCS_Node* Root : Script->GetRootNodes())
		{
			Pending.Add({ Root, FTransform::Identity });
		}

		while (Pending.Num() > 0)
		{
			const TPair<const USCS_Node*, FTransform> Entry = Pending.Pop();
			const USCS_Node* Node = Entry.Key;
			if (!Node)
			{
				continue;
			}

			FTransform NodeToActor = Entry.Value;
			if (const USceneComponent* Scene = Cast<USceneComponent>(Node->ComponentTemplate))
			{
				NodeToActor = Scene->GetRelativeTransform() * Entry.Value;
			}

			if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Node->ComponentTemplate))
			{
				if (ShouldGhost(Mesh))
				{
					OutParts.Add({ Mesh->GetStaticMesh(), NodeToActor });
				}
			}

			for (const USCS_Node* Child : Node->GetChildNodes())
			{
				Pending.Add({ Child, NodeToActor });
			}
		}
	}
}

void UFXR_Socket::GatherGhostParts(const UFXR_Grab* Approaching, TArray<FGhostPart>& OutParts) const
{
	OutParts.Reset();

	// The carried object itself when there is one, so the preview is literally the thing being
	// placed — including anything its construction script built, which a class read cannot see.
	if (Approaching && Approaching->GetOwner())
	{
		GatherFromActor(Approaching, OutParts);
		if (OutParts.Num() > 0)
		{
			return;
		}
	}

	GatherFromClass(GhostActor.LoadSynchronous(), OutParts);
}

void UFXR_Socket::ResizeGhostPool(int32 Count)
{
	while (GhostParts.Num() > Count)
	{
		if (UStaticMeshComponent* Spare = GhostParts.Pop())
		{
			Spare->DestroyComponent();
		}
	}

	while (GhostParts.Num() < Count)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Part->SetupAttachment(this);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);

		// Flagged as a visualiser so the editor keeps it out of selection, serialisation and anything
		// that walks an actor's real components — it exists to be looked at, nothing more.
		Part->SetIsVisualizationComponent(true);
		Part->RegisterComponent();
		GhostParts.Add(Part);
	}
}

void UFXR_Socket::RefreshGhost()
{
	const UWorld* World = GetWorld();
	const bool bPlaying = World && World->IsGameWorld();

	// Outside play the ghost is an authoring aid, drawn with Debug Draw so the seat pose can be placed
	// against the object that will actually sit there rather than by guesswork. Nothing is being
	// carried then, so the shape comes from the Ghost Actor.
	const bool bAuthoring = World && !bPlaying && IsDrawDebugEnabled() && !GhostActor.IsNull();

	UFXR_Grab* Approaching = bPlaying ? Preview.Get() : nullptr;
	const bool bWanted = bAuthoring ||
		(bPlaying && GhostMode != EFXR_SocketGhostMode::Off &&
			(GhostMode == EFXR_SocketGhostMode::Always
				? (IsInteractionEnabled() && !Socketed.IsValid())
				: Approaching != nullptr));

	TArray<FGhostPart> Parts;
	UMaterialInterface* GhostSource = bWanted ? GhostMaterial.LoadSynchronous() : nullptr;
	if (bWanted && GhostSource)
	{
		GatherGhostParts(Approaching, Parts);
	}

	if (Parts.Num() == 0)
	{
		// Faded out rather than torn down, so the components survive to fade back in.
		GhostTarget = 0.f;
		RefreshTickState();
		return;
	}

	ResizeGhostPool(Parts.Num());

	// One instance shared by every piece: they fade together, as one object would.
	if (!GhostMID)
	{
		GhostMID = UMaterialInstanceDynamic::Create(GhostSource, this);
	}

	// The seat pose the object will actually take, with each piece placed relative to it exactly as
	// it sits on the real thing.
	const FTransform Seat = GetSeatTransform(Approaching);

	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		UStaticMeshComponent* Part = GhostParts[Index];
		Part->SetStaticMesh(Parts[Index].Mesh);
		Part->SetWorldTransform(Parts[Index].RelativeToRoot * Seat);
		if (GhostMID)
		{
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				Part->SetMaterial(Slot, GhostMID);
			}
		}
	}

	GhostTarget = 1.f;
	ApplyGhostAlpha();
	RefreshTickState();
}
