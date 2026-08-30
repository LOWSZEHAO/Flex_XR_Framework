// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_TeleportBlocker.h"
#include "Detection/FXR_TeleportRegistry.h"
#include "DrawDebugHelpers.h"

UFXR_TeleportBlocker::UFXR_TeleportBlocker()
{
	// Tick exists only for the optional debug draw, and runs in the editor too — a blocked volume
	// is invisible by nature, so it has to be visible while placing it.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = true;

	// Blocking is decided against BoxExtent, never by stopping the arc on the marker.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CanCharacterStepUpOn = ECB_No;
}

void UFXR_TeleportBlocker::OnRegister()
{
	Super::OnRegister();

	RefreshTickState();
}

void UFXR_TeleportBlocker::BeginPlay()
{
	Super::BeginPlay();

	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->RegisterBlocker(this);
	}
	RefreshTickState();
}

void UFXR_TeleportBlocker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->UnregisterBlocker(this);
	}
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UFXR_TeleportBlocker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshTickState();
}
#endif

void UFXR_TeleportBlocker::RefreshTickState()
{
	SetComponentTickEnabled(bDrawDebug);
}

void UFXR_TeleportBlocker::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawDebug)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		const FTransform Transform = GetComponentTransform();
		DrawDebugBox(World, Transform.GetLocation(), BoxExtent, Transform.GetRotation(), FColor::Red, false, -1.f, 0, 1.f);
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
