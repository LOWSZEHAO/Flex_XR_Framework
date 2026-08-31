// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Driver/FXR_InteractionDriver.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Detection/FXR_FocusSubsystem.h"
#include "Highlight/FXR_HighlightSubsystem.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Interactable/FXR_Grab.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Interactable/FXR_Press.h"
#include "Interactable/FXR_RayTarget.h"
#include "Interactable/FXR_Socket.h"
#include "Interactor/FXR_Interactor.h"
#include "Rig/FXR_Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UFXR_InteractionDriver::UFXR_InteractionDriver()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Ship with the plugin so a bare driver draws a pointer. The tube and ring are the same meshes
	// the teleport arc uses — a beam is a stretched tube, and the endpoint marker is the same ring.
	RayMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/FlexXR/Meshes/SM_FXR_ArcSegment.SM_FXR_ArcSegment")));
	RayCursorMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/FlexXR/Meshes/SM_FXR_Reticle.SM_FXR_Reticle")));
	RayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_Ray.M_FXR_Ray")));
}

void UFXR_InteractionDriver::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_InteractionDriver_Tick);

	// One trace per hand, before anything reads it: DriveHand may claim a summonable object with it
	// and PublishFocus hovers from it, and tracing per consumer adds up on a Quest frame budget.
	UpdateFarHits();

	// Sampled before DriveHand consumes the edge, so a ray selection can see the same press.
	const float LeftSelect = ReadSelect(EFXR_HandSide::Left);
	const float RightSelect = ReadSelect(EFXR_HandSide::Right);
	const float LeftPrev = LeftPrevSelect;
	const float RightPrev = RightPrevSelect;

	DriveHand(EFXR_HandSide::Left, LeftHeld, LeftPrevSelect, DeltaTime);
	DriveHand(EFXR_HandSide::Right, RightHeld, RightPrevSelect, DeltaTime);
	DrivePokes(EFXR_HandSide::Left);
	DrivePokes(EFXR_HandSide::Right);
	DriveSockets(EFXR_HandSide::Left, LeftPreviewSocket);
	DriveSockets(EFXR_HandSide::Right, RightPreviewSocket);

	// After the hands are driven, so a grab claimed this frame publishes as Selected rather than
	// spending a frame as Hovered first.
	PublishFocus(EFXR_HandSide::Left, LeftHeld, LeftAimed, LeftSelect, LeftPrev, DeltaTime);
	PublishFocus(EFXR_HandSide::Right, RightHeld, RightAimed, RightSelect, RightPrev, DeltaTime);
}

