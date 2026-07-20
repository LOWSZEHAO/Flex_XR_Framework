// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Components/FXR_Locomotion.h"
#include "Rig/FXR_LocomotionOwner.h"
#include "Interactor/FXR_Interactor.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Driver/FXR_InteractionDriver.h"
#include "Types/FXR_LogChannels.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	// Snap fires past the high threshold and rearms only after re-centring below the low one — so a
	// single flick is one snap; a partial hold below the high threshold drives smooth (Both mode).
	constexpr float TurnSnapThreshold = 0.8f;
	constexpr float TurnRearmThreshold = 0.3f;
	constexpr float TurnSmoothDeadzone = 0.15f;
}

UFXR_Locomotion::UFXR_Locomotion()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_Locomotion::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Cache the rig's interactors (aim source) and interaction driver (yielding) once — they are
		// created with the pawn and never added at runtime; only their active/held state changes.
		TArray<UFXR_InteractorComponent*> Found;
		Owner->GetComponents<UFXR_InteractorComponent>(Found);
		CachedInteractors.Reset();
		for (UFXR_InteractorComponent* Interactor : Found)
		{
			CachedInteractors.Add(Interactor);
		}

		CachedDriver = Owner->FindComponentByClass<UFXR_InteractionDriver>();
	}
}

void UFXR_Locomotion::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Locomotion_Tick);

	if (!bInputBound)
	{
		TryBindInput();
	}

	// Turning runs whenever the view isn't mid-transition — including while aiming a teleport arc.
	if (Phase == ETeleportPhase::Idle || Phase == ETeleportPhase::Aiming)
	{
		ProcessTurn(DeltaTime);
	}

	switch (Phase)
	{
	case ETeleportPhase::Aiming:
		UpdateAim();
		break;

	case ETeleportPhase::FadingOut:
		FadeElapsed += DeltaTime;
		if (FadeElapsed >= FadeDuration * 0.5f)
		{
			ExecuteMove();
			Phase = ETeleportPhase::FadingIn;
			FadeElapsed = 0.f;
			StartCameraFade(1.f, 0.f);
		}
		break;

	case ETeleportPhase::FadingIn:
		FadeElapsed += DeltaTime;
		if (FadeElapsed >= FadeDuration * 0.5f)
		{
			Phase = ETeleportPhase::Idle;
		}
		break;

	default:
		break;
	}
}

void UFXR_Locomotion::SetLocomotionEnabled(bool bEnabled)
{
	bLocomotionEnabled = bEnabled;
	if (!bEnabled && Phase == ETeleportPhase::Aiming)
	{
		Phase = ETeleportPhase::Idle;
	}
}

void UFXR_Locomotion::SetTeleportEnabled(bool bEnabled)
{
	bAllowTeleport = bEnabled;
	if (!bEnabled && Phase == ETeleportPhase::Aiming)
	{
		Phase = ETeleportPhase::Idle;
	}
}

void UFXR_Locomotion::SetTurnEnabled(bool bEnabled)
{
	bTurnEnabled = bEnabled;
}

bool UFXR_Locomotion::TeleportToLocation(const FVector& Location, FRotator Rotation)
{
	// Scripted move: no aim/validation, no transition — used by SOP "MoveTo" steps and game script.
	TargetLocation = Location;
	TargetFacingYaw = Rotation.Yaw;
	bTargetValid = true;
	ExecuteMove();
	return true;
}

void UFXR_Locomotion::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		bInputBound = true; // nothing to bind to; don't retry every tick forever
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return; // not possessed yet — retry next tick
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!EnhancedInput)
	{
		return; // input component not ready — retry next tick
	}

	if (LocomotionContext)
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(LocomotionContext, InputPriority);
			}
		}
	}

	if (TeleportAction)
	{
		EnhancedInput->BindAction(TeleportAction, ETriggerEvent::Started, this, &UFXR_Locomotion::HandleTeleportStarted);
		EnhancedInput->BindAction(TeleportAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleTeleportCompleted);
	}

	if (TurnAction)
	{
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleTurn);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleTurnCompleted);
	}

	bInputBound = true;
}

void UFXR_Locomotion::HandleTeleportStarted()
{
	if (!bLocomotionEnabled || !bAllowTeleport || Phase != ETeleportPhase::Idle)
	{
		return;
	}
	if (IsHandBusy(TeleportHand)) // yield: the aiming hand is holding an interactable
	{
		return;
	}
	Phase = ETeleportPhase::Aiming;
}

void UFXR_Locomotion::HandleTeleportCompleted()
{
	if (Phase == ETeleportPhase::Aiming)
	{
		CommitTeleport();
	}
}

void UFXR_Locomotion::HandleTurn(const FInputActionValue& Value)
{
	CurrentTurnAxis = Value.Get<float>();
}

void UFXR_Locomotion::HandleTurnCompleted()
{
	CurrentTurnAxis = 0.f;
}

