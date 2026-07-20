// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Detection/FXR_TeleportRegistry.h"
#include "World/FXR_TeleportAnchor.h"
#include "World/FXR_TeleportBlocker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UFXR_TeleportRegistry::RegisterAnchor(UFXR_TeleportAnchor* Anchor)
{
	if (Anchor)
	{
		Anchors.AddUnique(Anchor);
	}
}

void UFXR_TeleportRegistry::UnregisterAnchor(UFXR_TeleportAnchor* Anchor)
{
	Anchors.RemoveSingleSwap(Anchor);
}

void UFXR_TeleportRegistry::RegisterBlocker(UFXR_TeleportBlocker* Blocker)
{
	if (Blocker)
	{
		Blockers.AddUnique(Blocker);
	}
}

void UFXR_TeleportRegistry::UnregisterBlocker(UFXR_TeleportBlocker* Blocker)
{
	Blockers.RemoveSingleSwap(Blocker);
}

UFXR_TeleportAnchor* UFXR_TeleportRegistry::FindAnchorNear(const FVector& Location) const
{
	UFXR_TeleportAnchor* Best = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const TObjectPtr<UFXR_TeleportAnchor>& Anchor : Anchors)
	{
		if (!Anchor)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(Location, Anchor->GetComponentLocation());
		if (DistanceSq <= FMath::Square(Anchor->GetSnapRadius()) && DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Anchor;
		}
	}

	return Best;
}

bool UFXR_TeleportRegistry::IsBlocked(const FVector& Location) const
{
	for (const TObjectPtr<UFXR_TeleportBlocker>& Blocker : Blockers)
	{
		if (Blocker && Blocker->IsInside(Location))
		{
			return true;
		}
	}
	return false;
}

UFXR_TeleportRegistry* UFXR_TeleportRegistry::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_TeleportRegistry>();
	}
	return nullptr;
}
