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
 * comfort, arbitrated internally rather than split into four components.
 *
 * Movement is chosen per hand (Left/Right Hand Movement), not per mode. Teleport and smooth move
 * both claim the stick's forward axis, so a hand can only carry one of them — expressing the
 * assignment this way makes the clash impossible rather than something to validate.
 *
 * Room-scale correct (ADR-006): teleport moves the play-space origin so the HMD lands on the
 * target, obtained via the owner's IFXR_LocomotionOwner — never a concrete pawn cast, so it works
 * with any project's pawn. Yields to interaction: a hand currently holding an interactable drives
 * no locomotion.
 *
 * Add it to the pawn and assign the mapping context plus one Axis2D action per thumbstick; push a
 * stick forward to aim the arc and release to commit. The arc + reticle draw as debug lines until
 * arc/reticle meshes are assigned.
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

	//~ Movement — what each hand's stick does. Set both to the same mode to give the player either
	//~ hand; set one to None for a rig where only one hand may move (common in guided training).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	EFXR_HandMovement LeftHandMovement = EFXR_HandMovement::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	EFXR_HandMovement RightHandMovement = EFXR_HandMovement::Teleport;

	/** Frame the move stick is relative to (Hip approximates to Head — the rig has no hip tracker). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement")
	EFXR_MoveDirectionSource MoveDirectionSource = EFXR_MoveDirectionSource::HeadRelative;

	/** Smooth-move speed (cm/s) — 250 ≈ 2.5 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Movement", meta = (ClampMin = "1.0"))
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

	/**
	 * Which stick's sideways axis turns. Both = whichever hand is free to. A hand set to Smooth
	 * Move needs its X for strafing, so it never turns regardless of what is chosen here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning", meta = (EditCondition = "TurnMode != EFXR_TurnMode::None"))
	EFXR_LocomotionHand TurnHand = EFXR_LocomotionHand::Right;

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
	 * One Axis2D action per hand — bind the left thumbstick to one and the right to the other, and
	 * never touch the mapping context again. What a stick *does* is read off Left/Right Hand
	 * Movement and Turn Hand above, so changing the hand layout is a detail-panel edit.
	 *
	 * An action per hand rather than per mode is what makes that honest: Enhanced Input cannot say
	 * which hand actuated a shared action, so a per-mode action bound to both sticks would let the
	 * left stick start the right hand's teleport.
	 *
	 *   Y (forward)  → that hand's movement mode (teleport aim, or smooth-move forward)
	 *   X (sideways) → turning, if that hand is the turn hand; strafe while smooth-moving
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> LeftStickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input")
	TObjectPtr<UInputAction> RightStickAction;

	/**
	 * Forward push at or above which the teleport arc appears. Thresholding here rather than in the
	 * mapping keeps it directional — Enhanced Input actuates on magnitude, which would let pulling
	 * the stick back teleport as well.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Input", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float TeleportActivationThreshold = 0.6f;

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

	void HandleLeftStick(const FInputActionValue& Value);
	void HandleRightStick(const FInputActionValue& Value);
	void HandleLeftStickCompleted();
	void HandleRightStickCompleted();

	/** Store one hand's stick and act on its forward axis; turning is polled from it in ProcessTurn. */
	void ApplyStick(EFXR_HandSide Side, FVector2D Axis);

	/** What this hand's stick is set to do. */
	EFXR_HandMovement MovementForHand(EFXR_HandSide Side) const
	{
		return (Side == EFXR_HandSide::Left) ? LeftHandMovement : RightHandMovement;
	}

	/** Begin aiming if this hand may right now (enabled, idle, set to Teleport, not holding anything). */
	void TryBeginTeleportAim(EFXR_HandSide Side);

	/** True if this hand's stick X is free to turn: not holding anything (ADR-005), not strafing. */
	bool CanHandTurn(EFXR_HandSide Side) const;

	/**
	 * The hand a turn should act through. A single assigned hand is used if it can turn; Both takes
	 * whichever can, so turning keeps working while the other hand is occupied. Returns false when
	 * neither can — the mode then yields entirely (ADR-005).
	 */
	bool ResolveTurnHand(EFXR_HandSide& OutSide) const;

	/**
	 * A free hand set to this movement mode, if any. Both hands may carry the same mode, in which
	 * case the one not holding an interactable is used.
	 */
	bool ResolveMovementHand(EFXR_HandMovement Movement, EFXR_HandSide& OutSide) const;
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

	ETeleportPhase Phase = ETeleportPhase::Idle;
	bool bLocomotionEnabled = true;
	bool bTurnEnabled = true;
	// Runtime gates behind SetTeleportEnabled — separate from the authored hand assignment, so a
	// gameplay or SOP lock can suspend a mode without forgetting which hand the designer gave it.
	bool bTeleportEnabled = true;
	bool bInputBound = false;
	bool bLoggedMissingOwner = false;
	bool bLoggedHandFallback = false;

	FVector TargetLocation = FVector::ZeroVector;
	bool bTargetValid = false;
	float TargetFacingYaw = 0.f;
	float FadeElapsed = 0.f;

	/** Latest thumbstick per hand, indexed by EFXR_HandSide — the one place input lands. */
	FVector2D StickAxis[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
	bool bTurnArmed = true;
	/** Which hand owns the teleport arc in flight — resolved when aiming starts, held until it ends. */
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
