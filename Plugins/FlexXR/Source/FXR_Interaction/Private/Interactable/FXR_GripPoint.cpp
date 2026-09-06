// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_GripPoint.h"
#include "Interactable/FXR_HandPose.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Types/FXR_LogChannels.h"
#include "GameFramework/Actor.h"
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

	// Attach to the owning interactables' grip registries (ADR-007). Ambiguity resolved to none
	// on purpose — an unowned point never registers, and the author-time validation names it.
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
			TEXT("FXR_GripPoint '%s' on '%s': no owning interactable resolved — the point is inert. Parent it under an interactable, keep a single interactable on the actor, or set Owners explicitly."),
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
				TEXT("FXR_GripPoint '%s' on '%s': the actor has no interactable — the grip point will never register."),
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
				TEXT("FXR_GripPoint '%s' on '%s': ambiguous ownership — several interactables (%s) and no explicit Owners. Set Owners, or parent the point under its interactable."),
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
			TEXT("FXR_GripPoint '%s' on '%s': %d owners are enabled simultaneously — a shared grip point allows at most one enabled owner at a time. Disable all but one by default and switch at runtime."),
			*GetName(), *OwnerActor->GetName(), EnabledOwners);
	}
}
#endif