void UFXR_Locomotion::ProcessTurn(float DeltaTime)
{
	if (!bLocomotionEnabled || !bTurnEnabled || TurnMode == EFXR_TurnMode::None)
	{
		return;
	}
	if (IsHandBusy(TurnHand)) // yield: the turning hand is holding an interactable
	{
		return;
	}

	const float Axis = CurrentTurnAxis;
	const float AbsAxis = FMath::Abs(Axis);
	const bool bSnapActive = (TurnMode == EFXR_TurnMode::Snap || TurnMode == EFXR_TurnMode::Both);
	const bool bSmoothActive = (TurnMode == EFXR_TurnMode::Smooth || TurnMode == EFXR_TurnMode::Both);

	// In the snap zone a flick yaws once and disarms; smooth is suppressed so it never double-turns.
	if (bSnapActive && AbsAxis >= TurnSnapThreshold)
	{
		if (bTurnArmed)
		{
			ApplyYaw(FMath::Sign(Axis) * SnapAngle);
			bTurnArmed = false;
		}
		return;
	}

	if (AbsAxis < TurnRearmThreshold)
	{
		bTurnArmed = true;
	}

	if (bSmoothActive && AbsAxis > TurnSmoothDeadzone)
	{
		ApplyYaw(Axis * SmoothTurnRate * DeltaTime);
	}
}

void UFXR_Locomotion::ApplyYaw(float DeltaYawDegrees)
{
	if (FMath::IsNearlyZero(DeltaYawDegrees))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(Owner);
	USceneComponent* Origin = LocOwner ? LocOwner->GetTrackingOriginComponent() : nullptr;
	USceneComponent* HMD = LocOwner ? LocOwner->GetHMDComponent() : nullptr;

	if (Origin && HMD)
	{
		// Rotate the origin about the HMD's vertical axis: the head keeps its world position, the
		// world yaws around it — the room-scale-correct pivot (ADR-006), same as landing rotation.
		const FVector Pivot = HMD->GetComponentLocation();
		const FQuat DeltaQ(FVector::UpVector, FMath::DegreesToRadians(DeltaYawDegrees));
		const FVector NewLocation = Pivot + DeltaQ.RotateVector(Origin->GetComponentLocation() - Pivot);
		const FQuat NewRotation = DeltaQ * Origin->GetComponentQuat();
		Origin->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		// Fallback rotates about the pawn root, not the head — same tradeoff as the ADR-006 fallback.
		Owner->AddActorWorldRotation(FRotator(0.f, DeltaYawDegrees, 0.f));
	}
}

void UFXR_Locomotion::UpdateAim()
{
	// Yield mid-aim too: dropping into a grab while aiming cancels the arc.
	if (!bLocomotionEnabled || !bAllowTeleport || IsHandBusy(TeleportHand))
	{
		Phase = ETeleportPhase::Idle;
		return;
	}

	PredictAndValidate(TargetLocation, bTargetValid, TargetFacingYaw);
	DrawAim();
}

bool UFXR_Locomotion::PredictAndValidate(FVector& OutTarget, bool& OutValid, float& OutFacingYaw)
{
	OutValid = false;
	ArcPoints.Reset();

	IFXR_Interactor* Aim = GetAimInteractor();
	UWorld* World = GetWorld();
	if (!Aim || !World)
	{
		return false;
	}

	const FTransform AimPose = Aim->GetAimTransform();
	const FVector Start = AimPose.GetLocation();
	const FVector Dir = AimPose.GetUnitAxis(EAxis::X);
	OutFacingYaw = Dir.Rotation().Yaw;

	FVector HitLocation = FVector::ZeroVector;
	FVector HitNormal = FVector::UpVector;
	bool bHit = false;

	if (AimStyle == EFXR_TeleportAim::ProjectileArc)
	{
		FPredictProjectilePathParams Params;
		Params.StartLocation = Start;
		Params.LaunchVelocity = Dir * ArcLaunchSpeed;
		Params.bTraceWithCollision = true;
		Params.ProjectileRadius = 0.f;
		Params.MaxSimTime = 2.f;
		Params.SimFrequency = 20.f;
		Params.TraceChannel = TeleportTraceChannel;
		Params.bTraceComplex = false;
		Params.ActorsToIgnore.Add(GetOwner());

		bHit = UGameplayStatics::PredictProjectilePath(this, Params, ArcResult);
		for (const FPredictProjectilePathPointData& Point : ArcResult.PathData)
		{
			ArcPoints.Add(Point.Location);
		}
		HitLocation = ArcResult.HitResult.Location;
		HitNormal = ArcResult.HitResult.Normal;
	}
	else // StraightRay
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(GetOwner());
		const FVector End = Start + Dir * MaxDistance;
		bHit = World->LineTraceSingleByChannel(Hit, Start, End, TeleportTraceChannel, QueryParams);
		ArcPoints.Add(Start);
		ArcPoints.Add(bHit ? Hit.Location : End);
		HitLocation = Hit.Location;
		HitNormal = Hit.Normal;
	}

	if (!bHit)
	{
		return false;
	}

	switch (Validation)
	{
	case EFXR_TeleportValidation::NavMesh:
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation NavLoc;
			if (Nav->ProjectPointToNavigation(HitLocation, NavLoc, FVector(100.f, 100.f, 200.f)))
			{
				OutTarget = NavLoc.Location;
				OutValid = true;
			}
		}
		break;

	case EFXR_TeleportValidation::SurfaceAngle:
	{
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(HitNormal.GetSafeNormal(), FVector::UpVector)));
		if (Angle <= MaxSurfaceAngle)
		{
			OutTarget = HitLocation;
			OutValid = true;
		}
		break;
	}

	case EFXR_TeleportValidation::CustomChannel:
		// The aim already traced against the valid-surface channel, so any hit is a valid target.
		OutTarget = HitLocation;
		OutValid = true;
		break;

	case EFXR_TeleportValidation::AnchorsOnly:
		if (!bLoggedAnchorsUnsupported)
		{
			UE_LOG(LogFXR, Warning, TEXT("FXR_Locomotion: 'Anchors Only' validation needs FXR_TeleportAnchor, which is a later slice — no target will validate until then."));
			bLoggedAnchorsUnsupported = true;
		}
		break;
	}

	return OutValid;
}

