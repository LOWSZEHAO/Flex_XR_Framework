// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_Grab.generated.h"

class UPrimitiveComponent;

/**
 * UFXR_Grab — free 6-DOF grab.
 *
 * Kinematic-while-held (ADR-001): on grab the driven primitive stops simulating and
 * follows the interactor's grip pose; on release it resumes physics. This is the first
 * pass of the solver behaviour (SetWorldTransform for now); a later slice swaps in the
 * kinematic-target constraint solver and release velocity.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Grab : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime) override;
	virtual void OnEnd(EFXR_EndReason Reason) override;

protected:
	/** Multiplier on the hand's tracked velocity handed to the object on release — tune throw strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab", meta = (ClampMin = "0.0"))
	float ThrowVelocityScale = 1.f;

private:
	FTransform HeldOffset = FTransform::Identity;
	bool bRestorePhysics = false;
	TWeakObjectPtr<UPrimitiveComponent> HeldComponent;

	FVector LastLocation = FVector::ZeroVector;
	FQuat LastRotation = FQuat::Identity;
	FVector TrackedLinearVelocity = FVector::ZeroVector;
	FVector TrackedAngularVelocity = FVector::ZeroVector;
};
