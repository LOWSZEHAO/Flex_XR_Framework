// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_TeleportBlocker.h"
#include "Detection/FXR_TeleportRegistry.h"
#include "DrawDebugHelpers.h"

UFXR_TeleportBlocker::UFXR_TeleportBlocker()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_TeleportBlocker::BeginPlay()
{
	Super::BeginPlay();

	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->RegisterBlocker(this);
	}
	SetComponentTickEnabled(bDrawDebug);
}

void UFXR_TeleportBlocker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->UnregisterBlocker(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UFXR_TeleportBlocker::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDrawDebug)
	{
		if (const UWorld* World = GetWorld())
		{
			const FTransform Transform = GetComponentTransform();
			DrawDebugBox(World, Transform.GetLocation(), BoxExtent, Transform.GetRotation(), FColor::Red, false, -1.f, 0, 1.f);
		}
	}
}

bool UFXR_TeleportBlocker::IsInside(const FVector& Location) const
{
	// Rotation + translation only (no scale): BoxExtent is world-cm along the component's own axes.
	const FVector Local = GetComponentQuat().UnrotateVector(Location - GetComponentLocation());
	return FMath::Abs(Local.X) <= BoxExtent.X
		&& FMath::Abs(Local.Y) <= BoxExtent.Y
		&& FMath::Abs(Local.Z) <= BoxExtent.Z;
}
