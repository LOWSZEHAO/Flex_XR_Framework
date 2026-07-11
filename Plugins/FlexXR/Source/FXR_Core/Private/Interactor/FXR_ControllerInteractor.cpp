// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactor/FXR_ControllerInteractor.h"
#include "Input/FXR_InputConfig.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UFXR_ControllerInteractor::UFXR_ControllerInteractor()
{
	MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
	if (MotionController)
	{
		MotionController->SetupAttachment(this);
	}
}

void UFXR_ControllerInteractor::BeginPlay()
{
	Super::BeginPlay();

	if (MotionController)
	{
		MotionController->SetTrackingMotionSource(HandSide == EFXR_HandSide::Left ? FName("Left") : FName("Right"));
	}
}

FTransform UFXR_ControllerInteractor::GetTrackedTransform() const
{
	return MotionController ? MotionController->GetComponentTransform() : Super::GetTrackedTransform();
}

void UFXR_ControllerInteractor::BindInput(UEnhancedInputComponent* InputComponent, const UFXR_InputConfig* InputConfig)
{
	if (!InputComponent || !InputConfig)
	{
		return;
	}

	const bool bLeft = (HandSide == EFXR_HandSide::Left);
	UInputAction* SelectAction = bLeft ? InputConfig->SelectActionLeft : InputConfig->SelectActionRight;
	UInputAction* UseAction = bLeft ? InputConfig->UseActionLeft : InputConfig->UseActionRight;

	if (SelectAction)
	{
		InputComponent->BindAction(SelectAction, ETriggerEvent::Triggered, this, &UFXR_ControllerInteractor::HandleSelect);
		InputComponent->BindAction(SelectAction, ETriggerEvent::Completed, this, &UFXR_ControllerInteractor::HandleSelect);
	}
	if (UseAction)
	{
		InputComponent->BindAction(UseAction, ETriggerEvent::Triggered, this, &UFXR_ControllerInteractor::HandleUse);
		InputComponent->BindAction(UseAction, ETriggerEvent::Completed, this, &UFXR_ControllerInteractor::HandleUse);
	}
}

void UFXR_ControllerInteractor::HandleSelect(const FInputActionValue& Value)
{
	SelectValue = Value.Get<float>();
}

void UFXR_ControllerInteractor::HandleUse(const FInputActionValue& Value)
{
	UseValue = Value.Get<float>();
}

void UFXR_ControllerInteractor::SendHapticFeedback(float Amplitude, float Duration)
{
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			const EControllerHand Hand = (HandSide == EFXR_HandSide::Left) ? EControllerHand::Left : EControllerHand::Right;
			PC->SetHapticsByValue(1.f, FMath::Clamp(Amplitude, 0.f, 1.f), Hand);
		}
	}
}
