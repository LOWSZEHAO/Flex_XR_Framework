// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Components/FXR_Locomotion.h"
#include "Rig/FXR_LocomotionOwner.h"
#include "Interactor/FXR_Interactor.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Driver/FXR_InteractionDriver.h"
#include "Detection/FXR_TeleportRegistry.h"
#include "World/FXR_TeleportAnchor.h"
#include "Types/FXR_LogChannels.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	// Snap fires past the high threshold and rearms only after re-centring below the low one — so a
	// single flick is exactly one snap rather than a spin.
	constexpr float TurnSnapThreshold = 0.8f;
	constexpr float TurnRearmThreshold = 0.3f;
	constexpr float TurnSmoothDeadzone = 0.15f;
}

UFXR_Locomotion::UFXR_Locomotion()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Start consistent with the default preset so the panel never shows a preset the fields contradict.
	ApplyPreset(Preset);
}

void UFXR_Locomotion::ApplyPreset(EFXR_LocomotionPreset InPreset)
{
	switch (InPreset)
	{
	case EFXR_LocomotionPreset::Comfort:
		bAllowTeleport = true;   Transition = EFXR_TeleportTransition::Fade;
		bAllowSmoothMove = false;
		TurnMode = EFXR_TurnMode::Snap; SnapAngle = 30.f;
		VignetteMode = EFXR_VignetteMode::Always;
		break;

	case EFXR_LocomotionPreset::Standard:
		bAllowTeleport = true;   Transition = EFXR_TeleportTransition::Fade;
		bAllowSmoothMove = true; SmoothMoveSpeed = 250.f; // ~2.5 m/s
		TurnMode = EFXR_TurnMode::Snap; SnapAngle = 30.f;
		VignetteMode = EFXR_VignetteMode::Dynamic;
		break;

	case EFXR_LocomotionPreset::Free:
		bAllowTeleport = true;   Transition = EFXR_TeleportTransition::Dash;
		bAllowSmoothMove = true; SmoothMoveSpeed = 400.f; // ~4 m/s
		TurnMode = EFXR_TurnMode::Smooth; SmoothTurnRate = 90.f;
		VignetteMode = EFXR_VignetteMode::Off;
		break;

	case EFXR_LocomotionPreset::Custom:
	default:
		break; // Custom leaves the fields as authored
	}

	Preset = InPreset;
}

void UFXR_Locomotion::SetPreset(EFXR_LocomotionPreset InPreset)
{
	ApplyPreset(InPreset);
}

#if WITH_EDITOR
void UFXR_Locomotion::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Changed = PropertyChangedEvent.GetPropertyName();

	if (Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, Preset))
	{
		if (Preset != EFXR_LocomotionPreset::Custom)
		{
			ApplyPreset(Preset);
		}
		return;
	}

	// Editing any preset-controlled feel field flips to Custom (input / visual / hand setup fields don't).
	const bool bFeelField =
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, bAllowTeleport) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, Transition) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, bAllowSmoothMove) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, SmoothMoveSpeed) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, TurnMode) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, SnapAngle) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, SmoothTurnRate) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Locomotion, VignetteMode);

	if (bFeelField)
	{
		Preset = EFXR_LocomotionPreset::Custom;
	}
}
#endif

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

	CachedRegistry = UFXR_TeleportRegistry::Get(this);
}

