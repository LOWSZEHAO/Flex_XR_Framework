// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_Interactor.generated.h"

UINTERFACE(MinimalAPI)
class UFXR_Interactor : public UInterface
{
	GENERATED_BODY()
};

/**
 * IFXR_Interactor — the unified input contract every interaction source implements:
 * motion controller, tracked hand, hand-ray, and desktop-sim mouse. Interaction code
 * queries this and never knows which device is active; the active set hot-swaps at
 * runtime (e.g. controllers set down on Quest → tracked hands).
 *
 * These are queried per-tick on hot paths, so they are deliberately plain C++ virtuals,
 * NOT BlueprintNativeEvent (which would cost an Execute_ wrapper on every call).
 */
class IFXR_Interactor
{
	GENERATED_BODY()

public:
	/** Concrete source type — for diagnostics / capability UI only; never branch interaction logic on it. */
	virtual EFXR_InteractorType GetInteractorType() const = 0;

	/** Which hand this interactor drives. */
	virtual EFXR_HandSide GetHandSide() const = 0;

	/** True while this interactor is the active source for its hand. */
	virtual bool IsInteractorActive() const = 0;

	/** World-space transform where a held object is anchored (the grip). */
	virtual FTransform GetGripTransform() const = 0;

	/** World-space origin + orientation for far-ray targeting. */
	virtual FTransform GetAimTransform() const = 0;

	/** World-space palm transform, used for contact (palm-push) drive. */
	virtual FTransform GetPalmTransform() const = 0;

	/** Framework grab-sphere query shape for near broad-phase detection. */
	virtual void GetGrabSphere(FVector& OutCenter, float& OutRadius) const = 0;

	/**
	 * Fingertip probe (design 5.4): the index-tip sphere that drives poke interactions
	 * (FXR_Press travel). The real tracked fingertip on hand tracking; a tuned offset from the
	 * controller pose otherwise. First probe of the fingertip set — procedural grip and
	 * palm-contact points extend this seam later.
	 */
	virtual void GetPokeTip(FVector& OutLocation, float& OutRadius) const = 0;

	/** Framework far-ray query shape: origin + normalized direction. */
	virtual void GetFarRay(FVector& OutOrigin, FVector& OutDirection) const = 0;

	/** Normalized 0..1 select strength — grip on controllers, grab-pinch on tracked hands. */
	virtual float GetSelectValue() const = 0;

	/** Normalized 0..1 use strength — trigger on controllers, index-squeeze on tracked hands. */
	virtual float GetUseValue() const = 0;

	/** Fire a haptic pulse on this interactor's device; no-op where unsupported. */
	virtual void SendHapticFeedback(float Amplitude, float Duration) = 0;
};
