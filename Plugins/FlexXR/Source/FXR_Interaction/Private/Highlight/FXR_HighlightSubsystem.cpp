// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Highlight/FXR_HighlightSubsystem.h"
#include "Highlight/FXR_Highlight.h"
#include "Detection/FXR_FocusSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UFXR_HighlightSubsystem* UFXR_HighlightSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return World->GetSubsystem<UFXR_HighlightSubsystem>();
	}
	return nullptr;
}

void UFXR_HighlightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Bound here rather than in Initialize: subsystem construction order is not guaranteed, and by
	// BeginPlay every world subsystem exists.
	if (UFXR_FocusSubsystem* Focus = InWorld.GetSubsystem<UFXR_FocusSubsystem>())
	{
		Focus->OnFocusChanged.AddDynamic(this, &UFXR_HighlightSubsystem::HandleFocusChanged);
	}
}

void UFXR_HighlightSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (UFXR_FocusSubsystem* Focus = World->GetSubsystem<UFXR_FocusSubsystem>())
		{
			Focus->OnFocusChanged.RemoveDynamic(this, &UFXR_HighlightSubsystem::HandleFocusChanged);
		}
	}

	Super::Deinitialize();
}

void UFXR_HighlightSubsystem::HandleFocusChanged(UFXR_InteractableBase* Interactable, EFXR_FocusState State, EFXR_HandSide Hand)
{
	Refresh(Interactable);
}

void UFXR_HighlightSubsystem::SetGuidance(UFXR_InteractableBase* Interactable, bool bGuided)
{
	if (!Interactable)
	{
		return;
	}

	if (bGuided)
	{
		Guided.Add(Interactable);
	}
	else
	{
		Guided.Remove(Interactable);
	}
	Refresh(Interactable);
}

EFXR_HighlightState UFXR_HighlightSubsystem::GetHighlightState(const UFXR_InteractableBase* Interactable) const
{
	if (!Interactable)
	{
		return EFXR_HighlightState::None;
	}

	const UFXR_FocusSubsystem* Focus = UFXR_FocusSubsystem::Get(this);
	const EFXR_FocusState FocusState = Focus ? Focus->GetState(Interactable) : EFXR_FocusState::None;

	// Acting on it outranks being told to; being told to outranks merely being able to. Guidance
	// therefore survives the player reaching for the object, and stops once they have it.
	if (FocusState == EFXR_FocusState::Selected)
	{
		return EFXR_HighlightState::Selected;
	}
	if (Guided.Contains(Interactable))
	{
		return EFXR_HighlightState::Guidance;
	}
	if (FocusState == EFXR_FocusState::Hovered)
	{
		return EFXR_HighlightState::Hover;
	}
	return EFXR_HighlightState::None;
}

void UFXR_HighlightSubsystem::GatherTargets(const UFXR_InteractableBase* Interactable, TArray<UPrimitiveComponent*>& OutTargets) const
{
	OutTargets.Reset();
	if (!Interactable)
	{
		return;
	}

	const UFXR_Highlight* Config = Interactable->GetOwner()
		? Interactable->GetOwner()->FindComponentByClass<UFXR_Highlight>()
		: nullptr;

	const EFXR_HighlightScope Scope = Config ? Config->GetScope() : EFXR_HighlightScope::Everything;

	if (Scope == EFXR_HighlightScope::TargetMesh)
	{
		if (UPrimitiveComponent* Driven = Interactable->GetDrivenComponent())
		{
			OutTargets.Add(Driven);
		}
		return;
	}

	// Everything: the owning actor's primitives, so an extinguisher glows with its pin, handle and
	// hose as one object. Scoped to this actor — other actors and the hand meshes are not ours.
	if (const AActor* Owner = Interactable->GetOwner())
	{
		Owner->GetComponents<UPrimitiveComponent>(OutTargets);
	}
}

int32 UFXR_HighlightSubsystem::StencilFor(EFXR_HighlightState State)
{
	// The outline pass is one full-screen draw shared by every outlined object, so it cannot read a
	// per-object colour. Encoding state here is what lets it colour hover differently from guidance.
	switch (State)
	{
	case EFXR_HighlightState::Hover:    return 1;
	case EFXR_HighlightState::Guidance: return 2;
	case EFXR_HighlightState::Selected: return 3;
	default:                            return 0;
	}
}

void UFXR_HighlightSubsystem::EnsureOutlineBlendable()
{
	if (bOutlineBlendableAdded)
	{
		return;
	}

	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	if (!Settings)
	{
		return;
	}

	UMaterialInterface* Source = Settings->OutlineMaterial.LoadSynchronous();
	if (!Source)
	{
		return; // cleared on purpose disables outlines; the stencil still gets written for other passes
	}

	// The view target's camera component, not the camera manager: the manager only carries a blend
	// cache that is rebuilt every frame, while the component holds a blendable that persists.
	APlayerCameraManager* Manager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	AActor* ViewTarget = Manager ? Manager->GetViewTarget() : nullptr;
	UCameraComponent* Camera = ViewTarget ? ViewTarget->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera)
	{
		return; // no local player or no camera yet — retried on the next highlight
	}

	OutlineMID = UMaterialInstanceDynamic::Create(Source, this);
	if (!OutlineMID)
	{
		return;
	}

	OutlineMID->SetVectorParameterValue(TEXT("HoverColor"), Settings->GetColorFor(EFXR_HighlightState::Hover));
	OutlineMID->SetVectorParameterValue(TEXT("GuidanceColor"), Settings->GetColorFor(EFXR_HighlightState::Guidance));
	OutlineMID->SetVectorParameterValue(TEXT("SelectedColor"), Settings->GetColorFor(EFXR_HighlightState::Selected));
	OutlineMID->SetScalarParameterValue(TEXT("OutlineThickness"), Settings->OutlineThickness);
	OutlineMID->SetScalarParameterValue(TEXT("OutlineIntensity"), Settings->HighlightIntensity);

	// Found through the view target rather than requiring a post-process volume or a FlexXR pawn, so
	// a dropped-in interactable outlines with no level setup and whatever pawn the project ships.
	Camera->PostProcessSettings.AddBlendable(OutlineMID, 1.f);
	bOutlineBlendableAdded = true;
}

