// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactor/FXR_HandInteractor.h"
#include "Features/IModularFeatures.h"
#include "IHandTracker.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_HandInteractor_Sample);

	IHandTracker* Tracker = GetHandTracker();
	if (!Tracker || !Tracker->IsHandTrackingStateValid())
	{
		bHasValidHand = false;
		bHasValidIndexTip = false;
		SelectValue = 0.f;
		UseValue = 0.f;
		NavigateValue = 0.f;
		return;
	}

	const EControllerHand Hand = (HandSide == EFXR_HandSide::Left) ? EControllerHand::Left : EControllerHand::Right;

	FTransform Palm, Thumb, Index, Middle;
	float Radius = 0.f;
	float IndexRadius = 0.f;
	const bool bPalm = Tracker->GetKeypointState(Hand, EHandKeypoint::Palm, Palm, Radius);
	const bool bThumb = Tracker->GetKeypointState(Hand, EHandKeypoint::ThumbTip, Thumb, Radius);
	const bool bIndex = Tracker->GetKeypointState(Hand, EHandKeypoint::IndexTip, Index, IndexRadius);
	const bool bMiddle = Tracker->GetKeypointState(Hand, EHandKeypoint::MiddleTip, Middle, Radius);

	if (bPalm)
	{
		CachedPalm = Palm;
		bHasValidHand = true;
	}

	// The tracked index tip is the poke probe (FXR_Press) — the real fingertip, per-user scaled.
	bHasValidIndexTip = bIndex;
	if (bIndex)
	{
		CachedIndexTip = Index.GetLocation();
		CachedIndexTipRadius = (IndexRadius > KINDA_SMALL_NUMBER) ? IndexRadius : PokeRadius;
	}

	const auto PinchStrength = [this](const FVector& ThumbTip, const FVector& FingerTip)
	{
		return 1.f - FMath::Clamp(static_cast<float>(FVector::Dist(ThumbTip, FingerTip)) / PinchDistanceThreshold, 0.f, 1.f);
	};

	if (bThumb && bIndex)
	{
		SelectValue = PinchStrength(Thumb.GetLocation(), Index.GetLocation());
		// TODO(FXR_Core): derive Use (index-squeeze) from index-finger curl independently of the pinch.
		UseValue = SelectValue;
	}

	// Middle-finger pinch is the locomotion verb, so a hand can reach for an object and travel
	// without the two gestures ever being the same one (matches the Meta Interaction SDK split).
	NavigateValue = (bThumb && bMiddle) ? PinchStrength(Thumb.GetLocation(), Middle.GetLocation()) : 0.f;
}

FTransform UFXR_HandInteractor::GetTrackedTransform() const
{
	return bHasValidHand ? CachedPalm : Super::GetTrackedTransform();
}

void UFXR_HandInteractor::GetPokeTip(FVector& OutLocation, float& OutRadius) const
{
	if (bHasValidIndexTip)
	{
		OutLocation = CachedIndexTip;
		OutRadius = CachedIndexTipRadius;
		return;
	}
	Super::GetPokeTip(OutLocation, OutRadius);
}
