// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Hand/FXR_HandVisual.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Types/FXR_LogChannels.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "GameMapsSettings.h"
#include "DrawDebugHelpers.h"

UFXR_HandPose* UFXR_GripPoint::GetHandPose() const
{
	return HandPose;
}

UFXR_GripPoint::UFXR_GripPoint()
{
	// Tick only exists for the optional debug draw; disabled unless bDrawDebug is set.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_GripPoint::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(bDrawDebug);

	// Register with the owning interactables (ADR-007). If ownership is ambiguous it registers with
	// none; the author-time validation reports it.
	TArray<UFXR_InteractableBase*> ResolvedOwners;
	ResolveOwners(ResolvedOwners);
	for (UFXR_InteractableBase* Owner : ResolvedOwners)
	{
		Owner->RegisterGripPoint(this);
		RegisteredOwners.Add(Owner);
	}

	if (ResolvedOwners.Num() == 0)
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_GripPoint '%s' on '%s': no owning interactable resolved - the point is inert. Parent it under an interactable, keep a single interactable on the actor, or set Owners explicitly."),
			*GetName(), *GetNameSafe(GetOwner()));
	}
}

void UFXR_GripPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<UFXR_InteractableBase>& Owner : RegisteredOwners)
	{
		if (UFXR_InteractableBase* Interactable = Owner.Get())
		{
			Interactable->UnregisterGripPoint(this);
		}
	}
	RegisteredOwners.Reset();

	Super::EndPlay(EndPlayReason);
}

void UFXR_GripPoint::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDebug)
	{
		if (const UWorld* World = GetWorld())
		{
			const FTransform Transform = GetComponentTransform();
			DrawDebugCoordinateSystem(World, Transform.GetLocation(), Transform.Rotator(), 5.f, false, -1.f, 0, 0.3f);
			DrawDebugSphere(World, Transform.GetLocation(), ActivationRadius, 12, FColor::Magenta, false, -1.f, 0, 0.3f);
		}
	}
}

void UFXR_GripPoint::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	RefreshPreviewHand();
#endif
}

void UFXR_GripPoint::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITORONLY_DATA
	if (PreviewHand)
	{
		PreviewHand->DestroyComponent();
		PreviewHand = nullptr;
	}
#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UFXR_GripPoint::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	// The preview hand ignores attachment (see RefreshPreviewHand), so it is moved from here whenever
	// this point moves, including when something above it does.
#if WITH_EDITOR
	if (PreviewHand)
	{
		PlacePreviewHand();
	}
#endif
}

#if WITH_EDITOR
namespace FXR_GripPointPreview
{
	// The pawn's hand for one side, as a component template: the FXR_HandVisual on the pawn named in
	// project settings, else on the default game mode's pawn.
	const UFXR_HandVisual* FindPawnHand(EFXR_HandSide Side)
	{
		UClass* PawnClass = UFXR_InteractionSettings::Get()->HandPreviewPawn.LoadSynchronous();
		if (!PawnClass)
		{
			if (const UClass* GameMode = LoadClass<AGameModeBase>(nullptr, *UGameMapsSettings::GetGlobalDefaultGameMode()))
			{
				PawnClass = GameMode->GetDefaultObject<AGameModeBase>()->DefaultPawnClass;
			}
		}
		const AActor* PawnDefaults = PawnClass ? PawnClass->GetDefaultObject<AActor>() : nullptr;
		if (!PawnDefaults)
		{
			return nullptr;
		}

		// Hands declared in C++ first, then the usual case of hands added in the Blueprint, which only
		// exist as construction script templates.
		TArray<UFXR_HandVisual*> Native;
		PawnDefaults->GetComponents<UFXR_HandVisual>(Native);
		for (const UFXR_HandVisual* Hand : Native)
		{
			if (Hand->GetHandSide() == Side)
			{
				return Hand;
			}
		}

		for (UClass* Current = PawnClass; Current; Current = Current->GetSuperClass())
		{
			const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Current);
			if (!Generated || !Generated->SimpleConstructionScript)
			{
				continue;
			}
			for (const USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				const UFXR_HandVisual* Hand = Node ? Cast<UFXR_HandVisual>(Node->ComponentTemplate) : nullptr;
				if (Hand && Hand->GetHandSide() == Side)
				{
					return Hand;
				}
			}
		}
		return nullptr;
	}
}

