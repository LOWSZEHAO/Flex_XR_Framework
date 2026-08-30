// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/HitResult.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractionDriver.generated.h"

class IFXR_Interactor;
class UFXR_Grab;
class UFXR_InteractableBase;
class UFXR_RayTarget;
class UFXR_Socket;

/**
 * UFXR_InteractionDriver — the per-rig service that turns interactor input into grabs.
 *
 * Add it to AFXR_Pawn. Each tick, for both hands, it reads the active interactor's Select
 * value: crossing the grab threshold claims the best candidate from the detection
 * subsystem and begins its interaction; dropping below the release threshold ends it, and
 * OnUpdate runs while held. It lives in FXR_Interaction (not on the pawn) because FXR_Core
 * must never depend on FXR_Interaction.
 *
 * It also owns far-ray aiming, because near and far interaction have to be arbitrated in one
 * place: a hand that can reach something never also points at what is behind it.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_InteractionDriver : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_InteractionDriver();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The interactable currently held by the given hand, or null. */
	UFXR_InteractableBase* GetHeldInteractable(EFXR_HandSide Side) const;

	/** The ray target the given hand is currently pointing at, or null. */
	UFXR_RayTarget* GetAimedRayTarget(EFXR_HandSide Side) const;

protected:
	/** Select value at or above which a grab is claimed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrabThreshold = 0.5f;

	/** Select value below which the current hold is released (kept under GrabThreshold for hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseThreshold = 0.35f;

	/** How far this rig casts its far ray. Each FXR_RayTarget may shorten its own reach below this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Far Interaction", meta = (ClampMin = "1.0", Units = "cm"))
	float RayLength = 2000.f;

private:
	void DriveHand(EFXR_HandSide Side, TWeakObjectPtr<UFXR_InteractableBase>& Held, float& PrevSelect, float DeltaTime);
	/** Offer this hand's fingertip to presses in range (FXR_Press travel). */
	void DrivePokes(EFXR_HandSide Side);

	/**
	 * Find the nearest socket willing to take what this hand is carrying, and preview it. Run after
	 * the hands, so the release path can consult last frame's preview and seat instead of dropping.
	 */
	void DriveSockets(EFXR_HandSide Side, TWeakObjectPtr<UFXR_Socket>& PreviewSocket);

	/**
	 * Publish this hand's hover/selected into the focus subsystem, for highlight and UI to read,
	 * and resolve its far ray. Select and PrevSelect are sampled before DriveHand consumes the
	 * edge, so a ray selection sees the same press a grab would have.
	 */
	void PublishFocus(EFXR_HandSide Side, const TWeakObjectPtr<UFXR_InteractableBase>& Held,
		TWeakObjectPtr<UFXR_RayTarget>& Aimed, float Select, float PrevSelect);

	/** Cast both hands' far rays once per frame; hover and the distance-grab claim share the result. */
	void UpdateFarHits();
	bool CastFarRay(EFXR_HandSide Side, FHitResult& OutHit) const;
	bool GetFarHit(EFXR_HandSide Side, FHitResult& OutHit) const;

	/** What this hand's far ray currently offers: a ray target, else a summonable object. */
	UFXR_InteractableBase* ResolveFarTarget(EFXR_HandSide Side) const;
	UFXR_RayTarget* TraceRayTarget(EFXR_HandSide Side) const;
	UFXR_Grab* TraceDistanceGrab(EFXR_HandSide Side) const;

	/** Fire enter/exit as the aimed target changes, so listeners never strand a prompt on. */
	void UpdateAimed(EFXR_HandSide Side, TWeakObjectPtr<UFXR_RayTarget>& Aimed, UFXR_RayTarget* Now);

	float ReadSelect(EFXR_HandSide Side) const;
	IFXR_Interactor* GetActiveInteractor(EFXR_HandSide Side) const;

	TWeakObjectPtr<UFXR_InteractableBase> LeftHeld;
	TWeakObjectPtr<UFXR_InteractableBase> RightHeld;

	// Grab is claimed on the rising edge of Select, so holding grip and sweeping the hand through
	// the world cannot vacuum up whatever it passes.
	float LeftPrevSelect = 0.f;
	float RightPrevSelect = 0.f;

	// What each hand's ray rests on, kept so enter/exit fire on change rather than every frame.
	TWeakObjectPtr<UFXR_RayTarget> LeftAimed;
	TWeakObjectPtr<UFXR_RayTarget> RightAimed;

	// The socket each hand's carried object is hovering, so releasing hands it over rather than
	// dropping it on the floor.
	TWeakObjectPtr<UFXR_Socket> LeftPreviewSocket;
	TWeakObjectPtr<UFXR_Socket> RightPreviewSocket;

	// This frame's far-ray hits. Cached because three consumers want them and a line trace per
	// consumer per hand adds up on a Quest frame budget.
	FHitResult LeftFarHit;
	FHitResult RightFarHit;
	bool bLeftFarHit = false;
	bool bRightFarHit = false;
};
