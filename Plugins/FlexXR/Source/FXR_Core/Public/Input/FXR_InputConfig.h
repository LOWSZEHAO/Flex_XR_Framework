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
 * Holds the mapping context plus the Select and Use actions so controller mapping
 * (grip/trigger <-> Select/Use) is authored as data, not hardcoded. The pawn applies
 * the context and passes this to each controller interactor's BindInput.
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

	/** Select (grab) — grip on controllers, grab-pinch on hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> SelectAction;

	/** Use — trigger on controllers, index-squeeze on hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UInputAction> UseAction;
};