void UFXR_GripPoint::RefreshPreviewHand()
{
	// Only in the editor's own worlds: never in game, and never while cooking or running a
	// commandlet, which would load the pawn for nothing.
	const UWorld* World = GetWorld();
	const bool bWanted = bPreviewHand && World && !World->IsGameWorld() && GetOwner() && !IsRunningCommandlet();

	const EFXR_HandSide Side = (Handedness == EFXR_GripHandedness::LeftOnly) ? EFXR_HandSide::Left : EFXR_HandSide::Right;
	const UFXR_HandVisual* Source = bWanted ? FXR_GripPointPreview::FindPawnHand(Side) : nullptr;

	if (PreviewHand && (!Source || PreviewHand->GetHandSide() != Side))
	{
		PreviewHand->DestroyComponent();
		PreviewHand = nullptr;
	}
	if (!Source)
	{
		return;
	}

	if (!PreviewHand)
	{
		// A copy of the pawn's hand component, made the way the engine makes its editor sprites: owned
		// by the actor, attached here, transient and flagged as a visualiser so it is never selected,
		// saved or treated as one of the actor's real components.
		PreviewHand = NewObject<UFXR_HandVisual>(GetOwner(), Source->GetClass(), NAME_None,
			(GetFlags() & RF_Transactional) | RF_Transient | RF_TextExportTransient, const_cast<UFXR_HandVisual*>(Source));
		PreviewHand->CreationMethod = CreationMethod;
		PreviewHand->bIsEditorOnly = true;
		PreviewHand->SetIsVisualizationComponent(true);
		PreviewHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewHand->SetCastShadow(false);

		// World-space, so an object scaled in its Blueprint doesn't shrink or stretch the hand, the
		// same as in game where the hand keeps its own size whatever it holds.
		PreviewHand->SetUsingAbsoluteLocation(true);
		PreviewHand->SetUsingAbsoluteRotation(true);
		PreviewHand->SetUsingAbsoluteScale(true);
		PreviewHand->SetupAttachment(this);

		// Drawn with the socket's ghost material, so it reads as a preview rather than a real hand and
		// the object stays visible through it.
		if (UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr, TEXT("/FlexXR/Materials/M_FXR_Ghost.M_FXR_Ghost")))
		{
			for (int32 Slot = 0; Slot < PreviewHand->GetNumMaterials(); ++Slot)
			{
				PreviewHand->SetMaterial(Slot, Ghost);
			}
		}

		// Posed before registering: the Anim BP is evaluated once as it registers, and would otherwise
		// show an open hand until it next updates.
		PreviewHand->ShowPose(HandPose);
		PreviewHand->RegisterComponent();
		PreviewHand->SetUpdateAnimationInEditor(true);
	}

	PreviewHand->ShowPose(HandPose);
	PlacePreviewHand();
}

void UFXR_GripPoint::PlacePreviewHand() const
{
	// The hold lines this point up with the hand's grip, and the hand mesh sits at the pawn's grip
	// offset from that. Rotation and position only; the scale stays the hand's own, which is how a
	// mirrored left hand stays mirrored.
	const FTransform Hand = PreviewHand->GetGripPoseOffset() * FTransform(GetComponentQuat(), GetComponentLocation());
	PreviewHand->SetWorldLocationAndRotation(Hand.GetLocation(), Hand.GetRotation());
}

void UFXR_GripPoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Handedness picks the hand, Hand Pose shapes it, Preview Hand turns it on or off.
	RefreshPreviewHand();
}
#endif

FVector UFXR_GripPoint::GetClosestPointTo(const FVector& WorldLocation) const
{
	const FTransform Transform = GetComponentTransform();
	if (!IsRail())
	{
		return Transform.GetLocation();
	}

	// Clamp the hand onto the rail segment: local X, centred on the component.
	const FVector Centre = Transform.GetLocation();
	const FVector Axis = Transform.GetUnitAxis(EAxis::X);
	// Through the accessor, so the runtime clamp and the editor gizmo cannot disagree about how long
	// the rail is on a scaled object.
	const float HalfLength = GetRailLength() * 0.5f;
	const float Along = FMath::Clamp(static_cast<float>(FVector::DotProduct(WorldLocation - Centre, Axis)), -HalfLength, HalfLength);
	return Centre + Axis * Along;
}

