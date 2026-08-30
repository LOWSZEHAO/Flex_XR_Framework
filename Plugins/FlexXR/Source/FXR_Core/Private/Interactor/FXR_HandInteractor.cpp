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
		TriggerValue = 0.f;
		return;
	}

	const EControllerHand Hand = (HandSide == EFXR_HandSide::Left) ? EControllerHand::Left : EControllerHand::Right;

	FTransform Palm, Thumb, Index;
	float Radius = 0.f;
	float IndexRadius = 0.f;
	const bool bPalm = Tracker->GetKeypointState(Hand, EHandKeypoint::Palm, Palm, Radius);
	const bool bThumb = Tracker->GetKeypointState(Hand, EHandKeypoint::ThumbTip, Thumb, Radius);
	const bool bIndex = Tracker->GetKeypointState(Hand, EHandKeypoint::IndexTip, Index, IndexRadius);

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

	if (bThumb && bIndex)
	{
		const float PinchDistance = FVector::Dist(Thumb.GetLocation(), Index.GetLocation());
		SelectValue = 1.f - FMath::Clamp(PinchDistance / PinchDistanceThreshold, 0.f, 1.f);
		// TODO(FXR_Core): derive the trigger (index-squeeze) from index-finger curl independently of the pinch.
		TriggerValue = SelectValue;
	}
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
