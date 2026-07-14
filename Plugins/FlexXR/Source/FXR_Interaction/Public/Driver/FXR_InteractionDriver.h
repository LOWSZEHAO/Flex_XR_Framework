// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractionDriver.generated.h"

class IFXR_Interactor;
class UFXR_InteractableBase;

/**
 * UFXR_InteractionDriver — the per-rig service that turns interactor input into grabs.
 *
 * Add it to AFXR_Pawn. Each tick, for both hands, it reads the active interactor's Select
 * value: crossing the grab threshold claims the best candidate from the detection
 * subsystem and begins its interaction; dropping below the release threshold ends it, and
 * OnUpdate runs while held. It lives in FXR_Interaction (not on the pawn) because FXR_Core
 * must never depend on FXR_Interaction.
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

protected:
	/** Select value at or above which a grab is claimed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrabThreshold = 0.5f;

	/** Select value below which the current hold is released (kept under GrabThreshold for hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseThreshold = 0.35f;

private:
	void DriveHand(EFXR_HandSide Side, TWeakObjectPtr<UFXR_InteractableBase>& Held, float DeltaTime);
	/** Offer this hand's fingertip to presses in range (FXR_Press travel). */
	void DrivePokes(EFXR_HandSide Side);
	IFXR_Interactor* GetActiveInteractor(EFXR_HandSide Side) const;

	TWeakObjectPtr<UFXR_InteractableBase> LeftHeld;
	TWeakObjectPtr<UFXR_InteractableBase> RightHeld;
};