FTransform UFXR_GripPoint::GetGripTransformFor(const FVector& WorldLocation) const
{
	FTransform Transform = GetComponentTransform();
	if (IsRail())
	{
		// Authored orientation, but slid along the rail to meet the hand.
		Transform.SetLocation(GetClosestPointTo(WorldLocation));
	}
	return Transform;
}

bool UFXR_GripPoint::AcceptsHand(EFXR_HandSide Side) const
{
	switch (Handedness)
	{
	case EFXR_GripHandedness::LeftOnly:  return Side == EFXR_HandSide::Left;
	case EFXR_GripHandedness::RightOnly: return Side == EFXR_HandSide::Right;
	default:                             return true;
	}
}

void UFXR_GripPoint::ResolveOwners(TArray<UFXR_InteractableBase*>& OutOwners) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 1. Explicit Owners win.
	for (const FComponentReference& Reference : Owners)
	{
		if (UFXR_InteractableBase* Interactable = Cast<UFXR_InteractableBase>(Reference.GetComponent(OwnerActor)))
		{
			OutOwners.AddUnique(Interactable);
		}
	}
	if (OutOwners.Num() > 0)
	{
		return;
	}

	// 2. Nearest ancestor interactable in the component hierarchy.
	for (USceneComponent* Parent = GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
	{
		if (UFXR_InteractableBase* Interactable = Cast<UFXR_InteractableBase>(Parent))
		{
			OutOwners.Add(Interactable);
			return;
		}
	}

	// 3. The actor's single interactable. Several -> ambiguous -> none (validation names them).
	TArray<UFXR_InteractableBase*> Candidates;
	OwnerActor->GetComponents<UFXR_InteractableBase>(Candidates);
	if (Candidates.Num() == 1)
	{
		OutOwners.Add(Candidates[0]);
	}
}

bool UFXR_GripPoint::IsOwnedBy(const UFXR_InteractableBase* Interactable) const
{
	if (!Interactable)
	{
		return false;
	}
	TArray<UFXR_InteractableBase*> ResolvedOwners;
	ResolveOwners(ResolvedOwners);
	return ResolvedOwners.Contains(Interactable);
}

#if WITH_EDITOR
void UFXR_GripPoint::CheckForErrors()
{
	Super::CheckForErrors();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<UFXR_InteractableBase*> ResolvedOwners;
	ResolveOwners(ResolvedOwners);

	if (ResolvedOwners.Num() == 0)
	{
		TArray<UFXR_InteractableBase*> Candidates;
		OwnerActor->GetComponents<UFXR_InteractableBase>(Candidates);

		if (Candidates.Num() == 0)
		{
			UE_LOG(LogFXR, Warning,
				TEXT("FXR_GripPoint '%s' on '%s': the actor has no interactable - the grip point will never register."),
				*GetName(), *OwnerActor->GetName());
		}
		else
		{
			FString Names;
			for (const UFXR_InteractableBase* Candidate : Candidates)
			{
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(", "));
				Names += Candidate->GetName();
			}
			UE_LOG(LogFXR, Error,
				TEXT("FXR_GripPoint '%s' on '%s': ambiguous ownership - several interactables (%s) and no explicit Owners. Set Owners, or parent the point under its interactable."),
				*GetName(), *OwnerActor->GetName(), *Names);
		}
		return;
	}

	// Shared points are legal only while at most one owner is enabled at a time.
	int32 EnabledOwners = 0;
	for (const UFXR_InteractableBase* Owner : ResolvedOwners)
	{
		EnabledOwners += Owner->IsInteractionEnabled() ? 1 : 0;
	}
	if (EnabledOwners > 1)
	{
		UE_LOG(LogFXR, Error,
			TEXT("FXR_GripPoint '%s' on '%s': %d owners are enabled simultaneously - a shared grip point allows at most one enabled owner at a time. Disable all but one by default and switch at runtime."),
			*GetName(), *OwnerActor->GetName(), EnabledOwners);
	}
}
#endif
