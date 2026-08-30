// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "FXR_HandInteractor.generated.h"

class IHandTracker;

/**
 * UFXR_HandInteractor — articulated hand-tracking input source.
 *
 * Samples OpenXR hand joints each tick: the palm keypoint drives the tracked pose, and
 * thumb-tip / index-tip separation drives Select (pinch). Falls back to this component's
 * transform when no valid hand data is present.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_CORE_API UFXR_HandInteractor : public UFXR_InteractorComponent
{
	GENERATED_BODY()

public:
	UFXR_HandInteractor();

	virtual EFXR_InteractorType GetInteractorType() const override { return EFXR_InteractorType::TrackedHand; }
	virtual float GetSelectValue() const override { return SelectValue; }
	virtual float GetTriggerValue() const override { return TriggerValue; }
	virtual void GetPokeTip(FVector& OutLocation, float& OutRadius) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual FTransform GetTrackedTransform() const override;

	/** Thumb-tip to index-tip distance (cm) at which Select reads 0; touching reads 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor", meta = (ClampMin = "0.5"))
	float PinchDistanceThreshold = 3.f;

private:
	IHandTracker* GetHandTracker() const;
	void SampleHandTracking();

	FTransform CachedPalm = FTransform::Identity;
	FVector CachedIndexTip = FVector::ZeroVector;
	float CachedIndexTipRadius = 1.f;
	bool bHasValidHand = false;
	bool bHasValidIndexTip = false;
	float SelectValue = 0.f;
	float TriggerValue = 0.f;
};
