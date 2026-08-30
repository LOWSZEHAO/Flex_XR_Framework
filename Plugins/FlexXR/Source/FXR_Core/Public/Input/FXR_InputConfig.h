// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXR_InputConfig.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * UFXR_InputConfig — data-driven EnhancedInput binding for FlexXR interactors.
 *
 * Holds the mapping context plus per-hand Select and Trigger actions so controller mapping
 * (grip -> Select, trigger -> Trigger) is authored as data, not hardcoded, and the left and
 * right controllers never share one action. The pawn applies the context and passes this
 * to each controller interactor's BindInput.
 */
UCLASS(BlueprintType)
class FXR_CORE_API UFXR_InputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Mapping context added to the local player on possession. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Priority for the mapping context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	int32 MappingPriority = 0;

	/** Select (grab) — left hand (grip on controllers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> SelectActionLeft;

	/** Select (grab) — right hand (grip on controllers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> SelectActionRight;

	/** Trigger — left hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> TriggerActionLeft;

	/** Trigger — right hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> TriggerActionRight;
};
