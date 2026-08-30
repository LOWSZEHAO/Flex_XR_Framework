// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Components/FXR_Locomotion.h"
#include "Rig/FXR_LocomotionOwner.h"
#include "Interactor/FXR_Interactor.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Driver/FXR_InteractionDriver.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Detection/FXR_TeleportRegistry.h"
#include "World/FXR_TeleportAnchor.h"
#include "World/FXR_TeleportBlocker.h"
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

	// Capability fallback (ADR-005): tracked hands have no thumbstick, so teleport is gesture-driven
	// and anything needing a stick is unavailable. Documented, not silent — a hand set to Smooth
	// Move on hand tracking simply cannot move, and saying nothing would read as a broken build.
	const bool bLeftTracked = IsHandTracking(EFXR_HandSide::Left);
	const bool bRightTracked = IsHandTracking(EFXR_HandSide::Right);
	if (bLeftTracked || bRightTracked)
	{
		if (!bLoggedHandFallback)
		{
			bLoggedHandFallback = true;

			const bool bStranded = (bLeftTracked && LeftHandMovement == EFXR_HandMovement::SmoothMove)
				|| (bRightTracked && RightHandMovement == EFXR_HandMovement::SmoothMove);
			const bool bGesture = (bLeftTracked && LeftHandMovement == EFXR_HandMovement::Teleport)
				|| (bRightTracked && RightHandMovement == EFXR_HandMovement::Teleport);

			UE_LOG(LogFXR, Log, TEXT("FXR_Locomotion: tracked hands active — %s (ADR-005 capability rule)."),
				bGesture ? TEXT("gesture teleport + rotation-on-landing") : TEXT("no gesture is bound"));

			UE_CLOG(bStranded, LogFXR, Warning,
				TEXT("FXR_Locomotion: a hand set to Smooth Move is hand-tracked and has no thumbstick, so it cannot move. Set it to Teleport for gesture locomotion."));
			UE_CLOG(!bGesture, LogFXR, Warning,
				TEXT("FXR_Locomotion: no tracked hand is set to Teleport, so there is no locomotion at all on hand tracking."));
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

	// One place to close out aim highlights: every path out of Aiming (commit, yield, disable,
	// gesture cancel) passes through here, so a bound On Exit can never be missed.
	if (Phase != ETeleportPhase::Aiming && (HoveredAnchor.IsValid() || HoveredBlocker.IsValid()))
	{
		UpdateAimHover(nullptr, nullptr);
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
	bTeleportEnabled = bEnabled;
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

	if (LeftStickAction)
	{
		EnhancedInput->BindAction(LeftStickAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleLeftStick);
		EnhancedInput->BindAction(LeftStickAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleLeftStickCompleted);
	}

	if (RightStickAction)
	{
		EnhancedInput->BindAction(RightStickAction, ETriggerEvent::Triggered, this, &UFXR_Locomotion::HandleRightStick);
		EnhancedInput->BindAction(RightStickAction, ETriggerEvent::Completed, this, &UFXR_Locomotion::HandleRightStickCompleted);
	}

	bInputBound = true;
}

bool UFXR_Locomotion::ResolveMovementHand(EFXR_HandMovement Movement, EFXR_HandSide& OutSide) const
{
	const bool bLeft = (LeftHandMovement == Movement);
	const bool bRight = (RightHandMovement == Movement);

	// Prefer a hand that is free, so the mode survives the other hand picking something up.
	if (bRight && !IsHandBusy(EFXR_HandSide::Right))
	{
		OutSide = EFXR_HandSide::Right;
		return true;
	}
	if (bLeft && !IsHandBusy(EFXR_HandSide::Left))
	{
		OutSide = EFXR_HandSide::Left;
		return true;
	}

	OutSide = bRight ? EFXR_HandSide::Right : EFXR_HandSide::Left;
	return false;
}

void UFXR_Locomotion::TryBeginTeleportAim(EFXR_HandSide Side)
{
	if (!bLocomotionEnabled || !bTeleportEnabled || Phase != ETeleportPhase::Idle)
	{
		return;
	}

	if (MovementForHand(Side) != EFXR_HandMovement::Teleport || IsHandBusy(Side))
	{
		return; // this stick doesn't teleport, or its hand is holding something (ADR-005)
	}

	AimingHand = Side;
	Phase = ETeleportPhase::Aiming;
}

void UFXR_Locomotion::HandleLeftStick(const FInputActionValue& Value)
{
	ApplyStick(EFXR_HandSide::Left, Value.Get<FVector2D>());
}

void UFXR_Locomotion::HandleRightStick(const FInputActionValue& Value)
{
	ApplyStick(EFXR_HandSide::Right, Value.Get<FVector2D>());
}

void UFXR_Locomotion::HandleLeftStickCompleted()
{
	ApplyStick(EFXR_HandSide::Left, FVector2D::ZeroVector);
}

void UFXR_Locomotion::HandleRightStickCompleted()
{
	ApplyStick(EFXR_HandSide::Right, FVector2D::ZeroVector);
}

void UFXR_Locomotion::ApplyStick(EFXR_HandSide Side, FVector2D Axis)
{
	StickAxis[static_cast<int32>(Side)] = Axis;

	if (MovementForHand(Side) != EFXR_HandMovement::Teleport)
	{
		return; // smooth move and turning are polled in tick; only teleport is edge-driven
	}

	if (Axis.Y >= TeleportActivationThreshold)
	{
		TryBeginTeleportAim(Side);
	}
	else if (Phase == ETeleportPhase::Aiming && AimingHand == Side)
	{
		// Released, or eased back below the threshold without fully centring — both commit.
		CommitTeleport();
	}
}

void UFXR_Locomotion::ProcessTurn(float DeltaTime)
{
	if (!bLocomotionEnabled || !bTurnEnabled)
	{
		return;
	}

	// Each hand turns in its own mode, so both are polled — one may snap while the other strafes.
	ProcessTurnForHand(EFXR_HandSide::Left, DeltaTime);
	ProcessTurnForHand(EFXR_HandSide::Right, DeltaTime);
}

void UFXR_Locomotion::ProcessTurnForHand(EFXR_HandSide Side, float DeltaTime)
{
	const EFXR_TurnMode Mode = TurnModeForHand(Side);
	const int32 Index = static_cast<int32>(Side);

	if (Mode == EFXR_TurnMode::None || IsHandBusy(Side)) // yield: this hand is holding something
	{
		bTurnArmed[Index] = true; // re-arm so re-enabling never fires a stale snap
		return;
	}

	const float Axis = StickAxis[Index].X;
	const float AbsAxis = FMath::Abs(Axis);

	// A flick past the snap threshold yaws once and disarms until that stick re-centres.
	if (Mode == EFXR_TurnMode::Snap)
	{
		if (AbsAxis >= TurnSnapThreshold)
		{
			if (bTurnArmed[Index])
			{
				ApplyYaw(FMath::Sign(Axis) * SnapAngle);
				bTurnArmed[Index] = false;
			}
		}
		else if (AbsAxis < TurnRearmThreshold)
		{
			bTurnArmed[Index] = true;
		}
		return;
	}

	if (AbsAxis > TurnSmoothDeadzone)
	{
		ApplyYaw(Axis * SmoothTurnRate * DeltaTime);
		// Continuous turn feeds the comfort vignette; two hands turning at once take the stronger.
		SmoothTurnFactor = FMath::Max(SmoothTurnFactor, FMath::Min(AbsAxis, 1.f));
	}
}

bool UFXR_Locomotion::AccumulateSmoothMove(EFXR_HandSide Side, float DeltaTime, FVector& InOutDelta) const
{
	if (MovementForHand(Side) != EFXR_HandMovement::SmoothMove || IsHandBusy(Side))
	{
		return false; // not a moving hand, or it is holding something (ADR-005)
	}

	FVector2D Axis = StickAxis[static_cast<int32>(Side)];

	// This hand's X belongs to turning unless it has no turn mode, in which case it strafes.
	if (TurnModeForHand(Side) != EFXR_TurnMode::None)
	{
		Axis.X = 0.f;
	}

	if (Axis.IsNearlyZero())
	{
		return false;
	}

	const IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(GetOwner());
	const USceneComponent* HMD = LocOwner ? LocOwner->GetHMDComponent() : nullptr;

	// Horizontal frame this stick steers in. Hand-relative uses this hand's own aim yaw; Head and
	// Hip use the HMD yaw (the rig has no hip tracker, so Hip degenerates to Head).
	float Yaw = 0.f;
	if (MoveDirectionSource == EFXR_MoveDirectionSource::HandRelative)
	{
		if (const IFXR_Interactor* MoveInteractor = GetInteractorForHand(Side))
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
	InOutDelta += (Forward * Axis.Y + Right * Axis.X) * SmoothMoveSpeed * DeltaTime;
	return true;
}

void UFXR_Locomotion::ProcessSmoothMove(float DeltaTime)
{
	if (!bLocomotionEnabled)
	{
		return;
	}

	// Both hands contribute: setting both to Smooth Move means both sticks move you, rather than
	// one silently winning. Each steers in its own frame, so the sum is what the player asked for.
	FVector Delta = FVector::ZeroVector;
	const bool bLeft = AccumulateSmoothMove(EFXR_HandSide::Left, DeltaTime, Delta);
	const bool bRight = AccumulateSmoothMove(EFXR_HandSide::Right, DeltaTime, Delta);
	if (!bLeft && !bRight)
	{
		return;
	}

	const IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(GetOwner());
	USceneComponent* Origin = LocOwner ? LocOwner->GetTrackingOriginComponent() : nullptr;
	if (!Origin)
	{
		return;
	}

	// Two sticks can otherwise stack past the authored speed; the cap keeps SmoothMoveSpeed honest.
	const float MaxStep = SmoothMoveSpeed * DeltaTime;
	if (Delta.SizeSquared() > FMath::Square(MaxStep))
	{
		Delta = Delta.GetSafeNormal() * MaxStep;
	}

	Origin->AddWorldOffset(Delta, false, nullptr, ETeleportType::TeleportPhysics);

	SmoothMoveFactor = (MaxStep > KINDA_SMALL_NUMBER)
		? FMath::Min(static_cast<float>(Delta.Size()) / MaxStep, 1.f) // fraction of top speed
		: 0.f;
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
	// While aiming, stay with the hand that started; otherwise take a free hand set to Teleport,
	// exactly as the stick path does.
	EFXR_HandSide GestureSide = AimingHand;
	if (Phase != ETeleportPhase::Aiming && !ResolveMovementHand(EFXR_HandMovement::Teleport, GestureSide))
	{
		return;
	}

	IFXR_Interactor* Hand = GetInteractorForHand(GestureSide);
	if (!Hand || Hand->GetInteractorType() != EFXR_InteractorType::TrackedHand)
	{
		return;
	}

	// Yield to interaction (ADR-005), and to anything merely *reachable*: the pinch that commits a
	// teleport is the same pinch that grabs, so reaching for an object must never also move you.
	if (!bLocomotionEnabled || !bTeleportEnabled || IsHandBusy(GestureSide) || HasGrabCandidate(GestureSide))
	{
		if (Phase == ETeleportPhase::Aiming)
		{
			Phase = ETeleportPhase::Idle; // reached for something / disabled mid-aim → cancel
		}
		return;
	}

	const bool bPalmAway = IsPalmFacingAway(Hand);
	const bool bPinch = Hand->GetSelectValue() >= HandPinchCommitThreshold;

	if (Phase == ETeleportPhase::Idle)
	{
		// Turn the palm outward (without pinching) to begin aiming.
		if (bPalmAway && !bPinch)
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
		else if (!bPalmAway)
		{
			Phase = ETeleportPhase::Idle; // turn the palm back in to cancel
		}
	}
}

bool UFXR_Locomotion::IsHandTracking(EFXR_HandSide Side) const
{
	const IFXR_Interactor* Interactor = GetInteractorForHand(Side);
	return Interactor && Interactor->GetInteractorType() == EFXR_InteractorType::TrackedHand;
}

bool UFXR_Locomotion::IsPalmFacingAway(const IFXR_Interactor* Hand) const
{
	if (!Hand)
	{
		return false;
	}

	const FTransform Palm = Hand->GetPalmTransform();
	const FVector PalmNormal = Palm.TransformVectorNoScale(PalmForwardAxisLocal).GetSafeNormal();

	const IFXR_LocomotionOwner* LocOwner = Cast<IFXR_LocomotionOwner>(GetOwner());
	const USceneComponent* HMD = LocOwner ? LocOwner->GetHMDComponent() : nullptr;
	if (!HMD)
	{
		return false;
	}

	// Measured against head→hand rather than head-forward, so turning the palm outward still reads
	// as "away" when the arm is held out to the side. Too close to the head for that to be stable,
	// and the look direction is the better answer.
	FVector Outward = Palm.GetLocation() - HMD->GetComponentLocation();
	if (!Outward.Normalize(10.f))
	{
		Outward = HMD->GetForwardVector();
	}

	return FVector::DotProduct(PalmNormal, Outward) >= PalmAwayThreshold;
}

bool UFXR_Locomotion::HasGrabCandidate(EFXR_HandSide Side) const
{
	const IFXR_Interactor* Interactor = GetInteractorForHand(Side);
	UFXR_InteractionSubsystem* Subsystem = UFXR_InteractionSubsystem::Get(this);
	if (!Interactor || !Subsystem)
	{
		return false;
	}

	// A read-only query against the same sphere the driver claims with, so this never depends on
	// whether the driver has ticked yet this frame.
	FVector GrabCenter;
	float GrabRadius = 0.f;
	Interactor->GetGrabSphere(GrabCenter, GrabRadius);
	return Subsystem->FindBestCandidate(GrabCenter, GrabRadius, Side) != nullptr;
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
	if (!bLocomotionEnabled || !bTeleportEnabled || IsHandBusy(AimingHand))
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
	UFXR_TeleportAnchor* HitAnchor = nullptr;

	IFXR_Interactor* Aim = GetAimInteractor();
	UWorld* World = GetWorld();
	if (!Aim || !World)
	{
		UpdateAimHover(nullptr, nullptr);
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
		UpdateAimHover(nullptr, nullptr);
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
			if (UFXR_TeleportAnchor* Anchor = CachedRegistry->FindAnchorNear(HitLocation))
			{
				OutTarget = Anchor->GetComponentLocation();
				OutValid = true;
				HitAnchor = Anchor;
				if (Anchor->ShouldOverrideFacing())
				{
					OutFacingYaw = Anchor->GetComponentRotation().Yaw;
				}
			}
		}
		break;
	}

	// A blocker rejects any otherwise-valid target, in every validation mode.
	UFXR_TeleportBlocker* HitBlocker = nullptr;
	if (OutValid && CachedRegistry)
	{
		HitBlocker = CachedRegistry->FindBlockerAt(OutTarget);
		if (HitBlocker)
		{
			OutValid = false;
			HitAnchor = nullptr; // blocked: the anchor is not the thing to highlight
		}
	}

	UpdateAimHover(HitAnchor, HitBlocker);
	return OutValid;
}

void UFXR_Locomotion::UpdateAimHover(UFXR_TeleportAnchor* Anchor, UFXR_TeleportBlocker* Blocker)
{
	if (HoveredAnchor.Get() != Anchor)
	{
		if (UFXR_TeleportAnchor* Previous = HoveredAnchor.Get())
		{
			Previous->OnExit.Broadcast();
		}
		HoveredAnchor = Anchor;
		if (Anchor)
		{
			Anchor->OnAim.Broadcast();
		}
	}

	if (HoveredBlocker.Get() != Blocker)
	{
		if (UFXR_TeleportBlocker* Previous = HoveredBlocker.Get())
		{
			Previous->OnExit.Broadcast();
		}
		HoveredBlocker = Blocker;
		if (Blocker)
		{
			Blocker->OnAim.Broadcast();
		}
	}
}

void UFXR_Locomotion::CommitTeleport()
{
	if (!bTargetValid)
	{
		Phase = ETeleportPhase::Idle; // aimed at an invalid spot — cancel
		return;
	}

	// Announced at commit rather than after the fade, so a landing sound starts with the fade out.
	if (UFXR_TeleportAnchor* Anchor = HoveredAnchor.Get())
	{
		Anchor->OnTeleported.Broadcast();
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
