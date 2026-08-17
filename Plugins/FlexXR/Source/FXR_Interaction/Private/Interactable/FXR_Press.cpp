// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Interactable/FXR_Press.h"
#include "Interactor/FXR_Interactor.h"
#include "Types/FXR_LogChannels.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#if WITH_EDITOR
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#endif

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

	// Only fingertips over the face press it (the tip sphere may lap over the rim). Once engaged the
	// rim is a little more forgiving, so a finger resting near the edge cannot flicker on and off.
	const float RimRadius = FaceRadius + TipRadius + ((Depth > 0.f) ? EdgeTolerance : 0.f);
	if (FVector2D(TipLocal.X, TipLocal.Y).SizeSquared() > FMath::Square(RimRadius))
	{
		// Sliding off the side ends this approach outright — returning must start from the front
		// again, or a finger swinging back in below the face would re-press the button.
		bPokeArmed = false;
		return;
	}

	LastOverFaceFrame = GFrameCounter;
	SetComponentTickEnabled(true);

	// Depth the tip sphere's leading surface has pushed past the rest face.
	const float PokeDepth = TipRadius - static_cast<float>(TipLocal.Z);

	// In front of the face: nothing to press yet, but this is where a legitimate approach starts.
	if (PokeDepth <= 0.f)
	{
		bPokeArmed = true;
		return;
	}

	// Arrived from the side or behind without ever being in front — ignore, so the cap never jumps
	// to meet a finger that did not push it there.
	if (!bPokeArmed)
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

	// Leaving the face disarms, so the next press must again approach from the front.
	if (!bOverFace)
	{
		bPokeArmed = false;
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

UPrimitiveComponent* UFXR_Press::ResolvePreviewCap() const
{
	// Placed instance: the normal driven-component rule applies.
	if (UPrimitiveComponent* Cap = ResolveDrivenComponent())
	{
		return Cap;
	}

	// Blueprint editor: component templates carry no attach parent — the hierarchy lives in the
	// construction script, so walk the SCS to find the node whose child is this template.
	const UBlueprintGeneratedClass* BPClass = GetTypedOuter<UBlueprintGeneratedClass>();
	USimpleConstructionScript* SCS = BPClass ? BPClass->SimpleConstructionScript : nullptr;
	if (!SCS)
	{
		return nullptr;
	}

	const TArray<USCS_Node*>& Nodes = SCS->GetAllNodes();
	USCS_Node* SelfNode = nullptr;
	for (USCS_Node* Node : Nodes)
	{
		if (Node && Node->ComponentTemplate == this)
		{
			SelfNode = Node;
			break;
		}
	}
	if (!SelfNode)
	{
		return nullptr;
	}

	for (USCS_Node* Node : Nodes)
	{
		if (Node && Node->GetChildNodes().Contains(SelfNode))
		{
			return Cast<UPrimitiveComponent>(Node->ComponentTemplate);
		}
	}
	return nullptr;
}

void UFXR_Press::ApplyEditorPreview()
{
	UPrimitiveComponent* Cap = ResolvePreviewCap();
	if (!Cap)
	{
		UE_LOG(LogFXR, Warning,
			TEXT("FXR_Press '%s': Preview Pressed found no cap mesh — the press must be attached under the mesh it moves."),
			*GetName());
		bPreviewPressed = false;
		return;
	}

	Cap->Modify();
	Modify();

	// Restore first so repeated edits (e.g. scrubbing Travel while previewing) never stack offsets.
	if (bPreviewActive)
	{
		Cap->SetRelativeLocation(PreviewRestRelative);
		bPreviewActive = false;
	}

	if (!bPreviewPressed)
	{
		return;
	}

	PreviewRestRelative = Cap->GetRelativeLocation();

	// The cap moves in its own parent's space. A placed instance has live world transforms; a
	// Blueprint template only has relative ones, where the press sits directly under the cap.
	FVector Delta;
	if (GetOwner())
	{
		const FVector WorldDelta = -GetComponentTransform().GetUnitAxis(EAxis::Z) * Travel;
		const USceneComponent* CapParent = Cap->GetAttachParent();
		Delta = CapParent ? CapParent->GetComponentTransform().InverseTransformVectorNoScale(WorldDelta) : WorldDelta;
	}
	else
	{
		const FVector PressAxisInCapSpace = GetRelativeRotation().RotateVector(FVector::UpVector);
		Delta = Cap->GetRelativeRotation().RotateVector(-PressAxisInCapSpace * Travel);
	}

	Cap->SetRelativeLocation(PreviewRestRelative + Delta);
	bPreviewActive = true;
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
