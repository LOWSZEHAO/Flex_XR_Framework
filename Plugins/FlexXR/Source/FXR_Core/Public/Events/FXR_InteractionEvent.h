// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractionEvent.generated.h"

class AActor;

/**
 * A single interaction event broadcast on the FXR event bus. Interactables flagged
 * "Expose to Training" emit these by InteractionId (e.g. "Pull_Pin_Extinguisher01");
 * the SOP step runner, analytics, or game quest systems subscribe. Consumers that
 * ignore the bus pay nothing.
 */
USTRUCT(BlueprintType)
struct FFXR_InteractionEvent
{
	GENERATED_BODY()

	/** Author-assigned identifier for the interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Events")
	FName InteractionId;

	/** Lifecycle phase of this event. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Events")
	EFXR_InteractionPhase Phase = EFXR_InteractionPhase::Began;

	/** Hand that produced the interaction. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Events")
	EFXR_HandSide HandSide = EFXR_HandSide::Right;

	/** Optional analog payload (e.g. use value 0..1); 0 when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Events")
	float Value = 0.f;

	/** The interactable actor that emitted the event (may be null). */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Events")
	TWeakObjectPtr<AActor> Instigator;
};
