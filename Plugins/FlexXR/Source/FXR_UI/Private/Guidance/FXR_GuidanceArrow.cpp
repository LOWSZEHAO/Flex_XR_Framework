// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Guidance/FXR_GuidanceArrow.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Settings/FXR_MotionSettings.h"
#include "Types/FXR_HighlightTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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

	// A component target takes priority and is re-read every frame, so the arrow follows it if it moves.
	if (const USceneComponent* Target = TargetComponent.Get())
	{
		OutLocation = Target->GetComponentLocation();
		return true;
	}

	// Target component was destroyed: stop instead of pointing at its old location.
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

		// Horizontal only, for both the check and the arrow. A target straight above isn't somewhere you
		// can walk, and an arrow pitched at the floor looks broken.
		FVector ToTarget = Target - CameraLocation;
		const float GroundDistance = ToTarget.Size2D();
		ToTarget.Z = 0.f;
		Heading = ToTarget.GetSafeNormal();

		FVector Facing = Cam->GetForwardVector();
		Facing.Z = 0.f;
		Facing = Facing.GetSafeNormal();

		// Either way the player doesn't need the arrow any more.
		const bool bArrived = GroundDistance <= ArriveRadius;
		const float AngleOff = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Facing, Heading), -1.f, 1.f)));
		const bool bAlreadyLooking = AngleOff <= HideWithinAngle;

		bWanted = !Heading.IsNearlyZero() && !bArrived && !bAlreadyLooking;

		// Follow the camera's yaw only, so the arrow stays put when the player looks up and down.
		const FRotator YawOnly(0.f, Cam->GetComponentRotation().Yaw, 0.f);
		Anchor = CameraLocation + YawOnly.RotateVector(FVector(Distance, LateralOffset, 0.f)) + FVector(0.f, 0.f, HeightOffset);
	}

	const float Step = FFXR_Motion::FadeStep(DeltaTime, UFXR_MotionSettings::GetFadeDuration());
	Alpha = FMath::FInterpConstantTo(Alpha, bWanted ? 1.f : 0.f, 1.f, Step);


	if (Alpha <= KINDA_SMALL_NUMBER)
	{
		if (Arrow)
		{
			Arrow->SetVisibility(false);
		}
		if (VisualActor)
		{
			VisualActor->SetActorHiddenInGame(true);
		}
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Fade by scaling, since the material is opaque unlit and can't fade its alpha.
	const float Eased = FFXR_Motion::EaseFade(Alpha);

	// A Visual Class Blueprint takes priority when set (particles, animated markers, etc.).
	if (VisualClass)
	{
		if (!VisualActor && !bVisualSpawnAttempted)
		{
			bVisualSpawnAttempted = true;
			if (UWorld* World = GetWorld())
			{
				FActorSpawnParameters Params;
				Params.Owner = Owner;
				Params.Instigator = Owner->GetInstigator();
				// Transient so it's never saved; always spawn, since it's purely visual and shouldn't fail
				// because of an overlap.
				Params.ObjectFlags |= RF_Transient;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				VisualActor = World->SpawnActor<AActor>(VisualClass, Anchor, FRotator::ZeroRotator, Params);
				if (VisualActor)
				{
					// Keep the Blueprint's own scale so the fade multiplies it.
					VisualBaseScale = VisualActor->GetActorScale3D();
				}
			}
		}

		if (!VisualActor)
		{
			return; // spawn failed once; do not thrash trying again every frame
		}

		// Actors face +X by convention, so no axis setting is needed for a Blueprint.
		VisualActor->SetActorLocationAndRotation(Anchor, FRotationMatrix::MakeFromX(Heading).Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		VisualActor->SetActorScale3D(VisualBaseScale * Eased);
		VisualActor->SetActorHiddenInGame(false);
		return;
	}

	UStaticMesh* Mesh = ArrowMesh.LoadSynchronous();
	UMaterialInterface* Material = ArrowMaterial.LoadSynchronous();
	if (!Mesh)
	{
		return; // cleared on purpose draws nothing; guidance state is unaffected
	}

	// Created on first use (like the locomotion arc and the ray beam), so a pawn that never shows
	// guidance doesn't pay for it.
	if (!Arrow)
	{
		Arrow = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Arrow->SetupAttachment(Owner->GetRootComponent());
		// Movable (runtime components default to Static and ignore transform updates) and absolute
		// scale, so the rig's scale doesn't affect a size in cm.
		Arrow->SetMobility(EComponentMobility::Movable);
		Arrow->SetAbsolute(true, true, true);
		Arrow->SetCastShadow(false);
		Arrow->SetStaticMesh(Mesh);
		// Clear collision after assigning the mesh (the mesh brings its own body setup). The arrow must
		// never block traces, pokes or thrown objects.
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

			// Use the Guidance colour from the same state map the highlight uses, so they always match.
			if (ArrowMID)
			{
				if (const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get())
				{
					ArrowMID->SetVectorParameterValue(TEXT("ArrowColor"), Settings->GetColorFor(EFXR_HighlightState::Guidance));
				}
			}
		}
	}

	// Size from the mesh bounds along its forward axis, so Arrow Size really is in cm for any mesh.
	const FVector Extent = Mesh->GetBounds().BoxExtent;
	float MeshLength = KINDA_SMALL_NUMBER;
	float MeshGirth = KINDA_SMALL_NUMBER;
	switch (MeshForwardAxis)
	{
	case EFXR_MeshAxis::X:
	case EFXR_MeshAxis::NegX:
		MeshLength = FMath::Max(Extent.X * 2.f, KINDA_SMALL_NUMBER);
		MeshGirth = FMath::Max(FMath::Max(Extent.Y, Extent.Z) * 2.f, KINDA_SMALL_NUMBER);
		break;
	case EFXR_MeshAxis::Y:
	case EFXR_MeshAxis::NegY:
		MeshLength = FMath::Max(Extent.Y * 2.f, KINDA_SMALL_NUMBER);
		MeshGirth = FMath::Max(FMath::Max(Extent.X, Extent.Z) * 2.f, KINDA_SMALL_NUMBER);
		break;
	default:
		MeshLength = FMath::Max(Extent.Z * 2.f, KINDA_SMALL_NUMBER);
		MeshGirth = FMath::Max(FMath::Max(Extent.X, Extent.Y) * 2.f, KINDA_SMALL_NUMBER);
		break;
	}

	const float Length = (ArrowSize * Eased) / MeshLength;
	const float Girth = (ArrowSize * 0.55f * Eased) / MeshGirth;

	FRotator Rotation;
	FVector Scale;
	switch (MeshForwardAxis)
	{
	case EFXR_MeshAxis::X:
		Rotation = FRotationMatrix::MakeFromX(Heading).Rotator();
		Scale = FVector(Length, Girth, Girth);
		break;
	case EFXR_MeshAxis::NegX:
		Rotation = FRotationMatrix::MakeFromX(-Heading).Rotator();
		Scale = FVector(Length, Girth, Girth);
		break;
	case EFXR_MeshAxis::Y:
		Rotation = FRotationMatrix::MakeFromY(Heading).Rotator();
		Scale = FVector(Girth, Length, Girth);
		break;
	case EFXR_MeshAxis::NegY:
		Rotation = FRotationMatrix::MakeFromY(-Heading).Rotator();
		Scale = FVector(Girth, Length, Girth);
		break;
	case EFXR_MeshAxis::NegZ:
		Rotation = FRotationMatrix::MakeFromZ(-Heading).Rotator();
		Scale = FVector(Girth, Girth, Length);
		break;
	case EFXR_MeshAxis::Z:
	default:
		Rotation = FRotationMatrix::MakeFromZ(Heading).Rotator();
		Scale = FVector(Girth, Girth, Length);
		break;
	}

	Arrow->SetWorldLocationAndRotation(Anchor, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Arrow->SetWorldScale3D(Scale);
	Arrow->SetVisibility(true);
}

void UFXR_GuidanceArrow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The mesh component is transient and dies with the owner, but a spawned actor has to be
	// destroyed explicitly.
	if (VisualActor)
	{
		VisualActor->Destroy();
		VisualActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}
