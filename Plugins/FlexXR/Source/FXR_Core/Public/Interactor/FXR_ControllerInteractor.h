// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "FXR_ControllerInteractor.generated.h"

class UEnhancedInputComponent;
class UFXR_InputConfig;
struct FInputActionValue;

/**
 * UFXR_ControllerInteractor — motion-controller input source.
 *
 * Reads Select/Use from EnhancedInput (grip -> Select, trigger -> Use, mapped in a
 * UFXR_InputConfig). The owning rig parents this under a UMotionControllerComponent, so
 * this component's inherited transform is the tracked controller pose. The owning pawn
 * calls BindInput to wire the actions.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_CORE_API UFXR_ControllerInteractor : public UFXR_InteractorComponent
{
	GENERATED_BODY()

public:
	virtual EFXR_InteractorType GetInteractorType() const override { return EFXR_InteractorType::MotionController; }
	virtual float GetSelectValue() const override { return SelectValue; }
	virtual float GetUseValue() const override { return UseValue; }
	virtual void SendHapticFeedback(float Amplitude, float Duration) override;

	/** Bind this interactor's Select/Use to the config's per-hand input actions on the given component (called by the pawn). */
	void BindInput(UEnhancedInputComponent* InputComponent, const UFXR_InputConfig* InputConfig);

private:
	void HandleSelect(const FInputActionValue& Value);
	void HandleUse(const FInputActionValue& Value);

	float SelectValue = 0.f;
	float UseValue = 0.f;
};
