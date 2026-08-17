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

void UFXR_Press::DrawInteractionDebug() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cached at BeginPlay, so this is the authored rest face even while the cap is pushed in.
	const FVector Face = FaceRestWorld.GetLocation();
	const FVector Normal = FaceRestWorld.GetUnitAxis(EAxis::Z);
	const FVector AxisX = FaceRestWorld.GetUnitAxis(EAxis::X);
	const FVector AxisY = FaceRestWorld.GetUnitAxis(EAxis::Y);

	// Basic: the pressable disc (the Face Radius a fingertip must be inside) + approach direction.
	DrawDebugCircle(World, Face, FaceRadius, 24, FColor::White, false, -1.f, 0, 0.4f, AxisX, AxisY, false);
	DrawDebugDirectionalArrow(World, Face + Normal * 5.f, Face, 6.f, FColor::Cyan, false, -1.f, 0, 0.4f);

	if (!IsFullDebug())
	{
		return;
	}

	// Full: bottom of travel (grey), click threshold (yellow), and the live cap depth
	// (green once latched past the click point, orange on the way there).
	DrawDebugCircle(World, Face - Normal * Travel, FaceRadius, 24, FColor(90, 90, 90), false, -1.f, 0, 0.3f, AxisX, AxisY, false);
	DrawDebugCircle(World, Face - Normal * (Travel * ActivationFraction), FaceRadius * 0.9f, 24, FColor::Yellow, false, -1.f, 0, 0.3f, AxisX, AxisY, false);
	DrawDebugCircle(World, Face - Normal * Depth, FaceRadius * 0.75f, 24, bPressed ? FColor::Green : FColor::Orange, false, -1.f, 0, 0.6f, AxisX, AxisY, false);
}

#if WITH_EDITOR
void UFXR_Press::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UFXR_Press, bPreviewPressed) ||
		Changed == GET_MEMBER_NAME_CHECKED(UFXR_Press, Travel))
	{
		ApplyEditorPreview();
	}
}

void UFXR_Press::ApplyEditorPreview()
{
	UPrimitiveComponent* Cap = ResolveDrivenComponent();
	if (!Cap)
	{
		return;
	}

	// Restore first so repeated edits (e.g. scrubbing Travel while previewing) never stack offsets.
	if (bPreviewActive)
	{
		Cap->SetRelativeLocation(PreviewRestRelative);
		bPreviewActive = false;
	}

	if (bPreviewPressed)
	{
		// The press is a child of the cap, so moving the cap moves it too — but translation leaves
		// the rotation (and therefore the press normal) intact, so one offset is enough.
		PreviewRestRelative = Cap->GetRelativeLocation();
		Cap->AddWorldOffset(-GetComponentTransform().GetUnitAxis(EAxis::Z) * Travel);
		bPreviewActive = true;
	}
}
#endif

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