void UFXR_InteractionDriver::PublishFocus(EFXR_HandSide Side, const TWeakObjectPtr<UFXR_InteractableBase>& Held,
	TWeakObjectPtr<UFXR_RayTarget>& Aimed, float Select, float PrevSelect, float DeltaTime)
{
	UFXR_FocusSubsystem* Focus = UFXR_FocusSubsystem::Get(this);
	if (!Focus)
	{
		return;
	}

	UFXR_InteractableBase* HeldNow = Held.Get();
	Focus->SetSelected(Side, HeldNow);

	// A hand that owns something is not shopping for the next thing: holding a valve must not keep
	// the crate behind it lit. Hover resolves only for a free hand.
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this);

	UFXR_InteractableBase* Near = nullptr;
	if (!HeldNow && Interactor && Subsystem)
	{
		// The same query the grab claim uses, run every frame instead of only on the rising edge —
		// which is what makes the object that lights up provably the object you would take.
		FVector GrabCenter;
		float GrabRadius = 0.f;
		Interactor->GetGrabSphere(GrabCenter, GrabRadius);
		Near = Subsystem->FindBestCandidate(GrabCenter, GrabRadius, Side);
	}

	DriveProximity(Side, HeldNow != nullptr, Near, Interactor, Subsystem);

	// Far yields to near. A hand already able to touch something must not also be pointing past it,
	// or reaching for a valve would arm a selection on the wall behind it.
	UFXR_InteractableBase* Far = (HeldNow || Near) ? nullptr : ResolveFarTarget(Side);
	UFXR_RayTarget* RayNow = Cast<UFXR_RayTarget>(Far);
	UpdateAimed(Side, Aimed, RayNow);

	// A distance-grabbable object hovers from range too, so what lights up is what the press takes —
	// the same promise near hover makes.
	Focus->SetHovered(Side, Near ? Near : Far);

	// Same rising edge a grab would claim on, so pointing and pressing reads identically to reaching
	// and squeezing.
	if (RayNow && Select >= GrabThreshold && PrevSelect < GrabThreshold)
	{
		RayNow->NotifyRaySelected(Side);
	}

	// Drawn from the same answers the logic just used, so the beam can never disagree with what a
	// press would actually do.
	DriveRayVisual(Side, HeldNow != nullptr || Near != nullptr, Far, DeltaTime);
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
			// A socket that was previewing this object claims it instead of the floor. Read before
			// the release, because ending the hold is what clears the preview.
			TWeakObjectPtr<UFXR_Socket>& PreviewSocket = (Side == EFXR_HandSide::Left) ? LeftPreviewSocket : RightPreviewSocket;
			UFXR_Socket* Receiver = PreviewSocket.Get();

			// Role-aware: a two-hand hold detaches just this hand (or promotes the survivor);
			// single-hand ends the interaction. The interactable decides — the driver has no roles.
			Current->ReleaseHand(Interactor, EFXR_EndReason::Released);
			Held = nullptr;

			// Only once the object is actually free: a hand still on it (the other half of a
			// two-hand carry) means it was not really let go.
			if (Receiver && !Current->IsHeld())
			{
				Receiver->Seat(Cast<UFXR_Grab>(Current));
			}
			PreviewSocket = nullptr;
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
					return;
				}
			}
		}

		// Nothing in reach: fall back to what this hand is pointing at, if that object opts into being
		// summoned. Tried only after the near query, so an object in the hand always wins.
		if (UFXR_Grab* FarGrab = TraceDistanceGrab(Side))
		{
			if (FarGrab->CanBegin(Interactor))
			{
				FarGrab->BeginDistanceGrab(Interactor);
				Held = FarGrab;
			}
		}
	}
}

void UFXR_InteractionDriver::UpdateFarHits()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_InteractionDriver_FarRays);

	bLeftFarHit = CastFarRay(EFXR_HandSide::Left, LeftFarHit);
	bRightFarHit = CastFarRay(EFXR_HandSide::Right, RightFarHit);
}

bool UFXR_InteractionDriver::CastFarRay(EFXR_HandSide Side, FHitResult& OutHit) const
{
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	const UWorld* World = GetWorld();
	if (!Interactor || !World)
	{
		return false;
	}

	FVector Origin;
	FVector Direction;
	Interactor->GetFarRay(Origin, Direction);
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	// The one custom channel (ADR-002). It blocks by default, so ordinary world geometry occludes
	// the ray and no per-object collision setup is needed to make a target hittable.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FXR_FarRay), /*bTraceComplex=*/false, GetOwner());
	return World->LineTraceSingleByChannel(OutHit, Origin, Origin + Direction.GetSafeNormal() * RayLength, FXR_TraceChannel, Params);
}

bool UFXR_InteractionDriver::GetFarHit(EFXR_HandSide Side, FHitResult& OutHit) const
{
	const bool bLeft = (Side == EFXR_HandSide::Left);
	OutHit = bLeft ? LeftFarHit : RightFarHit;
	return bLeft ? bLeftFarHit : bRightFarHit;
}