void UFXR_Locomotion::CommitTeleport()
{
	if (!bTargetValid)
	{
		Phase = ETeleportPhase::Idle; // aimed at an invalid spot — cancel
		return;
	}

	const bool bFade = (Transition != EFXR_TeleportTransition::Instant) && (FadeDuration > KINDA_SMALL_NUMBER);
	if (bFade)
	{
		Phase = ETeleportPhase::FadingOut;
		FadeElapsed = 0.f;
		StartCameraFade(0.f, 1.f);
	}
	else
	{
		ExecuteMove();
		Phase = ETeleportPhase::Idle;
	}
}

void UFXR_Locomotion::ExecuteMove()
{
	if (!bTargetValid)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(Owner);
	USceneComponent* Origin = LocOwner ? LocOwner->GetTrackingOriginComponent() : nullptr;
	USceneComponent* HMD = LocOwner ? LocOwner->GetHMDComponent() : nullptr;

	if (Origin && HMD)
	{
		// Move the play-space origin so the HMD's horizontal position lands on the target (ADR-006);
		// the origin's height becomes the destination floor so the player stands on it.
		FVector HMDOffset = HMD->GetComponentLocation() - Origin->GetComponentLocation();
		HMDOffset.Z = 0.f;

		FVector NewOrigin = TargetLocation - HMDOffset;
		NewOrigin.Z = TargetLocation.Z;
		Origin->SetWorldLocation(NewOrigin, false, nullptr, ETeleportType::TeleportPhysics);

		// Landing rotation about the HMD (ADR-006). KeepFacing is a no-op; FaceArc turns the player
		// to face the arc's aim direction. Thumbstick Choose (aim-time stick) is a later refinement.
		if (LandingRotation == EFXR_LandingRotation::FaceArc)
		{
			const float HMDYaw = HMD->GetComponentRotation().Yaw;
			ApplyYaw(FRotator::NormalizeAxis(TargetFacingYaw - HMDYaw));
		}
	}
	else
	{
		Owner->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (!bLoggedMissingOwner)
		{
			UE_LOG(LogFXR, Warning, TEXT("FXR_Locomotion on '%s': owner does not implement IFXR_LocomotionOwner — falling back to SetActorLocation; the HMD will not land precisely on target (ADR-006). Implement the interface on your pawn."), *GetNameSafe(Owner));
			bLoggedMissingOwner = true;
		}
	}

	bTargetValid = false;
}

void UFXR_Locomotion::DrawAim() const
{
	const UWorld* World = GetWorld();
	if (!World || ArcPoints.Num() == 0)
	{
		return;
	}

	const FColor Color = bTargetValid ? FColor::Green : FColor::Red;
	for (int32 Index = 1; Index < ArcPoints.Num(); ++Index)
	{
		DrawDebugLine(World, ArcPoints[Index - 1], ArcPoints[Index], Color, false, -1.f, 0, 1.5f);
	}

	const FVector Reticle = bTargetValid ? TargetLocation : ArcPoints.Last();
	DrawDebugCircle(World, Reticle + FVector(0.f, 0.f, 2.f), 20.f, 24, Color, false, -1.f, 0, 1.5f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
}

void UFXR_Locomotion::StartCameraFade(float From, float To) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			const bool bHoldWhenFinished = (To > From); // fading to black holds until we fade back
			PC->PlayerCameraManager->StartCameraFade(From, To, FadeDuration * 0.5f, FColor::Black, false, bHoldWhenFinished);
		}
	}
}

bool UFXR_Locomotion::IsHandBusy(EFXR_HandSide Side) const
{
	return CachedDriver && CachedDriver->GetHeldInteractable(Side) != nullptr;
}

IFXR_Interactor* UFXR_Locomotion::GetAimInteractor() const
{
	for (const TObjectPtr<UFXR_InteractorComponent>& Comp : CachedInteractors)
	{
		if (Comp && Comp->IsInteractorActive() && Comp->GetHandSide() == TeleportHand)
		{
			return Comp.Get();
		}
	}
	return nullptr;
}
