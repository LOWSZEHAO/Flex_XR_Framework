// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Press.h"
#include "Interactor/FXR_Interactor.h"
#include "Types/FXR_LogChannels.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

UFXR_Press::UFXR_Press()
{
	// A press ticks while poked (cap follow) and while returning to rest; idle it sleeps.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_Press::BeginPlay()
{
	Super::BeginPlay();

	// The component's own transform is the rest face; cache it (and the cap rest) at t=0 so a
	// press parented under the cap it moves doesn't chase its own moving transform.
	Driven = ResolveDrivenComponent();
	FaceRestWorld = GetComponentTransform();
	if (Driven.IsValid())
	{
		DrivenRestWorld = Driven->GetComponentTransform();

		// A button cap is mechanism, not loose physics — driven kinematically along its travel.
		if (UPrimitiveComponent* DrivenComponent = Driven.Get())
		{
			if (DrivenComponent->IsSimulatingPhysics())
			{
				DrivenComponent->SetSimulatePhysics(false);
			}
		}
	}
	else
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_Press '%s' on '%s': no driven cap resolved — attach the press UNDER the mesh it should move (the mesh must be the press's parent, or the actor's root). It will not respond to pokes."),
			*GetName(), *GetNameSafe(GetOwner()));
	}
}

void UFXR_Press::NotifyPoke(const FVector& TipLocation, float TipRadius, IFXR_Interactor* Interactor)
{
	if (!bInteractionEnabled || !Driven.IsValid())
	{
		return;
	}

	// Work in the rest-face frame: +Z out of the button, the face plane at Z = 0.
	const FVector TipLocal = FaceRestWorld.InverseTransformPositionNoScale(TipLocation);

	// Only fingertips over the face press it (the tip sphere may lap over the rim).
	if (FVector2D(TipLocal.X, TipLocal.Y).SizeSquared() > FMath::Square(FaceRadius + TipRadius))
	{
		return;
	}

	// Reject approaches from behind the mechanism.
	if (TipLocal.Z < -(Travel + TipRadius))
	{
		return;
	}

	// Depth the tip sphere's leading surface has pushed past the rest face.
	const float PokeDepth = FMath::Clamp(TipRadius - static_cast<float>(TipLocal.Z), 0.f, Travel);
	if (PokeDepth <= 0.f)
	{
		return;
	}

	if (PokeDepth >= PendingPokeDepth)
	{
		PendingPokeDepth = PokeDepth;
		PressingInteractor = Interactor;
	}
	bPokedThisFrame = true;
	SetComponentTickEnabled(true);
}

void UFXR_Press::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Press_Tick);

	if (bPokedThisFrame)
	{
		// The cap follows the fingertip directly — a button under a finger must never lag it.
		Depth = PendingPokeDepth;
	}
	else
	{
		// No finger: spring back to rest, then sleep (unless the debug draw keeps the tick alive).
		PressingInteractor = nullptr;
		Depth = FMath::FInterpTo(Depth, 0.f, DeltaTime, ReturnSpeed);
		if (Depth < KINDA_SMALL_NUMBER)
		{
			Depth = 0.f;
			if (!IsDrawDebugEnabled())
			{
				SetComponentTickEnabled(false);
			}
		}
	}
	PendingPokeDepth = 0.f;
	bPokedThisFrame = false;

	ApplyDepth();

	// Click edges with hysteresis; haptic tick on the press edge (design 5.2 / motion spec seam).
	const float Value = GetPressValue();
	if (!bPressed && Value >= ActivationFraction)
	{
		bPressed = true;
		if (PressingInteractor && HapticAmplitude > 0.f)
		{
			PressingInteractor->SendHapticFeedback(HapticAmplitude, 0.05f);
		}
		OnPressed.Broadcast();
		BroadcastInteractionEvent(EFXR_InteractionPhase::Began, PressingInteractor);
	}
	else if (bPressed && Value < ReleaseFraction)
	{
		bPressed = false;
		OnReleased.Broadcast();
		BroadcastInteractionEvent(EFXR_InteractionPhase::Ended, nullptr);
	}
}

float UFXR_Press::GetPressValue() const
{
	return (Travel > KINDA_SMALL_NUMBER) ? FMath::Clamp(Depth / Travel, 0.f, 1.f) : 0.f;
}

void UFXR_Press::ApplyDepth()
{
	if (UPrimitiveComponent* DrivenComponent = Driven.Get())
	{
		const FVector PressAxis = -FaceRestWorld.GetUnitAxis(EAxis::Z);
		DrivenComponent->SetWorldLocation(DrivenRestWorld.GetLocation() + PressAxis * Depth, false, nullptr, ETeleportType::TeleportPhysics);
	}
}
