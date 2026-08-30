// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "FXR_DesktopSimInteractor.generated.h"

/**
 * UFXR_DesktopSimInteractor — mouse/keyboard input source for PIE without a headset.
 *
 * Poses come from this component's transform (the pawn drives it from the mouse); Select
 * and Trigger are pushed in by the pawn's desktop-sim input. Gives interaction the same
 * IFXR_Interactor surface as a real device so feel can be iterated at a desk.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_CORE_API UFXR_DesktopSimInteractor : public UFXR_InteractorComponent
{
	GENERATED_BODY()

public:
	virtual EFXR_InteractorType GetInteractorType() const override { return EFXR_InteractorType::DesktopSim; }
	virtual float GetSelectValue() const override { return SelectValue; }
	virtual float GetTriggerValue() const override { return TriggerValue; }

	/** Set by the pawn's desktop-sim input (e.g. left mouse). */
	void SetSelectValue(float Value) { SelectValue = FMath::Clamp(Value, 0.f, 1.f); }

	/** Set by the pawn's desktop-sim input (e.g. right mouse). */
	void SetTriggerValue(float Value) { TriggerValue = FMath::Clamp(Value, 0.f, 1.f); }

private:
	float SelectValue = 0.f;
	float TriggerValue = 0.f;
};