UFXR_InteractableBase* UFXR_InteractionDriver::ResolveFarTarget(EFXR_HandSide Side) const
{
	// A RayTarget wins where an object has both: it is the deliberate "point at me" marker, whereas
	// distance grab is a property an object merely permits.
	if (UFXR_RayTarget* Target = TraceRayTarget(Side))
	{
		return Target;
	}
	return TraceDistanceGrab(Side);
}

UFXR_RayTarget* UFXR_InteractionDriver::TraceRayTarget(EFXR_HandSide Side) const
{
	FHitResult Hit;
	if (!GetFarHit(Side, Hit))
	{
		return nullptr;
	}

	const AActor* HitActor = Hit.GetActor();
	UFXR_RayTarget* Target = HitActor ? HitActor->FindComponentByClass<UFXR_RayTarget>() : nullptr;
	if (!Target || !Target->IsInteractionEnabled())
	{
		return nullptr;
	}

	// Its own shorter reach, so a small panel can be pointed at across a room only if it says so.
	return (Hit.Distance <= Target->MaxRayDistance) ? Target : nullptr;
}

UFXR_Grab* UFXR_InteractionDriver::TraceDistanceGrab(EFXR_HandSide Side) const
{
	FHitResult Hit;
	if (!GetFarHit(Side, Hit))
	{
		return nullptr;
	}

	const AActor* HitActor = Hit.GetActor();
	UFXR_Grab* Grab = HitActor ? HitActor->FindComponentByClass<UFXR_Grab>() : nullptr;
	if (!Grab || !Grab->AllowsDistanceGrab() || !Grab->IsInteractionEnabled() || Grab->IsHeld())
	{
		return nullptr;
	}
	return Grab;
}

void UFXR_InteractionDriver::UpdateAimed(EFXR_HandSide Side, TWeakObjectPtr<UFXR_RayTarget>& Aimed, UFXR_RayTarget* Now)
{
	UFXR_RayTarget* Was = Aimed.Get();
	if (Was == Now)
	{
		return;
	}

	// Exit before enter, so a listener that shares state between two targets sees a clean handover.
	if (Was)
	{
		Was->NotifyRayExit(Side);
	}
	if (Now)
	{
		Now->NotifyRayEnter(Side);
	}
	Aimed = Now;
}

UFXR_RayTarget* UFXR_InteractionDriver::GetAimedRayTarget(EFXR_HandSide Side) const
{
	return (Side == EFXR_HandSide::Left) ? LeftAimed.Get() : RightAimed.Get();
}

float UFXR_InteractionDriver::ReadSelect(EFXR_HandSide Side) const
{
	const IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	return Interactor ? Interactor->GetSelectValue() : 0.f;
}

void UFXR_InteractionDriver::DriveSockets(EFXR_HandSide Side, TWeakObjectPtr<UFXR_Socket>& PreviewSocket)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_InteractionDriver_Sockets);

	UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	UFXR_Grab* Carried = Cast<UFXR_Grab>(GetHeldInteractable(Side));

	// Nearest accepting socket wins, so two mounts side by side resolve to the one you are actually
	// reaching toward rather than whichever happens to be first in the registry.
	UFXR_Socket* Best = nullptr;
	float BestDistanceSq = MAX_flt;

	// Linear pass over the registry (first-pass detection, design 5.4), matching the poke pass.
	for (const TObjectPtr<UFXR_InteractableBase>& Interactable : Subsystem->GetRegistered())
	{
		UFXR_Socket* Socket = Cast<UFXR_Socket>(Interactable.Get());
		if (!Socket)
		{
			continue;
		}

		// Done here rather than on a socket tick: this pass already walks every socket each frame,
		// and it keeps a socket free of per-object ticking.
		if (Side == EFXR_HandSide::Left)
		{
			Socket->RefreshOccupancy();
		}

		float DistanceSq = 0.f;
		if (Carried && Socket->CanAccept(Carried, DistanceSq) && DistanceSq < BestDistanceSq)
		{
			Best = Socket;
			BestDistanceSq = DistanceSq;
		}
	}

	if (PreviewSocket.Get() != Best)
	{
		if (UFXR_Socket* Previous = PreviewSocket.Get())
		{
			Previous->EndPreview();
		}
		if (Best)
		{
			Best->BeginPreview(Carried);
		}
		PreviewSocket = Best;
	}

	// A magnetic socket takes it out of the hand the moment it qualifies, rather than waiting to be
	// let go over it.
	if (Best && Best->IsAutoSnap())
	{
		Best->Seat(Carried);
		PreviewSocket = nullptr;
	}
}

