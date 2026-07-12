// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_HandVisual.generated.h"

class IFXR_Interactor;
class UFXR_HandPose;

/**
 * UFXR_HandVisual — displayed hand mesh for one rig hand; the first pass of the §5.2
 * hand presentation pipeline.
 *
 * Follows the active interactor's grip pose each tick and exposes a smoothly-blended
 * GripAlpha / TriggerAlpha (0..1) for the hand Anim BP to drive an open->grab blend.
 * Set its Skeletal Mesh + Anim Class in the details panel and add it to AFXR_Pawn.
 *
 * A testing-grade stand-in: the full pipeline (UFXR_HandPose retargeting, tension model,
 * fingertip IK) replaces the raw follow + alpha later, but the alpha concept carries over.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_HandVisual : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UFXR_HandVisual();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Smoothed grip (Select) blend 0..1 — bind the hand Anim BP's open->grab pose to this. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Hand")
	float GetGripAlpha() const { return GripAlpha; }

	/** Smoothed trigger (Use) blend 0..1 — for index-finger / point poses. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Hand")
	float GetTriggerAlpha() const { return TriggerAlpha; }

	/** Blended per-finger curls for the hand Anim BP — the held grip point's authored pose, else a uniform grip-driven curl. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Hand")
	FFXR_FingerCurls GetFingerCurls() const { return FingerCurls; }

	/** Overall grasp (strongest finger curl) — convenience for Anim BPs still driven by a single grasp alpha. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Hand")
	float GetGrasp() const;

protected:
	/** Which hand this mesh represents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Hand")
	EFXR_HandSide HandSide = EFXR_HandSide::Right;

	/** Mesh offset from the interactor grip pose when driven by a controller (align the mesh to the controller grip). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Hand")
	FTransform GripPoseOffset;

	/** Mesh offset when driven by hand tracking — the tracked palm frame differs from the controller grip, so it needs its own alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Hand")
	FTransform HandTrackingPoseOffset;

	/** Follow the active interactor's grip pose each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Hand")
	bool bFollowInteractorPose = true;

	/** FInterpTo speed for the grip/trigger alpha blend (higher = snappier; ~12 gives a ~100 ms feel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Hand", meta = (ClampMin = "0.0"))
	float BlendSpeed = 12.f;

private:
	IFXR_Interactor* ResolveActiveInteractor() const;
	const UFXR_HandPose* ResolveHeldHandPose() const;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Hand", meta = (AllowPrivateAccess = "true"))
	float GripAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Hand", meta = (AllowPrivateAccess = "true"))
	float TriggerAlpha = 0.f;

	FFXR_FingerCurls FingerCurls;
};
