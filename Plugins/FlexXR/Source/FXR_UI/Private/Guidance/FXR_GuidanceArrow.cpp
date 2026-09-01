// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Guidance/FXR_GuidanceArrow.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Types/FXR_HighlightTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UFXR_GuidanceArrow::UFXR_GuidanceArrow()
{
	PrimaryComponentTick.bCanEverTick = true;

	ArrowMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone")));
	ArrowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/FlexXR/Materials/M_FXR_Arrow.M_FXR_Arrow")));
}

void UFXR_GuidanceArrow::PointToLocation(FVector WorldLocation)
{
	TargetComponent = nullptr;
	TargetLocation = WorldLocation;
	bHasTarget = true;
}

void UFXR_GuidanceArrow::PointToComponent(USceneComponent* Target)
{
	TargetComponent = Target;
	bHasTarget = (Target != nullptr);
}

void UFXR_GuidanceArrow::ClearGuidance()
{
	TargetComponent = nullptr;
	bHasTarget = false;
}

bool UFXR_GuidanceArrow::ResolveTarget(FVector& OutLocation) const
{
	if (!bHasTarget)
	{
		return false;
	}

	// A component target wins and is re-read every frame, so guidance survives the thing moving —
	// a cart being wheeled to its bay is still the place to go.
	if (const USceneComponent* Target = TargetComponent.Get())
	{
		OutLocation = Target->GetComponentLocation();
		return true;
	}

	// A component that has been destroyed is not a stale location to keep pointing at.
	if (TargetComponent.IsExplicitlyNull())
	{
		OutLocation = TargetLocation;
		return true;
	}
	return false;
}

UCameraComponent* UFXR_GuidanceArrow::GetCamera() const
{
	if (!Camera)
	{
		if (const AActor* Owner = GetOwner())
		{
			Camera = Owner->FindComponentByClass<UCameraComponent>();
		}
	}
	return Camera;
}

void UFXR_GuidanceArrow::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateArrow(DeltaTime);
}

void UFXR_GuidanceArrow::UpdateArrow(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_GuidanceArrow);

	const UCameraComponent* Cam = GetCamera();
	FVector Target;
	bool bWanted = Cam && ResolveTarget(Target);

	FVector Anchor = FVector::ZeroVector;
	FVector Heading = FVector::ForwardVector;

	if (bWanted)
	{
		const FVector CameraLocation = Cam->GetComponentLocation();

		// Horizontal only, both for the test and the arrow itself. A target directly overhead is not
		// a direction anyone can walk, and an arrow that pitches at the floor reads as broken.
		FVector ToTarget = Target - CameraLocation;
		const float GroundDistance = ToTarget.Size2D();
		ToTarget.Z = 0.f;
		Heading = ToTarget.GetSafeNormal();

		FVector Facing = Cam->GetForwardVector();
		Facing.Z = 0.f;
		Facing = Facing.GetSafeNormal();

		// Redundant in two ways, and both mean the same thing: the player does not need telling.
		const bool bArrived = GroundDistance <= ArriveRadius;
		const float AngleOff = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Heading), -1.f, 1.f)));
		const bool bAlreadyLooking = AngleOff <= HideWithinAngle;

		bWanted = !Heading.IsNearlyZero() && !bArrived && !bAlreadyLooking;

		// Anchored on the camera's yaw rather than its full rotation, so the arrow holds still while
		// the player looks up and down instead of swimming around the view.
		const FRotator YawOnly(0.f, Cam->GetComponentRotation().Yaw, 0.f);
		Anchor = CameraLocation + YawOnly.RotateVector(FVector(Distance, 0.f, 0.f)) + FVector(0.f, 0.f, HeightOffset);
	}

	const float Step = (FadeTime > KINDA_SMALL_NUMBER) ? (DeltaTime / FadeTime) : 1.f;
	Alpha = FMath::FInterpConstantTo(Alpha, bWanted ? 1.f : 0.f, 1.f, Step);

	if (Alpha <= KINDA_SMALL_NUMBER)
	{
		if (Arrow)
		{
			Arrow->SetVisibility(false);
		}
		return;
	}

	AActor* Owner = GetOwner();
	UStaticMesh* Mesh = ArrowMesh.LoadSynchronous();
	UMaterialInterface* Material = ArrowMaterial.LoadSynchronous();
	if (!Owner || !Mesh)
	{
		return; // cleared on purpose draws nothing; guidance state is unaffected
	}

	// Built on first use, like the locomotion arc and the far-ray beam: a pawn that never receives
	// guidance costs nothing.
	if (!Arrow)
	{
		Arrow = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Arrow->SetupAttachment(Owner->GetRootComponent());
		// Movable and absolute: a runtime component defaults to Static mobility and silently discards
		// every transform update, and absolute keeps the rig's own scale out of a size in centimetres.
		Arrow->SetMobility(EComponentMobility::Movable);
		Arrow->SetAbsolute(true, true, true);
		Arrow->SetCastShadow(false);
		Arrow->SetStaticMesh(Mesh);
		// Cleared after the mesh is assigned, which brings its own body setup with it. The arrow is a
		// picture; it must never block a trace, catch a poke, or stop a thrown object.
		Arrow->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arrow->SetGenerateOverlapEvents(false);
		Arrow->CanCharacterStepUpOn = ECB_No;
		Arrow->RegisterComponent();

		if (Material)
		{
			ArrowMID = UMaterialInstanceDynamic::Create(Material, this);
			if (ArrowMID)
			{
				Arrow->SetMaterial(0, ArrowMID);
			}
			else
			{
				Arrow->SetMaterial(0, Material);
			}

			// Guidance's colour, from the same state map the highlight reads. An arrow that disagreed
			// with the glow at the end of it would be two vocabularies for one instruction.
			if (ArrowMID)
			{
				if (const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get())
				{
					ArrowMID->SetVectorParameterValue(TEXT("ArrowColor"), Settings->GetColorFor(EFXR_HighlightState::Guidance));
				}
			}
		}
	}

	// Size from the mesh's own bounds, so Arrow Size is honestly centimetres whatever mesh is set.
	// The cone points +Z, so Z is its length and X/Y its girth.
	const FVector Extent = Mesh->GetBounds().BoxExtent;
	const float MeshLength = FMath::Max(Extent.Z * 2.f, KINDA_SMALL_NUMBER);
	const float MeshGirth = FMath::Max(Extent.X * 2.f, KINDA_SMALL_NUMBER);

	// The fade is geometric — the arrow grows in rather than dissolving, which an opaque unlit
	// material cannot do anyway, and which reads as arriving rather than materialising.
	const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
	const float Length = (ArrowSize * Eased) / MeshLength;
	const float Girth = (ArrowSize * 0.55f * Eased) / MeshGirth;

	Arrow->SetWorldLocationAndRotation(Anchor, FRotationMatrix::MakeFromZ(Heading).Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	Arrow->SetWorldScale3D(FVector(Girth, Girth, Length));
	Arrow->SetVisibility(true);
}