void UFXR_InteractionDriver::DriveProximity(EFXR_HandSide Side, bool bHolding, UFXR_InteractableBase* Near,
	IFXR_Interactor* Interactor, UFXR_InteractionSubsystem* Subsystem)
{
	UFXR_HighlightSubsystem* Highlight = UFXR_HighlightSubsystem::Get(this);
	if (!Highlight)
	{
		return;
	}

	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();

	// A hand that is holding something is not approaching anything, and an object already in reach
	// is Hovered outright — the ramp only covers the stretch before that.
	if (bHolding || Near || !Interactor || !Subsystem || !Settings || !Settings->bProximityHighlight)
	{
		Highlight->SetProximity(Side, nullptr, 0.f);
		return;
	}

	FVector GrabCenter;
	float GrabRadius = 0.f;
	Interactor->GetGrabSphere(GrabCenter, GrabRadius);

	// The same detection query, widened. Nothing new is invented for the ramp: what glows on approach
	// is exactly what would become the candidate if the hand kept going.
	const float Reach = GrabRadius + FMath::Max(Settings->ProximityRange, 0.f);
	UFXR_InteractableBase* Approaching = Subsystem->FindBestCandidate(GrabCenter, Reach, Side);
	if (!Approaching)
	{
		Highlight->SetProximity(Side, nullptr, 0.f);
		return;
	}

	const float Distance = FVector::Dist(GrabCenter, Approaching->GetInteractionLocation());
	const float Span = FMath::Max(Reach - GrabRadius, KINDA_SMALL_NUMBER);
	const float Closeness = 1.f - FMath::Clamp((Distance - GrabRadius) / Span, 0.f, 1.f);

	Highlight->SetProximity(Side, Approaching, Closeness * Settings->ProximityMaxAlpha);
}

