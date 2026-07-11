// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Events/FXR_EventBus.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UFXR_EventBus::Broadcast(const FFXR_InteractionEvent& Event)
{
	NativeEvent.Broadcast(Event);
	OnInteractionEventBP.Broadcast(Event);
}

UFXR_EventBus* UFXR_EventBus::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_EventBus>();
	}

	return nullptr;
}
