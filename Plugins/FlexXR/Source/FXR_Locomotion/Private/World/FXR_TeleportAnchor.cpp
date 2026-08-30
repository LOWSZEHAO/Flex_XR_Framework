// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "World/FXR_TeleportAnchor.h"
#include "Detection/FXR_TeleportRegistry.h"
#include "DrawDebugHelpers.h"

UFXR_TeleportAnchor::UFXR_TeleportAnchor()
{
	// Tick exists only for the optional debug draw; disabled unless bDrawDebug is set. It runs in
	// the editor too, so the snap radius is visible while placing anchors, not only in play.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bTickInEditor = true;
}

void UFXR_TeleportAnchor::OnRegister()
{
	Super::OnRegister();

	RefreshTickState();
}

void UFXR_TeleportAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->RegisterAnchor(this);
	}
	RefreshTickState();
}

void UFXR_TeleportAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UFXR_TeleportRegistry* Registry = UFXR_TeleportRegistry::Get(this))
	{
		Registry->UnregisterAnchor(this);
	}
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UFXR_TeleportAnchor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Take effect on the tick the box is ticked, rather than at the next BeginPlay.
	RefreshTickState();
}
#endif

void UFXR_TeleportAnchor::RefreshTickState()
{
	SetComponentTickEnabled(bDrawDebug);
}

void UFXR_TeleportAnchor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawDebug)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform Transform = GetComponentTransform();
	DrawDebugCircle(World, Transform.GetLocation() + FVector(0.f, 0.f, 2.f), SnapRadius, 24, FColor::Cyan, false, -1.f, 0, 1.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
	if (bOverrideFacing)
	{
		DrawDebugDirectionalArrow(World, Transform.GetLocation(), Transform.GetLocation() + Transform.GetUnitAxis(EAxis::X) * SnapRadius, 12.f, FColor::Cyan, false, -1.f, 0, 1.f);
	}
}
