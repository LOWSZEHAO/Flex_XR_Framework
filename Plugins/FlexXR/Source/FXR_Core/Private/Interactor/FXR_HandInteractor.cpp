// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactor/FXR_HandInteractor.h"
#include "Features/IModularFeatures.h"
#include "IHandTracker.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"

UFXR_HandInteractor::UFXR_HandInteractor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_HandInteractor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SampleHandTracking();
}

IHandTracker* UFXR_HandInteractor::GetHandTracker() const
{
	IModularFeatures& Features = IModularFeatures::Get();
	const FName FeatureName = IHandTracker::GetModularFeatureName();
	if (Features.IsModularFeatureAvailable(FeatureName))
	{
		TArray<IHandTracker*> Trackers = Features.GetModularFeatureImplementations<IHandTracker>(FeatureName);
		if (Trackers.Num() > 0)
		{
			return Trackers[0];
		}
	}
	return nullptr;
}

void UFXR_HandInteractor::SampleHandTracking()
{
	IHandTracker* Tracker = GetHandTracker();
	if (!Tracker || !Tracker->IsHandTrackingStateValid())
	{
		bHasValidHand = false;
		SelectValue = 0.f;
		UseValue = 0.f;
		return;
	}

	const EControllerHand Hand = (HandSide == EFXR_HandSide::Left) ? EControllerHand::Left : EControllerHand::Right;

	FTransform Palm, Thumb, Index;
	float Radius = 0.f;
	const bool bPalm = Tracker->GetKeypointState(Hand, EHandKeypoint::Palm, Palm, Radius);
	const bool bThumb = Tracker->GetKeypointState(Hand, EHandKeypoint::ThumbTip, Thumb, Radius);
	const bool bIndex = Tracker->GetKeypointState(Hand, EHandKeypoint::IndexTip, Index, Radius);

	if (bPalm)
	{
		CachedPalm = Palm;
		bHasValidHand = true;
	}

	if (bThumb && bIndex)
	{
		const float PinchDistance = FVector::Dist(Thumb.GetLocation(), Index.GetLocation());
		SelectValue = 1.f - FMath::Clamp(PinchDistance / PinchDistanceThreshold, 0.f, 1.f);
		// TODO(FXR_Core): derive Use (index-squeeze) from index-finger curl independently of the pinch.
		UseValue = SelectValue;
	}
}

FTransform UFXR_HandInteractor::GetTrackedTransform() const
{
	return bHasValidHand ? CachedPalm : Super::GetTrackedTransform();
}