void UFXR_InteractionDriver::DriveRayVisual(EFXR_HandSide Side, bool bBusyNear, const UFXR_InteractableBase* FarTarget, float DeltaTime)
{
	FRayVisual& Visual = RayVisuals[Side == EFXR_HandSide::Left ? 0 : 1];

	// Shown while a hand is free and not already reaching for something. A beam that is always on
	// says nothing; one that appears when far interaction becomes this hand's job does.
	IFXR_Interactor* Interactor = GetActiveInteractor(Side);
	const bool bWanted = bShowRay && Interactor && !bBusyNear;

	const float Step = (RayFadeTime > KINDA_SMALL_NUMBER) ? (DeltaTime / RayFadeTime) : 1.f;
	Visual.Alpha = FMath::FInterpConstantTo(Visual.Alpha, bWanted ? 1.f : 0.f, 1.f, Step);

	if (Visual.Alpha <= KINDA_SMALL_NUMBER)
	{
		if (UStaticMeshComponent* Beam = Visual.Beam.Get())
		{
			Beam->SetVisibility(false);
		}
		if (UStaticMeshComponent* Cursor = Visual.Cursor.Get())
		{
			Cursor->SetVisibility(false);
		}
		return;
	}

	AActor* Owner = GetOwner();
	UStaticMesh* BeamMesh = RayMesh.LoadSynchronous();
	UMaterialInterface* Material = RayMaterial.LoadSynchronous();
	if (!Owner || !BeamMesh || !Material)
	{
		return; // cleared on purpose draws nothing; the ray itself still works
	}

	// Built on first use, like the locomotion arc — a rig that never points costs nothing.
	if (!Visual.Beam.IsValid())
	{
		UStaticMeshComponent* Beam = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Beam->SetupAttachment(Owner->GetRootComponent());
		// Movable, and absolute. A component built at runtime defaults to Static mobility, which
		// silently discards every transform update — the beam was being aimed each frame and ignoring
		// it. Absolute keeps the rig own scale out of a thickness measured in centimetres.
		Beam->SetMobility(EComponentMobility::Movable);
		Beam->SetAbsolute(true, true, true);
		Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Beam->SetCastShadow(false);
		Beam->SetStaticMesh(BeamMesh);
		Beam->RegisterComponent();
		Visual.Beam = Beam;

		Beam->SetMaterial(0, Material);
	}

	if (UStaticMesh* CursorMesh = RayCursorMesh.LoadSynchronous())
	{
		if (!Visual.Cursor.IsValid())
		{
			UStaticMeshComponent* Cursor = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			Cursor->SetupAttachment(Owner->GetRootComponent());
			Cursor->SetMobility(EComponentMobility::Movable);
			Cursor->SetAbsolute(true, true, true);
			Cursor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Cursor->SetCastShadow(false);
			Cursor->SetStaticMesh(CursorMesh);
			Cursor->RegisterComponent();
			Cursor->SetMaterial(0, Material);
			Visual.Cursor = Cursor;
		}
	}

	FVector Origin;
	FVector Direction;
	Interactor->GetFarRay(Origin, Direction);
	Direction = Direction.GetSafeNormal();

	// Stops at whatever the ray actually hit, so the beam reads as touching the world rather than
	// passing through it. The same cached hit the claim and the hover use — one trace, one truth.
	FHitResult Hit;
	const bool bHit = GetFarHit(Side, Hit);
	const float Length = bHit ? Hit.Distance : RayLength;

	if (UStaticMeshComponent* Beam = Visual.Beam.Get())
	{
		// Both scales come from the mesh own bounds rather than assuming how it was authored. The
		// tube runs along +X, so X stretches it to reach and Y/Z set a real centimetre thickness, which
		// is what lets Ray Width be honestly documented as centimetres.
		const FVector MeshExtent = BeamMesh->GetBounds().BoxExtent;
		const float MeshLength = FMath::Max(MeshExtent.X * 2.f, KINDA_SMALL_NUMBER);
		const float MeshRadius = FMath::Max(MeshExtent.Y, KINDA_SMALL_NUMBER);
		// The fade is geometric: an opaque beam cannot fade its opacity, so it thins to nothing
		// instead. On a tube this narrow that reads as the beam retracting rather than dissolving,
		// which is the better motion anyway — nothing pops on or off.
		const float Eased = FMath::SmoothStep(0.f, 1.f, Visual.Alpha);
		const float Thickness = (RayWidth * 0.5f * Eased) / MeshRadius;

		Beam->SetWorldLocationAndRotation(Origin, Direction.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		Beam->SetWorldScale3D(FVector(Length / MeshLength, Thickness, Thickness));
		Beam->SetVisibility(true);
	}

	const float CursorEased = FMath::SmoothStep(0.f, 1.f, Visual.Alpha);
	if (UStaticMeshComponent* Cursor = Visual.Cursor.Get())
	{
		// Only on something worth pointing at: the beam says where you are aiming, the cursor says
		// that what you are aiming at will answer.
		const bool bOnTarget = FarTarget != nullptr;
		Cursor->SetVisibility(bOnTarget);
		if (bOnTarget)
		{
			Cursor->SetWorldScale3D(FVector(CursorEased));
			Cursor->SetWorldLocation(Origin + Direction * Length, false, nullptr, ETeleportType::TeleportPhysics);
			Cursor->SetWorldRotation((-Direction).Rotation());
		}
	}

}
