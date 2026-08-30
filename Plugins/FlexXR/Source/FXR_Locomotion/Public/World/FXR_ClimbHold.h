// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_ClimbHold.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_ClimbHoldEvent);

/**
 * UFXR_ClimbHold — a surface the player can pull themselves along: a ladder rung, a ledge, a pipe.
 *
 * Deliberately almost empty. It is an interactable like any other (ADR-003: subclass the base, no
 * interface), and grabbing it is ordinary FXR_Grab detection — this class only marks the hold as
 * *climbable* so UFXR_Locomotion recognises it. The play space is moved by the locomotion
 * component, never from here, because ADR-005 puts every metre the player travels through one
 * arbiter; a hold that moved the rig itself would be a second locomotion system.
 *
 * Both hands may hold it at once, and hand-over-hand across separate holds works because each
 * grab re-anchors against wherever that hand now is.
 *
 * Its Debug Draw reach sphere shows in the level viewport as well as in play, matching the teleport
 * anchor and blocker: a hold is placed by reaching for it, so its reach has to be visible while you
 * position it. The Phase 2 interactables use selection-gated viewport visualizers instead.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_ClimbHold : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	UFXR_ClimbHold();

	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ Two hands are the point of a climb, so a second one always may join.
	virtual bool CanBeginSecondary(IFXR_Interactor* Interactor) const override { return true; }
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnBeginSecondary(IFXR_Interactor* Interactor) override;
	virtual void ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason) override;
	virtual void OnEnd(EFXR_EndReason Reason) override;

	/** How many hands are on this hold right now. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|ClimbHold")
	int32 GetHandCount() const { return HandCount; }

	/** A hand took hold of this — the first one, or a second joining it. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|ClimbHold")
	FFXR_ClimbHoldEvent OnGrabbed;

	/** A hand let go. Fires per hand, so a two-handed hold reports twice. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|ClimbHold")
	FFXR_ClimbHoldEvent OnReleased;

private:
	/** The base draws the reach sphere on tick; this decides whether that tick runs at all. */
	void RefreshTickState();

	int32 HandCount = 0;
};
