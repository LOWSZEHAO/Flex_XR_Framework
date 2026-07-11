// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "System/FXR_XRSubsystem.h"
#include "Types/FXR_LogChannels.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "Features/IModularFeatures.h"
#include "IHandTracker.h"

void UFXR_XRSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Capabilities = DetectCapabilities();
	UE_LOG(LogFXR, Log,
		TEXT("FXR_XRSubsystem initialised: controllers=%d handTracking=%d standalone=%d passthrough=%d"),
		Capabilities.bHasMotionControllers, Capabilities.bHasHandTracking,
		Capabilities.bIsStandalone, Capabilities.bSupportsPassthrough);
}

void UFXR_XRSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFXR_XRSubsystem::RefreshCapabilities()
{
	const FFXR_DeviceCapabilities Previous = Capabilities;
	Capabilities = DetectCapabilities();

	if (Capabilities.bHasMotionControllers != Previous.bHasMotionControllers ||
		Capabilities.bHasHandTracking != Previous.bHasHandTracking)
	{
		OnActiveInteractorsChanged.Broadcast();
	}
}

void UFXR_XRSubsystem::SetMode(EFXR_Mode NewMode)
{
	if (Mode != NewMode)
	{
		Mode = NewMode;
		OnModeChanged.Broadcast(Mode);
	}
}

FFXR_DeviceCapabilities UFXR_XRSubsystem::DetectCapabilities() const
{
	FFXR_DeviceCapabilities Caps;

	// Hand tracking: any registered IHandTracker modular feature (OpenXRHandTracking provides one).
	Caps.bHasHandTracking =
		IModularFeatures::Get().IsModularFeatureAvailable(IHandTracker::GetModularFeatureName());

	if (GEngine && GEngine->XRSystem.IsValid())
	{
		const IHeadMountedDisplay* Hmd = GEngine->XRSystem->GetHMDDevice();

		// Provisional: an active HMD implies a motion-controller-capable session.
		// TODO(FXR_Core): enumerate tracked XR controller devices once the interactors land.
		Caps.bHasMotionControllers = (Hmd != nullptr);

#if PLATFORM_ANDROID
		// Mobile HMD build target (e.g. Meta Quest standalone).
		Caps.bIsStandalone = true;
#endif

		// TODO(FXR_Core): query the OpenXR passthrough extension in the MR phase.
		Caps.bSupportsPassthrough = false;
	}

	return Caps;
}