void UFXR_HighlightSubsystem::Refresh(UFXR_InteractableBase* Interactable)
{
	if (!Interactable)
	{
		return;
	}

	const EFXR_HighlightState State = GetHighlightState(Interactable);

	const UFXR_Highlight* Config = Interactable->GetOwner()
		? Interactable->GetOwner()->FindComponentByClass<UFXR_Highlight>()
		: nullptr;

	// No component is the common case: the style comes straight from project settings, which is what
	// makes "drop FXR_Grab and it glows" true with zero setup.
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	EFXR_HighlightStyle Style = EFXR_HighlightStyle::None;
	if (State != EFXR_HighlightState::None)
	{
		Style = Config ? Config->ResolveStyle(State)
			: (Settings ? Settings->GetStyleFor(State) : EFXR_HighlightStyle::None);
	}

	// Style picks the mechanism, not the colour: Outline is one full-screen pass keyed off the
	// stencil, while Inner Blink and Sweep draw per mesh through the overlay material slot.
	const bool bOutline = (Style == EFXR_HighlightStyle::Outline);
	const bool bOverlay = (Style == EFXR_HighlightStyle::InnerBlink || Style == EFXR_HighlightStyle::Sweep);
	const int32 Stencil = bOutline ? StencilFor(State) : 0;

	if (bOutline)
	{
		EnsureOutlineBlendable();
	}

	TArray<UPrimitiveComponent*> Targets;
	GatherTargets(Interactable, Targets);

	for (UPrimitiveComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		Target->SetRenderCustomDepth(Stencil != 0);
		Target->SetCustomDepthStencilValue(Stencil);

		// Only meshes have an overlay slot; other primitives still take the stencil above.
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(Target))
		{
			if (bOverlay)
			{
				ApplyOverlay(Mesh, Style, State, Config);
			}
			else
			{
				ClearOverlay(Mesh);
			}
		}
	}

	// Track only what is lit, so the "clear everything" path never walks the whole registry.
	if (Stencil != 0)
	{
		Lit.Add(Interactable);
	}
	else
	{
		Lit.Remove(Interactable);
	}
}

void UFXR_HighlightSubsystem::ApplyOverlay(UMeshComponent* Mesh, EFXR_HighlightStyle Style, EFXR_HighlightState State, const UFXR_Highlight* Config)
{
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	if (!Mesh || !Settings)
	{
		return;
	}

	FFXR_OverlayRecord& Record = Overlays.FindOrAdd(Mesh);
	if (!Record.Instance)
	{
		UMaterialInterface* Source = Settings->OverlayMaterial.LoadSynchronous();
		if (!Source)
		{
			return; // cleared on purpose disables Inner Blink and Sweep
		}

		// Remember what was there first: clearing later restores it rather than blanking a mesh whose
		// overlay the project set for its own reasons.
		Record.Original = Mesh->GetOverlayMaterial();
		Record.Instance = UMaterialInstanceDynamic::Create(Source, this);
		if (!Record.Instance)
		{
			Overlays.Remove(Mesh);
			return;
		}
		Mesh->SetOverlayMaterial(Record.Instance);
	}

	// Pushed every refresh, not just on creation, so a state change recolours without rebuilding.
	const FLinearColor Color = Config ? Config->ResolveColor(State) : Settings->GetColorFor(State);
	const float Intensity = Config ? Config->ResolveIntensity() : Settings->HighlightIntensity;
	const float PulseRate = Config ? Config->ResolvePulseRate() : Settings->HighlightPulseRate;
	const FVector Sweep = Config ? Config->GetSweepDirection() : FVector::UpVector;

	Record.Instance->SetVectorParameterValue(TEXT("HighlightColor"), Color);
	Record.Instance->SetScalarParameterValue(TEXT("HighlightIntensity"), Intensity);
	Record.Instance->SetScalarParameterValue(TEXT("PulseRate"), PulseRate);
	Record.Instance->SetScalarParameterValue(TEXT("SweepAmount"), Style == EFXR_HighlightStyle::Sweep ? 1.f : 0.f);
	Record.Instance->SetVectorParameterValue(TEXT("SweepDirection"), FLinearColor(Sweep.X, Sweep.Y, Sweep.Z, 0.f));
}

void UFXR_HighlightSubsystem::ClearOverlay(UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}

	FFXR_OverlayRecord Record;
	if (!Overlays.RemoveAndCopyValue(Mesh, Record))
	{
		return; // never ours — leave it alone
	}
	Mesh->SetOverlayMaterial(Record.Original);
}