void UFXR_Locomotion::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Locomotion_Tick);

	if (!bInputBound)
	{
		TryBindInput();
	}

	// Cleared each frame; the smooth-motion handlers below set them, so teleport/snap never vignette.
	SmoothMoveFactor = 0.f;
	SmoothTurnFactor = 0.f;

	// Capability fallback (ADR-005): tracked hands have no thumbstick, so gesture-drive teleport and
	// drop smooth move. Documented, not silent.
	const bool bHandTeleport = (TeleportHand != EFXR_LocomotionHand::Right && IsHandTracking(EFXR_HandSide::Left))
		|| (TeleportHand != EFXR_LocomotionHand::Left && IsHandTracking(EFXR_HandSide::Right));
	if (bHandTeleport)
	{
		if (!bLoggedHandFallback)
		{
			UE_LOG(LogFXR, Log, TEXT("FXR_Locomotion: tracked hands active — using gesture teleport + rotation-on-landing; smooth move disabled (ADR-005 capability rule)."));
			bLoggedHandFallback = true;
		}
		ProcessHandTeleportGesture();
	}

	// Turning runs whenever the view isn't mid-transition. On hands the turn stick is absent, so this
	// is a no-op and turning comes from teleport landing rotation instead.
	if (Phase == ETeleportPhase::Idle || Phase == ETeleportPhase::Aiming)
	{
		ProcessTurn(DeltaTime);
	}

	// Smooth move is illegal while aiming a teleport (ADR-005) and not offered on hands (no stick).
	if (Phase == ETeleportPhase::Idle)
	{
		ProcessSmoothMove(DeltaTime);
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

	UpdateVignette(DeltaTime);
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
		EnhancedInput->BindAction(TeleportAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleTeleport);
		EnhancedInput->BindAction(TeleportAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleTeleportCompleted);
	}

	if (TurnAction)
	{
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleTurn);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleTurnCompleted);
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleMove);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleMoveCompleted);
	}

	bInputBound = true;
}

bool UFXR_Locomotion::ResolveLocomotionHand(EFXR_LocomotionHand Assignment, EFXR_HandSide& OutSide) const
{
	switch (Assignment)
	{
	case EFXR_LocomotionHand::Left:
		OutSide = EFXR_HandSide::Left;
		return !IsHandBusy(OutSide);

	case EFXR_LocomotionHand::Right:
		OutSide = EFXR_HandSide::Right;
		return !IsHandBusy(OutSide);

	case EFXR_LocomotionHand::Both:
	default:
		// Either hand may drive, so prefer one that is free; only yield when both are occupied.
		if (!IsHandBusy(EFXR_HandSide::Right))
		{
			OutSide = EFXR_HandSide::Right;
			return true;
		}
		if (!IsHandBusy(EFXR_HandSide::Left))
		{
			OutSide = EFXR_HandSide::Left;
			return true;
		}
		OutSide = EFXR_HandSide::Right;
		return false;
	}
}

void UFXR_Locomotion::TryBeginTeleportAim()
{
	if (!bLocomotionEnabled || !bAllowTeleport || Phase != ETeleportPhase::Idle)
	{
		return;
	}

	EFXR_HandSide Side;
	if (!ResolveLocomotionHand(TeleportHand, Side)) // yield: that hand is holding an interactable
	{
		return;
	}

	AimingHand = Side;
	Phase = ETeleportPhase::Aiming;
}

void UFXR_Locomotion::HandleTeleport(const FInputActionValue& Value)
{
	// Read as a float so one binding serves both a thumbstick and a button (a button reads 1.0).
	// Thresholding here rather than in the mapping keeps it directional: Enhanced Input actuates a
	// digital action on magnitude, which would let pulling the stick *back* teleport as well.
	const float Push = Value.Get<float>();

	if (Push >= TeleportActivationThreshold)
	{
		TryBeginTeleportAim();
	}
	else if (Phase == ETeleportPhase::Aiming)
	{
		// Eased back below the threshold without fully centring — that is still a commit.
		CommitTeleport();
	}
}

void UFXR_Locomotion::HandleTeleportCompleted()
{
	// Stick centred (or button released) — the action stops triggering, so commit from here.
	if (Phase == ETeleportPhase::Aiming)
	{
		CommitTeleport();
	}
}

void UFXR_Locomotion::HandleTurn(const FInputActionValue& Value)
{
	CurrentTurnAxis = Value.Get<float>();
}

void UFXR_Locomotion::HandleMove(const FInputActionValue& Value)
{
	CurrentMoveAxis = Value.Get<FVector2D>();
}

