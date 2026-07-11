// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Events/FXR_InteractionEvent.h"
#include "FXR_EventBus.generated.h"

/** Native (C++) subscription — used by hot consumers such as the SOP step runner. */
DECLARE_MULTICAST_DELEGATE_OneParam(FFXR_OnInteractionEventNative, const FFXR_InteractionEvent&);

/** Blueprint-facing subscription — the binding surface for game / quest systems. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_OnInteractionEventDynamic, const FFXR_InteractionEvent&, Event);

/**
 * UFXR_EventBus — world-scoped hub for InteractionId events (architecture §5.6 / §11).
 *
 * Lives in FXR_Core so producers (FXR_Interaction) and consumers (FXR_Training, or a
 * game's quest system) share one contract without breaking the one-way module
 * dependency. Broadcasts to both a native multicast (hot C++ consumers) and a
 * Blueprint-assignable multicast (the binding surface).
 */
UCLASS()
class FXR_CORE_API UFXR_EventBus : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Broadcast an interaction event to native and Blueprint subscribers. */
	void Broadcast(const FFXR_InteractionEvent& Event);

	/** Native multicast, for C++ subscribers on hot paths. */
	FFXR_OnInteractionEventNative& OnInteractionEvent() { return NativeEvent; }

	/** Blueprint-assignable multicast — the binding surface. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Events")
	FFXR_OnInteractionEventDynamic OnInteractionEventBP;

	/** Resolve the event bus from any world context object (may return null). */
	static UFXR_EventBus* Get(const UObject* WorldContextObject);

private:
	FFXR_OnInteractionEventNative NativeEvent;
};
