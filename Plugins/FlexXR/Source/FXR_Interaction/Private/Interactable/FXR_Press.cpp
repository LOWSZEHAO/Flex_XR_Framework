// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Press.h"
#include "Interactor/FXR_Interactor.h"
#include "Types/FXR_LogChannels.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Index into the per-hand arming state. */
	int32 FXR_HandIndex(const IFXR_Interactor* Interactor)
	{
		return (Interactor && Interactor->GetHandSide() == EFXR_HandSide::Left) ? 0 : 1;
	}
}

UFXR_Press::UFXR_Press()
{
	// A press ticks while poked (cap follow) and while returning to rest; idle it sleeps.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFXR_Press::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Editor viewports tick the preview actor's real components, which is the only context where a
	// press can move its cap on screen — a component template has neither owner nor attach parent.
	if (const UWorld* World = GetWorld())
	{
		if (!World->IsGameWorld())
		{
			bTickInEditor = true;
			SetComponentTickEnabled(true);
		}
	}
#endif
}

void UFXR_Press::OnUnregister()
{
#if WITH_EDITOR
	// A preview instance can be torn down mid-cycle (a Blueprint recompile rebuilds the preview
	// actor). Put the cap back first, so it is never left sitting at a pressed depth.
	EndEditorPreview();
#endif

	Super::OnUnregister();
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

	const int32 HandIndex = FXR_HandIndex(Interactor);

	// Work in the rest-face frame: +Z out of the button, the face plane at Z = 0.
	const FVector TipLocal = FaceRestWorld.InverseTransformPositionNoScale(TipLocation);

	// Only fingertips over the face press it (the tip sphere may lap over the rim).
	if (FVector2D(TipLocal.X, TipLocal.Y).SizeSquared() > FMath::Square(FaceRadius + TipRadius))
	{
		// Sliding off the side ends this hand's approach outright — returning must start from the
		// front again, or a finger swinging back in below the face would re-press the button.
		bPokeArmed[HandIndex] = false;
		return;
	}

	LastOverFaceFrame = GFrameCounter;
	SetComponentTickEnabled(true);

	// Depth the tip sphere's leading surface has pushed past the rest face.
	const float PokeDepth = TipRadius - static_cast<float>(TipLocal.Z);

	// In front of the face: nothing to press yet, but this is where a legitimate approach starts.
	if (PokeDepth <= 0.f)
	{
		bPokeArmed[HandIndex] = true;
		return;
	}

	// Arrived from the side or behind without ever being in front — ignore, so the cap never jumps
	// to meet a finger that did not push it there.
	if (!bPokeArmed[HandIndex])
	{
		return;
	}

	// Pushing past the bottom of travel holds the button fully pressed; it must never spring back
	// out from under a finger that is still driving it deeper.
	const float ClampedDepth = FMath::Min(PokeDepth, Travel);
	if (ClampedDepth >= PendingPokeDepth)
	{
		PendingPokeDepth = ClampedDepth;
		PressingInteractor = Interactor;
	}
	LastPokeFrame = GFrameCounter;
}

void UFXR_Press::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Press_Tick);

#if WITH_EDITOR
	if (const UWorld* World = GetWorld())
	{
		if (!World->IsGameWorld())
		{
			TickEditorPreview(DeltaTime);
			return;
		}
	}
#endif

	// A one-frame tolerance: the driver feeding pokes may tick after this component.
	const bool bPoked = (GFrameCounter - LastPokeFrame) <= 1;
	const bool bOverFace = (GFrameCounter - LastOverFaceFrame) <= 1;

	if (bPoked)
	{
		// The cap follows the fingertip directly — a button under a finger must never lag it.
		Depth = PendingPokeDepth;
	}
	else
	{
		// No finger on the face: spring back to rest, then sleep (unless debug keeps ticking).
		PressingInteractor = nullptr;
		Depth = FMath::FInterpTo(Depth, 0.f, DeltaTime, ReturnSpeed);
		if (Depth < KINDA_SMALL_NUMBER)
		{
			Depth = 0.f;
			if (!IsDrawDebugEnabled() && !bOverFace)
			{
				SetComponentTickEnabled(false);
			}
		}
	}
	PendingPokeDepth = 0.f;

	// No fingertip anywhere near the face — clear both hands so the next press approaches afresh.
	if (!bOverFace)
	{
		bPokeArmed[0] = false;
		bPokeArmed[1] = false;
	}

	ApplyDepth();

	// Analog depth for partial-press visuals / audio, then the click edges.
	const float Value = GetPressValue();
	if (!FMath::IsNearlyEqual(Value, LastBroadcastValue, 1e-3f))
	{
		LastBroadcastValue = Value;
		OnPressValueChanged.Broadcast(Value);
	}

	// Click edges with hysteresis; haptic tick on the press edge (design 5.2 / motion spec seam).
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
void UFXR_Press::EndEditorPreview()
{
	if (!bPreviewCaptured)
	{
		return;
	}

	// Always leave the cap where it was found, so an authoring aid can never be saved in.
	Depth = 0.f;
	ApplyDepth();
	bPreviewCaptured = false;
}

void UFXR_Press::TickEditorPreview(float DeltaTime)
{
	if (!bPreviewPressed)
	{
		EndEditorPreview();
		return;
	}

	if (!bPreviewCaptured)
	{
		Driven = ResolveDrivenComponent();
		if (!Driven.IsValid())
		{
			return;
		}
		// Rest is captured while the cap is known to be at rest: the previous run always restores
		// it, so a stopped-and-restarted preview cannot creep the mesh downward.
		FaceRestWorld = GetComponentTransform();
		DrivenRestWorld = Driven->GetComponentTransform();
		PreviewTime = 0.f;
		bPreviewCaptured = true;
	}

	// Ease down and back up so the motion reads as a press rather than a slide.
	PreviewTime = FMath::Fmod(PreviewTime + DeltaTime, PreviewCycleSeconds);
	const float Cycle = PreviewTime / PreviewCycleSeconds;
	const float PingPong = 1.f - FMath::Abs(Cycle * 2.f - 1.f);
	Depth = FMath::InterpEaseInOut(0.f, Travel, PingPong, 2.f);
	ApplyDepth();
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