void UFXR_Locomotion::ProcessTurn(float DeltaTime)
{
	if (!bLocomotionEnabled || !bTurnEnabled || TurnMode == EFXR_TurnMode::None)
	{
		return;
	}

	EFXR_HandSide Side;
	if (!ResolveLocomotionHand(TurnHand, Side)) // yield: the turning hand is holding an interactable
	{
		return;
	}

	const float Axis = CurrentTurnAxis;
	const float AbsAxis = FMath::Abs(Axis);

	// A flick past the snap threshold yaws once and disarms until the stick re-centres.
	if (TurnMode == EFXR_TurnMode::Snap)
	{
		if (AbsAxis >= TurnSnapThreshold)
		{
			if (bTurnArmed)
			{
				ApplyYaw(FMath::Sign(Axis) * SnapAngle);
				bTurnArmed = false;
			}
		}
		else if (AbsAxis < TurnRearmThreshold)
		{
			bTurnArmed = true;
		}
		return;
	}

	if (AbsAxis > TurnSmoothDeadzone)
	{
		ApplyYaw(Axis * SmoothTurnRate * DeltaTime);
		SmoothTurnFactor = FMath::Min(AbsAxis, 1.f); // continuous turn feeds the comfort vignette
	}
}

void UFXR_Locomotion::ProcessSmoothMove(float DeltaTime)
{
	if (!bLocomotionEnabled || !bAllowSmoothMove || CurrentMoveAxis.IsNearlyZero())
	{
		return;
	}

	EFXR_HandSide MovingSide;
	if (!ResolveLocomotionHand(MoveHand, MovingSide)) // yield: the moving hand is holding something
	{
		return;
	}

	const FVector2D Axis = CurrentMoveAxis;
	AActor* Owner = GetOwner();
	IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(Owner);
	USceneComponent* Origin = LocOwner ? LocOwner->GetTrackingOriginComponent() : nullptr;
	USceneComponent* HMD = LocOwner ? LocOwner->GetHMDComponent() : nullptr;
	if (!Origin)
	{
		return;
	}

	// Horizontal frame the stick steers in. Hand-relative uses the moving hand's aim yaw; Head and
	// Hip use the HMD yaw (the rig has no hip tracker, so Hip degenerates to Head).
	float Yaw = 0.f;
	if (MoveDirectionSource == EFXR_MoveDirectionSource::HandRelative)
	{
		if (const IFXR_Interactor* MoveInteractor = GetInteractorForHand(MovingSide))
		{
			Yaw = MoveInteractor->GetAimTransform().Rotator().Yaw;
		}
		else if (HMD)
		{
			Yaw = HMD->GetComponentRotation().Yaw;
		}
	}
	else if (HMD)
	{
		Yaw = HMD->GetComponentRotation().Yaw;
	}

	const FRotator YawRotation(0.f, Yaw, 0.f);
	const FVector Forward = YawRotation.Vector();
	const FVector Right = FRotationMatrix(YawRotation).GetScaledAxis(EAxis::Y);
	const FVector Delta = (Forward * Axis.Y + Right * Axis.X) * SmoothMoveSpeed * DeltaTime;
	Origin->AddWorldOffset(Delta, false, nullptr, ETeleportType::TeleportPhysics);

	SmoothMoveFactor = FMath::Min(static_cast<float>(Axis.Size()), 1.f); // stick magnitude ≈ speed
}

void UFXR_Locomotion::UpdateVignette(float DeltaTime)
{
	float Target = 0.f;
	switch (VignetteMode)
	{
	case EFXR_VignetteMode::Off:
		Target = 0.f;
		break;

	case EFXR_VignetteMode::Always:
		Target = VignetteStrength;
		break;

	case EFXR_VignetteMode::Dynamic:
	{
		float Factor = 0.f;
		if (bVignetteOnSmoothMove)
		{
			Factor = FMath::Max(Factor, SmoothMoveFactor);
		}
		if (bVignetteOnTurn)
		{
			Factor = FMath::Max(Factor, SmoothTurnFactor);
		}
		Target = Factor * VignetteStrength;
		break;
	}
	}

	// Ease so the vignette never pops (a popping vignette is itself uncomfortable).
	VignetteIntensity = FMath::FInterpTo(VignetteIntensity, Target, DeltaTime, 8.f);
	ApplyVignetteToMaterial();
}

