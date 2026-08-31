// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_InteractableBase.generated.h"

class IFXR_Interactor;
class UPrimitiveComponent;
class UFXR_GripPoint;
class UFXR_HandPose;

/**
 * UFXR_InteractableBase — shared base for FlexXR interactables (Grab, Latch, Press, ...).
 *
 * Per ADR-003 there is deliberately no IFXR_Interactable interface: this class's virtual
 * lifecycle IS the extension contract. Subclass it and override CanBegin / OnBegin /
 * OnUpdate / OnEnd, and inherit registry registration, the enable API, driven-component
 * resolution, and InteractionId event emission for free.
 *
 * A SceneComponent so it can be attached to (and sit at) the mesh it drives.
 */
UCLASS(Abstract, ClassGroup = (FlexXR))
class FXR_INTERACTION_API UFXR_InteractableBase : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_InteractableBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The everyday gameplay switch (cutscene disable, quest unlock, ...). Honors Already-Held Policy if held. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Interaction")
	void SetInteractionEnabled(bool bEnabled);

	/** End any current hold immediately (EFXR_EndReason::ForceReleased) — disarm, stun, cutscene rip. No-op if not held. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Interaction")
	void ForceRelease();

	UFUNCTION(BlueprintPure, Category = "FlexXR|Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Interaction")
	bool IsHeld() const { return bHeld; }

	//~ Extension contract (ADR-003) — override these in subclasses.
	virtual bool CanBegin(IFXR_Interactor* Interactor) const;
	virtual void OnBegin(IFXR_Interactor* Interactor);
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime);
	virtual void OnEnd(EFXR_EndReason Reason);

	//~ Two-hand contract — defaults are single-hand; interactables that support a second hand
	//~ (FXR_Grab's Allow Two-Handed) override these. The driver never assumes hand roles.
	/** Whether a second hand may join the current hold. */
	virtual bool CanBeginSecondary(IFXR_Interactor* Interactor) const { return false; }
	/** A second hand joined the current hold. */
	virtual void OnBeginSecondary(IFXR_Interactor* Interactor) {}
	/**
	 * A specific hand let go. Default: single-hand — the hold ends. Two-hand overrides detach
	 * just that hand (or promote the survivor) and only end the hold when the last hand leaves.
	 */
	virtual void ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason) { OnEnd(Reason); }

	/** Activation radius (cm) used by the detection broad phase. */
	float GetActivationRadius() const { return ActivationRadius; }

	/** The primitive this interactable moves/affects — the highlight subsystem's Target Mesh scope. */
	UPrimitiveComponent* GetDrivenComponent() const { return ResolveDrivenComponent(); }

	/** Whether any debug draw / viewport gizmo is enabled. */
	bool IsDrawDebugEnabled() const { return DebugDraw != EFXR_DebugDraw::Off; }

	/** Whether thresholds, limits and live state are drawn on top of the basic shape. */
	bool IsFullDebug() const { return DebugDraw == EFXR_DebugDraw::Full; }

	/** World location used for narrow-phase scoring (the driven component, else this component). */
	FVector GetInteractionLocation() const;

	/** Hand pose (finger shape) the given hand should form while this is held, or null. */
	virtual UFXR_HandPose* GetActiveHandPose(EFXR_HandSide Side) const { return nullptr; }

	//~ Grip registry (ADR-007): owning grip points attach themselves here at BeginPlay.
	void RegisterGripPoint(UFXR_GripPoint* GripPoint);
	void UnregisterGripPoint(UFXR_GripPoint* GripPoint);

	/** True if any grip point claims this interactable — grip points are then the only grab surface (ADR-007). */
	bool HasOwnedGripPoints() const { return OwnedGripPoints.Num() > 0; }

	/**
	 * Whether a hand's grab sphere reaches this interactable (ADR-007): with owned grip points,
	 * the points are the only grab surface — the nearest accepting point decides; without, the
	 * activation radius around the driven mesh decides. OutDistanceSq scores the narrow phase.
	 */
	bool IsInGrabReach(const FVector& GrabCenter, float GrabRadius, EFXR_HandSide HandSide, float& OutDistanceSq) const;

	/** Whether grab detection may claim this interactable — poke-driven types (Press) return false. */
	virtual bool IsGrabTarget() const { return true; }

	/**
	 * Whether a hand can ever hold this. Sockets receive and ray targets are pointed at; neither is
	 * ever held, which makes some inherited settings meaningless on them.
	 */
	virtual bool CanEverBeHeld() const { return true; }

#if WITH_EDITOR
	/** Greys out inherited settings that cannot apply to this particular subclass. */
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	/**
	 * While held, the world transform the hand mesh should sit at (e.g. a handle grip point), so the
	 * hand tracks the object instead of the controller. Return false to follow the interactor grip
	 * (the default — Grab moves the object to the hand, so the hand stays on the controller).
	 */
	virtual bool GetHandAttachTransform(EFXR_HandSide Side, FTransform& OutTransform) const { return false; }

protected:
	/** The primitive this interactable moves/affects: the attach-parent primitive, else the actor root primitive. */
	UPrimitiveComponent* ResolveDrivenComponent() const;

	/** Best owned grip point for this interactor's hand (highest priority, then nearest overlapping), or null. */
	UFXR_GripPoint* SelectGripPoint(IFXR_Interactor* Interactor) const;

	/** Emit this interactable's InteractionId on the FXR event bus, if Expose to Training is set. */
	void BroadcastInteractionEvent(EFXR_InteractionPhase Phase, IFXR_Interactor* Interactor);

	/** Runtime debug draw while Draw Debug Radius is set — override to draw a type-specific shape. */
	virtual void DrawInteractionDebug() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;

	/** What happens to an in-progress hold if this interactable is disabled while held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	EFXR_AlreadyHeldPolicy AlreadyHeldPolicy = EFXR_AlreadyHeldPolicy::FinishNaturally;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0"))
	float ActivationRadius = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	bool bExposeToTraining = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (EditCondition = "bExposeToTraining"))
	FName InteractionId;

	/** Authoring debug: Basic draws the defining shape, Full adds thresholds/limits and live state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Debug")
	EFXR_DebugDraw DebugDraw = EFXR_DebugDraw::Off;

	bool bHeld = false;

private:
	/** Grip points owned by this interactable (registered by the points themselves at BeginPlay). */
	TArray<TWeakObjectPtr<UFXR_GripPoint>> OwnedGripPoints;
};
