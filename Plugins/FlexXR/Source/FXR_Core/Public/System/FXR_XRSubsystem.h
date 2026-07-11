// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_DeviceCapabilities.h"
#include "FXR_XRSubsystem.generated.h"

/** Fired when the active presentation mode (VR / MR) changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FFXR_OnModeChanged, EFXR_Mode);

/**
 * UFXR_XRSubsystem — the platform / session brain of FXR_Core.
 *
 * Resolves device capabilities (hands? controllers? standalone? passthrough?) at
 * startup, owns the active EFXR_Mode (VR / MR), and signals when the active interactor
 * set should change (controllers <-> hands). Interaction code asks this what is
 * available; it never queries the OpenXR runtime directly.
 */
UCLASS()
class FXR_CORE_API UFXR_XRSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Current device capabilities (refreshed by RefreshCapabilities). */
	UFUNCTION(BlueprintPure, Category = "FlexXR|XR")
	FFXR_DeviceCapabilities GetCapabilities() const { return Capabilities; }

	/** Re-query the XR runtime; broadcasts OnActiveInteractorsChanged if the input picture changed. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|XR")
	void RefreshCapabilities();

	/** Active presentation mode. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|XR")
	EFXR_Mode GetMode() const { return Mode; }

	/** Switch presentation mode (VR <-> MR). */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|XR")
	void SetMode(EFXR_Mode NewMode);

	/** Fired when Mode changes (native). */
	FFXR_OnModeChanged OnModeChanged;

	/** Fired when the preferred active interactor set changes (e.g. controllers set down -> hands). */
	FSimpleMulticastDelegate OnActiveInteractorsChanged;

private:
	FFXR_DeviceCapabilities DetectCapabilities() const;

	FFXR_DeviceCapabilities Capabilities;
	EFXR_Mode Mode = EFXR_Mode::VR;
};