void UFXR_Locomotion::ApplyVignetteToMaterial()
{
	if (!VignetteMaterial)
	{
		return; // no material assigned — the game binds Get Vignette Intensity to its own overlay instead
	}

	if (!bVignetteBlendableAdded)
	{
		const IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(GetOwner());
		if (UCameraComponent* Camera = LocOwner ? Cast<UCameraComponent>(LocOwner->GetHMDComponent()) : nullptr)
		{
			VignetteMID = UMaterialInstanceDynamic::Create(VignetteMaterial, this);
			Camera->PostProcessSettings.AddBlendable(VignetteMID, 1.f);
			bVignetteBlendableAdded = true;
		}
	}

	if (VignetteMID)
	{
		VignetteMID->SetScalarParameterValue(VignetteIntensityParameter, VignetteIntensity);
	}
}

void UFXR_Locomotion::ProcessHandTeleportGesture()
{
	// While aiming, stay with the hand that started; otherwise take the hand this mode is assigned
	// to (Both prefers a free hand, exactly as the stick path does).
	EFXR_HandSide GestureSide = AimingHand;
	if (Phase != ETeleportPhase::Aiming && !ResolveLocomotionHand(TeleportHand, GestureSide))
	{
		return;
	}

	IFXR_Interactor* Hand = GetInteractorForHand(GestureSide);
	if (!Hand || Hand->GetInteractorType() != EFXR_InteractorType::TrackedHand)
	{
		return;
	}

	if (!bLocomotionEnabled || !bAllowTeleport || IsHandBusy(GestureSide))
	{
		if (Phase == ETeleportPhase::Aiming)
		{
			Phase = ETeleportPhase::Idle; // grabbed something / disabled mid-aim → cancel
		}
		return;
	}

	const bool bPalmDown = IsPalmDown(Hand);
	const bool bPinch = Hand->GetSelectValue() >= HandPinchCommitThreshold;

	if (Phase == ETeleportPhase::Idle)
	{
		// Raise the hand palm-down (without pinching) to begin aiming.
		if (bPalmDown && !bPinch)
		{
			AimingHand = GestureSide;
			Phase = ETeleportPhase::Aiming;
		}
	}
	else if (Phase == ETeleportPhase::Aiming)
	{
		if (bPinch)
		{
			CommitTeleport(); // pinch commits
		}
		else if (!bPalmDown)
		{
			Phase = ETeleportPhase::Idle; // open the hand / turn it up to cancel
		}
	}
}

bool UFXR_Locomotion::IsHandTracking(EFXR_HandSide Side) const
{
	const IFXR_Interactor* Interactor = GetInteractorForHand(Side);
	return Interactor && Interactor->GetInteractorType() == EFXR_InteractorType::TrackedHand;
}

bool UFXR_Locomotion::IsPalmDown(const IFXR_Interactor* Hand) const
{
	if (!Hand)
	{
		return false;
	}
	const FTransform Palm = Hand->GetPalmTransform();
	const FVector WorldAxis = Palm.TransformVectorNoScale(PalmDownAxisLocal).GetSafeNormal();
	return FVector::DotProduct(WorldAxis, FVector::DownVector) >= PalmDownThreshold;
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
	if (!bLocomotionEnabled || !bAllowTeleport || IsHandBusy(AimingHand))
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
		if (CachedRegistry)
		{
			if (const UFXR_TeleportAnchor* Anchor = CachedRegistry->FindAnchorNear(HitLocation))
			{
				OutTarget = Anchor->GetComponentLocation();
				OutValid = true;
				if (Anchor->ShouldOverrideFacing())
				{
					OutFacingYaw = Anchor->GetComponentRotation().Yaw;
				}
			}
		}
		break;
	}

	// A blocker rejects any otherwise-valid target, in every validation mode.
	if (OutValid && CachedRegistry && CachedRegistry->IsBlocked(OutTarget))
	{
		OutValid = false;
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

IFXR_Interactor* UFXR_Locomotion::GetInteractorForHand(EFXR_HandSide Side) const
{
	for (const TObjectPtr<UFXR_InteractorComponent>& Comp : CachedInteractors)
	{
		if (Comp && Comp->IsInteractorActive() && Comp->GetHandSide() == Side)
		{
			return Comp.Get();
		}
	}
	return nullptr;
}
