// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_ClimbHold.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_ClimbHoldEvent);

/**
 * UFXR_ClimbHold - something the player can climb by grabbing it: a ladder rung, a ledge, a pipe.
 *
 * Almost empty on purpose. It's a normal interactable (ADR-003) and grabbing it uses the normal
 * FXR_Grab detection; this class just marks it as climbable so UFXR_Locomotion knows. The rig is
 * moved by the locomotion component, not from here, so all player movement goes through one
 * place (ADR-005).
 *
 * Both hands can hold it at once, and hand-over-hand across holds works because each grab
 * re-anchors to where that hand is.
 *
 * The reach sphere debug draw shows in the editor viewport too (like the teleport anchor and
 * blocker), since you place a hold by judging where a hand can reach it.
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

	/**
	 * Reach is centred on the hold itself rather than on the mesh it sits on, so moving the
	 * component moves where you grab, and one mesh can carry several holds (a ladder's rungs).
	 */
	virtual FVector GetInteractionLocation() const override { return GetComponentLocation(); }

	//~ Two hands are the point of a climb, so a second one always may join.
	virtual bool CanBeginSecondary(IFXR_Interactor* Interactor) const override { return true; }
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnBeginSecondary(IFXR_Interactor* Interactor) override;
	virtual void ReleaseHand(IFXR_Interactor* Interactor, EFXR_EndReason Reason) override;
	virtual void OnEnd(EFXR_EndReason Reason) override;

	/** How many hands are on this hold right now. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|ClimbHold")
	int32 GetHandCount() const { return HandCount; }

	/** A hand took hold of this - the first one, or a second joining it. */
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
