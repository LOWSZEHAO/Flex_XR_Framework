// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_LocomotionTypes.h"
#include "FXR_Locomotion.generated.h"

class UInputAction;
class UInputMappingContext;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCameraComponent;
class UFXR_InteractorComponent;
class UFXR_InteractionDriver;
class UFXR_TeleportRegistry;
class IFXR_Interactor;
struct FInputActionValue;

/**
 * UFXR_Locomotion — the single locomotion component (ADR-005): teleport, smooth move, turn, and
 * comfort, arbitrated internally rather than split into four components. Teleport and snap/smooth
 * turning are live; smooth move, vignette, and presets are following slices.
 *
 * Room-scale correct (ADR-006): teleport moves the play-space origin so the HMD lands on the
 * target, obtained via the owner's IFXR_LocomotionOwner — never a concrete pawn cast, so it works
 * with any project's pawn. Yields to interaction: a hand currently holding an interactable drives
 * no locomotion.
 *
 * Add it to the pawn, assign a teleport input action + mapping context, hold to aim the arc and
 * release to commit. The arc + reticle draw as debug lines until arc/reticle meshes are assigned.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_Locomotion : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_Locomotion();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Master switch for all locomotion (games gate directly; SOP hard-lock steps call the same API). */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Locomotion")
	void SetLocomotionEnabled(bool bEnabled);

	/** Enable/disable teleport specifically. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Locomotion")
	void SetTeleportEnabled(bool bEnabled);

	/** Enable/disable turning specifically. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Locomotion")
	void SetTurnEnabled(bool bEnabled);

	/** Scripted move (SOP "MoveTo" step): relocate the rig so the HMD lands at Location, facing Rotation. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Locomotion")
	bool TeleportToLocation(const FVector& Location, FRotator Rotation);

	/** Apply a locomotion feel preset at runtime (fills the feel fields; Custom leaves them). */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Locomotion")
	void SetPreset(EFXR_LocomotionPreset InPreset);

	/** True while a teleport arc is being aimed. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Locomotion")
	bool IsAimingTeleport() const { return Phase == ETeleportPhase::Aiming; }

	/** Current comfort-vignette intensity 0..1 — bind to a post-process/overlay if not using Vignette Material. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Locomotion")
	float GetVignetteIntensity() const { return VignetteIntensity; }

protected:
	/** Feel preset — picking one fills every field below; editing any of them flips this to Custom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion")
	EFXR_LocomotionPreset Preset = EFXR_LocomotionPreset::Standard;

	//~ Movement
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	bool bAllowTeleport = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	bool bAllowSmoothMove = false;

	/** Frame the move stick is relative to (Hip approximates to Head — the rig has no hip tracker). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement", meta = (EditCondition = "bAllowSmoothMove"))
	EFXR_MoveDirectionSource MoveDirectionSource = EFXR_MoveDirectionSource::HeadRelative;

	/** Smooth-move speed (cm/s) — 250 ≈ 2.5 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement", meta = (ClampMin = "1.0", EditCondition = "bAllowSmoothMove"))
	float SmoothMoveSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	EFXR_TeleportTransition Transition = EFXR_TeleportTransition::Fade;

	//~ Teleport
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	EFXR_TeleportAim AimStyle = EFXR_TeleportAim::ProjectileArc;

	/** Straight-ray reach / arc distance cap (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport", meta = (ClampMin = "50.0"))
	float MaxDistance = 1000.f;

	/** Projectile-arc launch speed (cm/s) — higher throws the arc farther and flatter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport", meta = (ClampMin = "50.0", EditCondition = "AimStyle == EFXR_TeleportAim::ProjectileArc"))
	float ArcLaunchSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	EFXR_TeleportValidation Validation = EFXR_TeleportValidation::NavMesh;

	/** Steepest floor (degrees from horizontal) still teleportable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport", meta = (ClampMin = "0.0", ClampMax = "89.0", EditCondition = "Validation == EFXR_TeleportValidation::SurfaceAngle"))
	float MaxSurfaceAngle = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	EFXR_LandingRotation LandingRotation = EFXR_LandingRotation::KeepFacing;

	/** Collision channel the aim traces against (and the valid-surface channel for Custom Channel validation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	TEnumAsByte<ECollisionChannel> TeleportTraceChannel = ECC_WorldStatic;

	/** View transition length (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport", meta = (ClampMin = "0.0"))
	float FadeDuration = 0.15f;

	//~ Turning
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning")
	EFXR_TurnMode TurnMode = EFXR_TurnMode::Snap;

	/** Degrees per snap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning", meta = (ClampMin = "1.0", ClampMax = "180.0", EditCondition = "TurnMode == EFXR_TurnMode::Snap"))
	float SnapAngle = 30.f;

	/** Degrees per second for smooth turning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning", meta = (ClampMin = "1.0", EditCondition = "TurnMode == EFXR_TurnMode::Smooth"))
	float SmoothTurnRate = 90.f;

	//~ Comfort
	/** Peripheral vignette during artificial motion. Dynamic scales with speed; Always is a constant narrowed FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Comfort")
	EFXR_VignetteMode VignetteMode = EFXR_VignetteMode::Dynamic;

	/** Maximum vignette closure 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Comfort", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "VignetteMode != EFXR_VignetteMode::Off"))
	float VignetteStrength = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Comfort", meta = (EditCondition = "VignetteMode == EFXR_VignetteMode::Dynamic"))
	bool bVignetteOnTurn = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Comfort", meta = (EditCondition = "VignetteMode == EFXR_VignetteMode::Dynamic"))
	bool bVignetteOnSmoothMove = true;

	//~ Hand Tracking (gesture teleport when the hand is the active source; controllers ignore these)
	/** Pinch strength that commits a gesture teleport. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hand Tracking", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HandPinchCommitThreshold = 0.7f;

	/** Palm-local axis that points at the ground in the aim pose (palm down). Flip a sign in-headset if aiming reads inverted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hand Tracking")
	FVector PalmDownAxisLocal = FVector(0.f, 0.f, -1.f);

	/** How closely the palm must face down (dot with world-down) to begin aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hand Tracking", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PalmDownThreshold = 0.5f;

	//~ Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputMappingContext> LocomotionContext;

	/**
	 * Which hand does what is declared by which actions you assign — there is deliberately no
	 * separate "which hand" setting to contradict the wiring. Assign a hand's action and that hand
	 * gains the mode; assign both and both hands have it; leave one empty and that hand does not.
	 *
	 * Teleport: Axis1D (thumbstick Y) or a button. The value is read as a float and thresholded
	 * here, so pushing the stick forward aims and pulling it back does nothing. Add a Negate
	 * modifier if your stick reads forward as negative.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> TeleportActionLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> TeleportActionRight;

	/** Stick push at or above which the arc appears (a button reads 1.0, so any value under 1 works). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TeleportActivationThreshold = 0.6f;

	/** Turn: Axis1D (thumbstick X) — sign turns left/right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> TurnActionLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> TurnActionRight;

	/** Smooth move: Axis2D (thumbstick XY) — X = strafe, Y = forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> MoveActionLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> MoveActionRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	int32 InputPriority = 0;

	//~ Visuals (optional; debug lines are used until meshes are assigned — a later slice).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UStaticMesh> ReticleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	/** Post-process material driven each frame with the vignette intensity. If unset, bind Get Vignette Intensity to your own overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> VignetteMaterial;

	/** Scalar parameter on the vignette material that receives the 0..1 intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	FName VignetteIntensityParameter = TEXT("Intensity");

private:
	enum class ETeleportPhase : uint8 { Idle, Aiming, FadingOut, FadingIn };

	void ApplyPreset(EFXR_LocomotionPreset InPreset);
	void TryBindInput();

	// Enhanced Input binds a bare member function, so each hand needs its own entry point; they all
	// funnel straight into the shared per-side implementations below.
	void HandleTeleportLeft(const FInputActionValue& Value) { HandleTeleport(EFXR_HandSide::Left, Value); }
	void HandleTeleportRight(const FInputActionValue& Value) { HandleTeleport(EFXR_HandSide::Right, Value); }
	void HandleTeleportLeftCompleted() { HandleTeleportReleased(EFXR_HandSide::Left); }
	void HandleTeleportRightCompleted() { HandleTeleportReleased(EFXR_HandSide::Right); }
	void HandleTurnLeft(const FInputActionValue& Value) { HandleTurn(EFXR_HandSide::Left, Value); }
	void HandleTurnRight(const FInputActionValue& Value) { HandleTurn(EFXR_HandSide::Right, Value); }
	void HandleTurnLeftCompleted() { TurnAxis[0] = 0.f; }
	void HandleTurnRightCompleted() { TurnAxis[1] = 0.f; }
	void HandleMoveLeft(const FInputActionValue& Value) { HandleMove(EFXR_HandSide::Left, Value); }
	void HandleMoveRight(const FInputActionValue& Value) { HandleMove(EFXR_HandSide::Right, Value); }
	void HandleMoveLeftCompleted() { MoveAxis[0] = FVector2D::ZeroVector; }
	void HandleMoveRightCompleted() { MoveAxis[1] = FVector2D::ZeroVector; }

	void HandleTeleport(EFXR_HandSide Side, const FInputActionValue& Value);
	void HandleTeleportReleased(EFXR_HandSide Side);
	void HandleTurn(EFXR_HandSide Side, const FInputActionValue& Value);
	void HandleMove(EFXR_HandSide Side, const FInputActionValue& Value);
	/** Begin aiming with this hand if it may right now (enabled, idle, not holding anything). */
	void TryBeginTeleportAim(EFXR_HandSide Side);
	void UpdateAim();
	bool PredictAndValidate(FVector& OutTarget, bool& OutValid, float& OutFacingYaw);
	void CommitTeleport();
	void ExecuteMove();
	void ProcessTurn(float DeltaTime);
	void ProcessSmoothMove(float DeltaTime);
	void ProcessHandTeleportGesture();
	bool IsHandTracking(EFXR_HandSide Side) const;
	bool IsPalmDown(const IFXR_Interactor* Hand) const;
	void UpdateVignette(float DeltaTime);
	void ApplyVignetteToMaterial();
	/** Yaw the tracking origin about the HMD (head stays put, world spins — ADR-006). Shared by turn + landing. */
	void ApplyYaw(float DeltaYawDegrees);
	void DrawAim() const;
	void StartCameraFade(float From, float To) const;
	bool IsHandBusy(EFXR_HandSide Side) const;
	IFXR_Interactor* GetInteractorForHand(EFXR_HandSide Side) const;
	/** The hand currently aiming a teleport (whichever one pushed its stick). */
	IFXR_Interactor* GetAimInteractor() const { return GetInteractorForHand(AimingHand); }
	/** True if that hand has a teleport action assigned — assignment is what grants a hand the mode. */
	bool HandHasTeleport(EFXR_HandSide Side) const;
	static int32 HandIndex(EFXR_HandSide Side) { return Side == EFXR_HandSide::Left ? 0 : 1; }

	ETeleportPhase Phase = ETeleportPhase::Idle;
	bool bLocomotionEnabled = true;
	bool bTurnEnabled = true;
	bool bInputBound = false;
	bool bLoggedMissingOwner = false;
	bool bLoggedHandFallback = false;

	FVector TargetLocation = FVector::ZeroVector;
	bool bTargetValid = false;
	float TargetFacingYaw = 0.f;
	float FadeElapsed = 0.f;

	// Per hand, indexed by HandIndex(). Both hands may be wired for the same mode, so each keeps its
	// own axis and its own snap arming; the dominant push wins each frame rather than the two adding.
	float TurnAxis[2] = { 0.f, 0.f };
	bool bTurnArmed[2] = { true, true };
	FVector2D MoveAxis[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
	/** Which hand owns the teleport arc in flight — only one aims at a time. */
	EFXR_HandSide AimingHand = EFXR_HandSide::Right;

	// Comfort: per-frame smooth-motion factors (0 for teleport/snap — those never vignette) feed the
	// eased VignetteIntensity, which drives the assigned material and Get Vignette Intensity.
	float VignetteIntensity = 0.f;
	float SmoothMoveFactor = 0.f;
	float SmoothTurnFactor = 0.f;
	bool bVignetteBlendableAdded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VignetteMID;

	// Persistent scratch so aiming allocates nothing per frame (design 5.8): PathData's capacity is
	// reused across frames, and the drawn polyline reuses ArcPoints.
	FPredictProjectilePathResult ArcResult;
	TArray<FVector> ArcPoints;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_InteractorComponent>> CachedInteractors;

	UPROPERTY(Transient)
	TObjectPtr<UFXR_InteractionDriver> CachedDriver;

	UPROPERTY(Transient)
	TObjectPtr<UFXR_TeleportRegistry> CachedRegistry;
};
