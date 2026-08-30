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
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCameraComponent;
class UFXR_InteractorComponent;
class UFXR_InteractionDriver;
class UFXR_TeleportRegistry;
class UFXR_TeleportAnchor;
class UFXR_TeleportBlocker;
class UFXR_ClimbHold;
class IFXR_Interactor;
struct FInputActionValue;

/**
 * UFXR_Locomotion — the single locomotion component (ADR-005): teleport, smooth move, turn, and
 * comfort, arbitrated internally rather than split into four components.
 *
 * The control layout is described per hand rather than per mode, because a thumbstick has only two
 * axes to give: forward is that hand's movement (teleport or smooth move — never both), sideways is
 * its turn, or strafe when it has no turn mode. Saying it this way makes an unplayable layout
 * impossible to express instead of something to detect and warn about.
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

	/** True while a teleport arc is being aimed. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Locomotion")
	bool IsAimingTeleport() const { return Phase == ETeleportPhase::Aiming; }

	/** Current comfort-vignette intensity 0..1 — bind to a post-process/overlay if not using Vignette Material. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Locomotion")
	float GetVignetteIntensity() const { return VignetteIntensity; }

protected:
	//~ Hands — the whole control layout. Each hand's stick is described independently: forward drives
	//~ its movement, sideways its turn. Set one hand to None for a rig where only the other may move
	//~ (common in guided training); set both the same to give the player either hand.

	/** What the left stick's forward axis does. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hands", meta = (DisplayName = "Left Hand"))
	EFXR_HandMovement LeftHandMovement = EFXR_HandMovement::SmoothMove;

	/** How the left stick's sideways axis turns. None leaves it to strafe, if this hand smooth-moves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hands", meta = (DisplayName = "Left Turn Mode"))
	EFXR_TurnMode LeftHandTurn = EFXR_TurnMode::None;

	/** What the right stick's forward axis does. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hands", meta = (DisplayName = "Right Hand"))
	EFXR_HandMovement RightHandMovement = EFXR_HandMovement::Teleport;

	/** How the right stick's sideways axis turns. None leaves it to strafe, if this hand smooth-moves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hands", meta = (DisplayName = "Right Turn Mode"))
	EFXR_TurnMode RightHandTurn = EFXR_TurnMode::Snap;

	//~ Smooth Move — only the tuning; which hand smooth-moves is set per hand above.
	/** Frame the move stick is relative to (Hip approximates to Head — the rig has no hip tracker). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Smooth Move")
	EFXR_MoveDirectionSource MoveDirectionSource = EFXR_MoveDirectionSource::HeadRelative;

	/** Smooth-move speed (cm/s) — 250 ≈ 2.5 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Smooth Move", meta = (ClampMin = "1.0"))
	float SmoothMoveSpeed = 250.f;

	//~ Teleport
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	EFXR_TeleportAim AimStyle = EFXR_TeleportAim::ProjectileArc;

	/**
	 * How the view crosses to the destination. Fade blacks out and back over Fade Duration — a
	 * short Fade Duration (~0.06 s) is the "blink" comfort option, which is why there is no
	 * separate Blink mode. Dash slides the play space there with the world visible, and is the only
	 * one that creates optical flow, so it is the only one that vignettes. Instant cuts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport")
	EFXR_TeleportTransition Transition = EFXR_TeleportTransition::Fade;

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

	/** View transition length (s) — the fade for Fade, the slide for Dash. Blink and Instant ignore it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Teleport", meta = (ClampMin = "0.0", EditCondition = "Transition == EFXR_TeleportTransition::Fade || Transition == EFXR_TeleportTransition::Dash"))
	float FadeDuration = 0.15f;

	//~ Turning — the feel of a turn; which hand turns, and how, is set per hand above.
	/** Degrees per snap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning", meta = (ClampMin = "1.0", ClampMax = "180.0", EditCondition = "LeftHandTurn == EFXR_TurnMode::Snap || RightHandTurn == EFXR_TurnMode::Snap"))
	float SnapAngle = 30.f;

	/** Degrees per second for smooth turning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Turning", meta = (ClampMin = "1.0", EditCondition = "LeftHandTurn == EFXR_TurnMode::Smooth || RightHandTurn == EFXR_TurnMode::Smooth"))
	float SmoothTurnRate = 90.f;

	//~ Climbing — automatic wherever a FXR_ClimbHold exists; there is no switch to turn it on.
	/**
	 * Downward acceleration (cm/s²) applied after letting go above the floor. Climbing is the only
	 * thing in the rig that puts the player in the air, so this is the only place gravity applies —
	 * walking off a ledge does not fall, by design.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Climbing", meta = (ClampMin = "0.0"))
	float ClimbFallGravity = 980.f;

	/** Terminal speed (cm/s) of that fall. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Climbing", meta = (ClampMin = "1.0"))
	float MaxClimbFallSpeed = 1200.f;

	//~ Comfort
	/**
	 * Peripheral vignette during artificial motion. Dynamic scales with speed; Always is a constant
	 * narrowed FOV. This chooses *how much* vignette to ask for — a Vignette Material (or your own
	 * overlay bound to Get Vignette Intensity) is what actually draws it.
	 */
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
	//~ Pinch and hold to aim, release to teleport — the Meta Interaction SDK convention the Quest
	//~ system UI already teaches, and the same shape as the stick path (hold to aim, let go to go).
	//~
	//~ It is the *middle-finger* pinch (IFXR_Interactor::GetNavigateValue), because index pinch is
	//~ grab. Two fingers, two verbs: a failed reach can never teleport you, and standing beside a
	//~ prop never costs you the ability to travel. There is likewise no palm-orientation gate — the
	//~ palm normal is perpendicular to the aim direction, so any such gate fights aiming near.

	/** Middle-finger pinch strength that raises the arc. Releasing below it teleports. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Hand Tracking", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HandPinchThreshold = 0.7f;

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

	//~ Visuals (all optional; the arc and reticle fall back to debug lines).
	/**
	 * Mesh placed at the teleport target while aiming, replacing the debug circle. It is rotated to
	 * the landing facing, so a directional mesh shows which way you will end up looking. Build it
	 * lying in the XY plane with +X forward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UStaticMesh> ReticleMesh;

	/** Material on the reticle when the target is teleportable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	/** Material on the reticle when the target is rejected — off-nav, too steep, or blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	/** How far above the target the reticle sits (cm). Enough to clear the surface, not enough to float. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals", meta = (ClampMin = "0.0"))
	float ReticleGroundOffset = 2.f;

	/**
	 * Post-process material driven each frame with the vignette intensity, through the scalar named
	 * below. **The vignette draws nothing until this is set** — Vignette Mode only computes the
	 * intensity. Assign a post-process material, or leave this empty and bind Get Vignette Intensity
	 * to your own overlay.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	TObjectPtr<UMaterialInterface> VignetteMaterial;

	/** Scalar parameter on the vignette material that receives the 0..1 intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Locomotion|Visuals")
	FName VignetteIntensityParameter = TEXT("Intensity");

private:
	enum class ETeleportPhase : uint8 { Idle, Aiming, FadingOut, FadingIn, Dashing };

	void TryBindInput();

	void HandleLeftStick(const FInputActionValue& Value);
	void HandleRightStick(const FInputActionValue& Value);
	void HandleLeftStickCompleted();
	void HandleRightStickCompleted();

	/** Store one hand's stick and act on its forward axis; the sideways axis is polled in ProcessTurn. */
	void ApplyStick(EFXR_HandSide Side, FVector2D Axis);

	/** What this hand's stick forward axis does. */
	EFXR_HandMovement MovementForHand(EFXR_HandSide Side) const
	{
		return (Side == EFXR_HandSide::Left) ? LeftHandMovement : RightHandMovement;
	}

	/** What this hand's stick sideways axis does. None means it is free to strafe. */
	EFXR_TurnMode TurnModeForHand(EFXR_HandSide Side) const
	{
		return (Side == EFXR_HandSide::Left) ? LeftHandTurn : RightHandTurn;
	}

	/** Begin aiming if this hand may right now (enabled, idle, set to Teleport, not holding anything). */
	void TryBeginTeleportAim(EFXR_HandSide Side);

	/** Turn from one hand's stick X, in that hand's own mode. Called for each hand every frame. */
	void ProcessTurnForHand(EFXR_HandSide Side, float DeltaTime);

	/** Add one hand's stick contribution to the smooth-move delta; returns false if it contributes none. */
	bool AccumulateSmoothMove(EFXR_HandSide Side, float DeltaTime, FVector& InOutDelta) const;

	/**
	 * A free hand set to this movement mode, if any. Both hands may carry the same mode, in which
	 * case the one not holding an interactable is used.
	 */
	bool ResolveMovementHand(EFXR_HandMovement Movement, EFXR_HandSide& OutSide) const;
	void UpdateAim();
	bool PredictAndValidate(FVector& OutTarget, bool& OutValid, float& OutFacingYaw);

	/**
	 * Drive the aim-enter/exit events on the anchor and blocker under the reticle. Called every aim
	 * frame with what the aim resolved to, and with nulls when aiming ends, so a highlight bound in
	 * Blueprint always gets its closing event.
	 */
	void UpdateAimHover(UFXR_TeleportAnchor* Anchor, UFXR_TeleportBlocker* Blocker);
	void CommitTeleport();
	void ExecuteMove();

	/** Slide the play space toward the committed target over Fade Duration (the Dash transition). */
	void TickDash(float DeltaTime);

	/** The landing facing the stick is asking for, if it is deflected far enough to be asking. */
	bool ResolveThumbstickFacing(float& OutFacingYaw) const;

	/** How long the view transition takes, given the chosen Transition. */
	float GetTransitionDuration() const;
	void ProcessTurn(float DeltaTime);
	void ProcessSmoothMove(float DeltaTime);

	/**
	 * Drag the play space so a hand holding a FXR_ClimbHold stays put in the world — the hand is
	 * the fixed point and the rig moves, which is what pulling yourself up actually is.
	 * Returns true while a hold is being climbed, so the stick modes know to stand down.
	 */
	bool ProcessClimb(float DeltaTime);

	/** Fall to the floor after letting go mid-climb. */
	void ProcessClimbFall(float DeltaTime);

	/** The hold this hand is on, or null. */
	UFXR_ClimbHold* GetClimbHoldFor(EFXR_HandSide Side) const;

	/** Where the given hand grips in world space — the anchor a climb is measured against. */
	FVector GetClimbHandLocation(EFXR_HandSide Side) const;
	void ProcessHandTeleportGesture();
	bool IsHandTracking(EFXR_HandSide Side) const;
	void UpdateVignette(float DeltaTime);
	void ApplyVignetteToMaterial();
	/** Yaw the tracking origin about the HMD (head stays put, world spins — ADR-006). Shared by turn + landing. */
	void ApplyYaw(float DeltaYawDegrees);
	void DrawAim() const;

	/** Place/hide the reticle mesh for this frame's target. No-op until a Reticle Mesh is assigned. */
	void UpdateReticle(bool bVisible) const;
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

	// What the reticle is currently over, so enter/exit fire once on change rather than every frame.
	TWeakObjectPtr<UFXR_TeleportAnchor> HoveredAnchor;
	TWeakObjectPtr<UFXR_TeleportBlocker> HoveredBlocker;

	FVector TargetLocation = FVector::ZeroVector;
	bool bTargetValid = false;
	float TargetFacingYaw = 0.f;
	/** Whether TargetFacingYaw was actually asked for — a centred stick under Thumbstick Choose is not. */
	bool bApplyLandingFacing = false;
	float FadeElapsed = 0.f;

	// Climb state: the world-space point each climbing hand is pinned to, and which hand is driving
	// when both are on. Anchors are re-taken on every fresh grab, so hand-over-hand cannot drift.
	FVector ClimbAnchor[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bClimbing[2] = { false, false };
	EFXR_HandSide ClimbDriver = EFXR_HandSide::Right;
	bool bClimbFalling = false;
	float ClimbFallSpeed = 0.f;

	// Dash slides the play space instead of cutting, so it needs the endpoints held across frames.
	FVector DashStartOrigin = FVector::ZeroVector;
	FVector DashEndOrigin = FVector::ZeroVector;
	float DashElapsed = 0.f;

	/** Latest thumbstick per hand, indexed by EFXR_HandSide — the one place input lands. */
	FVector2D StickAxis[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
	/** Snap re-arm per hand, indexed by EFXR_HandSide — each stick flicks and re-centres on its own. */
	bool bTurnArmed[2] = { true, true };
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

	// Built at BeginPlay only when a Reticle Mesh is assigned, so a project that never sets one pays
	// nothing. Created with NewObject rather than as a default subobject — a component's default
	// subobject does not survive Blueprint SCS instancing.
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ReticleComponent;

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
