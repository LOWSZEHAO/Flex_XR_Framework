// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "FXR_ControllerInteractor.generated.h"

class UMotionControllerComponent;
class UEnhancedInputComponent;
class UFXR_InputConfig;
struct FInputActionValue;

/**
 * UFXR_ControllerInteractor — motion-controller input source.
 *
 * Owns a UMotionControllerComponent for the tracked pose and reads Select/Use from
 * EnhancedInput (grip -> Select, trigger -> Use, mapped in a UFXR_InputConfig). The
 * owning pawn calls BindInput to wire the actions.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_CORE_API UFXR_ControllerInteractor : public UFXR_InteractorComponent
{
	GENERATED_BODY()

public:
	UFXR_ControllerInteractor();

	virtual EFXR_InteractorType GetInteractorType() const override { return EFXR_InteractorType::MotionController; }
	virtual float GetSelectValue() const override { return SelectValue; }
	virtual float GetUseValue() const override { return UseValue; }
	virtual void SendHapticFeedback(float Amplitude, float Duration) override;

	/** Bind this interactor's Select/Use to the config's input actions on the given component (called by the pawn). */
	void BindInput(UEnhancedInputComponent* InputComponent, const UFXR_InputConfig* InputConfig);

protected:
	virtual void BeginPlay() override;
	virtual FTransform GetTrackedTransform() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	TObjectPtr<UMotionControllerComponent> MotionController;

private:
	void HandleSelect(const FInputActionValue& Value);
	void HandleUse(const FInputActionValue& Value);

	float SelectValue = 0.f;
	float UseValue = 0.f;
};
