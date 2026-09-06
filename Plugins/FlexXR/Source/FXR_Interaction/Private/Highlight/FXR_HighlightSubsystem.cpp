// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Highlight/FXR_HighlightSubsystem.h"
#include "Highlight/FXR_Highlight.h"
#include "Detection/FXR_FocusSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Settings/FXR_InteractionSettings.h"
#include "Settings/FXR_MotionSettings.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Two bits of state leave six for the fade — 64 levels reads as smooth across a 150 ms ramp. */
	constexpr int32 FXR_StencilStateStride = 4;
	constexpr int32 FXR_StencilMaxLevel = 63;
}

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

TStatId UFXR_HighlightSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFXR_HighlightSubsystem, STATGROUP_Tickables);
}

void UFXR_HighlightSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Highlight_Tick);

	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	const float FadeTime = UFXR_MotionSettings::GetFadeDuration();
	const float Step = (FadeTime > KINDA_SMALL_NUMBER) ? (DeltaTime / FadeTime) : 1.f;

	TArray<TWeakObjectPtr<UFXR_InteractableBase>> Finished;

	for (TPair<TWeakObjectPtr<UFXR_InteractableBase>, FFXR_HighlightRecord>& Pair : Active)
	{
		UFXR_InteractableBase* Interactable = Pair.Key.Get();
		if (!Interactable)
		{
			Finished.Add(Pair.Key);
			continue;
		}

		FFXR_HighlightRecord& Record = Pair.Value;
		if (!FMath::IsNearlyEqual(Record.Alpha, Record.Target))
		{
			Record.Alpha = FMath::FInterpConstantTo(Record.Alpha, Record.Target, 1.f, Step);
			Apply(Interactable, Record);
		}

		// Dropped only once it has actually faded out, so releasing a highlight never cuts it short.
		if (Record.Target <= 0.f && Record.Alpha <= KINDA_SMALL_NUMBER)
		{
			Release(Interactable);
			Finished.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<UFXR_InteractableBase>& Key : Finished)
	{
		Active.Remove(Key);
	}
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

float UFXR_HighlightSubsystem::GetHighlightAlpha(const UFXR_InteractableBase* Interactable) const
{
	const FFXR_HighlightRecord* Record = Active.Find(Interactable);
	return Record ? Record->Alpha : 0.f;
}

float UFXR_HighlightSubsystem::GetProximityAlpha(const UFXR_InteractableBase* Interactable) const
{
	float Best = 0.f;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		// Both hands can be approaching the same object; the nearer one decides.
		if (ProximityTarget[Index].Get() == Interactable)
		{
			Best = FMath::Max(Best, ProximityAlpha[Index]);
		}
	}
	return Best;
}

void UFXR_HighlightSubsystem::SetProximity(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable, float Alpha)
{
	const int32 Index = (Hand == EFXR_HandSide::Left) ? 0 : 1;

	UFXR_InteractableBase* Was = ProximityTarget[Index].Get();
	const float WasAlpha = ProximityAlpha[Index];
	if (Was == Interactable && FMath::IsNearlyEqual(WasAlpha, Alpha))
	{
		return; // nothing moved
	}

	ProximityTarget[Index] = Interactable;
	ProximityAlpha[Index] = Interactable ? FMath::Clamp(Alpha, 0.f, 1.f) : 0.f;

	// The object being left has to be re-resolved too, or it would hold the glow it had when the
	// hand turned away.
	if (Was && Was != Interactable)
	{
		Refresh(Was);
	}
	if (Interactable)
	{
		Refresh(Interactable);
	}
}

int32 UFXR_HighlightSubsystem::PackStencil(EFXR_HighlightState State, float Alpha)
{
	int32 StateBits = 0;
	switch (State)
	{
	case EFXR_HighlightState::Hover:    StateBits = 1; break;
	case EFXR_HighlightState::Guidance: StateBits = 2; break;
	case EFXR_HighlightState::Selected: StateBits = 3; break;
	default:                            return 0;
	}

	const int32 Level = FMath::RoundToInt(FMath::Clamp(Alpha, 0.f, 1.f) * FXR_StencilMaxLevel);
	return StateBits + Level * FXR_StencilStateStride;
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
	// makes a dropped-in FXR_Grab glow with zero setup.
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	EFXR_HighlightStyle Style = EFXR_HighlightStyle::None;
	if (State != EFXR_HighlightState::None)
	{
		Style = Config ? Config->ResolveStyle(State)
			: (Settings ? Settings->GetStyleFor(State) : EFXR_HighlightStyle::None);
	}

	bool bWanted = (State != EFXR_HighlightState::None) && (Style != EFXR_HighlightStyle::None);
	float Target = bWanted ? 1.f : 0.f;

	// An approaching hand lights the object part-way, in the Hover style. Full strength stays
	// reserved for actual reach: the step up is what says "you can take this now".
	EFXR_HighlightState DrawState = State;
	if (!bWanted)
	{
		const float Approach = GetProximityAlpha(Interactable);
		if (Approach > 0.f)
		{
			DrawState = EFXR_HighlightState::Hover;
			Style = Config ? Config->ResolveStyle(DrawState)
				: (Settings ? Settings->GetStyleFor(DrawState) : EFXR_HighlightStyle::None);
			if (Style != EFXR_HighlightStyle::None)
			{
				bWanted = true;
				Target = Approach;
			}
		}
	}

	if (!bWanted && !Active.Contains(Interactable))
	{
		return; // nothing to draw, and nothing part-way through fading out
	}

	FFXR_HighlightRecord& Record = Active.FindOrAdd(Interactable);
	Record.Target = Target;
	if (bWanted)
	{
		// State and style take effect at once; only strength eases. A hover that becomes guidance
		// recolours immediately rather than fading down through nothing and back up.
		Record.State = DrawState;
		Record.Style = Style;
		// The full-screen pass is installed lazily and only where it is the implementation. On the
		// hull tier no blendable is ever added, so a mobile project never pays for a post-process
		// chain it does not use — which is most of the point of having the tier.
		if (Style == EFXR_HighlightStyle::Outline)
		{
			const UFXR_InteractionSettings* TierSettings = UFXR_InteractionSettings::Get();
			if (!TierSettings || TierSettings->ResolveTier(GetWorld()) == EFXR_HighlightTier::PostProcess)
			{
				EnsureOutlineBlendable();
			}
		}
	}

	Apply(Interactable, Record);
}

void UFXR_HighlightSubsystem::Apply(UFXR_InteractableBase* Interactable, const FFXR_HighlightRecord& Record)
{
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();

	// Eased as well as timed, so the edge swells in rather than ramping linearly.
	const float Eased = FFXR_Motion::EaseFade(Record.Alpha);

	// One style, two implementations. Which one is a rendering decision, so it is resolved here and
	// never leaks into the state->style map that projects and training code actually read.
	const bool bHullTier = Settings && Settings->ResolveTier(GetWorld()) == EFXR_HighlightTier::MeshHull;
	const bool bOutline = (Record.Style == EFXR_HighlightStyle::Outline);
	const bool bHull = bOutline && bHullTier;
	const bool bOverlay = (Record.Style == EFXR_HighlightStyle::InnerBlink || Record.Style == EFXR_HighlightStyle::Sweep);

	// Nothing is written to the stencil on the hull tier: it is the post-process pass's private
	// channel, and a mobile project pays for custom depth the moment anything asks for it.
	const int32 Stencil = (bOutline && !bHullTier) ? PackStencil(Record.State, Eased) : 0;

	const UFXR_Highlight* Config = Interactable->GetOwner()
		? Interactable->GetOwner()->FindComponentByClass<UFXR_Highlight>()
		: nullptr;

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
				ApplyOverlay(Mesh, Record.Style, Record.State, Eased, Config);
			}
			else if (bHull)
			{
				ApplyHull(Mesh, Record.State, Eased, Config);
			}
			else
			{
				ClearOverlay(Mesh);
			}
		}
	}
}

void UFXR_HighlightSubsystem::Release(UFXR_InteractableBase* Interactable)
{
	TArray<UPrimitiveComponent*> Targets;
	GatherTargets(Interactable, Targets);

	for (UPrimitiveComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}
		Target->SetRenderCustomDepth(false);
		Target->SetCustomDepthStencilValue(0);
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(Target))
		{
			ClearOverlay(Mesh);
		}
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
	OutlineMID->SetScalarParameterValue(TEXT("OutlineIntensity"), Settings->OutlineIntensity);

	// Found through the view target rather than requiring a post-process volume or a FlexXR pawn, so
	// a dropped-in interactable outlines with no level setup and whatever pawn the project ships.
	Camera->PostProcessSettings.AddBlendable(OutlineMID, 1.f);
	bOutlineBlendableAdded = true;
}

UMaterialInstanceDynamic* UFXR_HighlightSubsystem::EnsureOverlayInstance(UMeshComponent* Mesh, UMaterialInterface* Source)
{
	if (!Mesh || !Source)
	{
		return nullptr; // a cleared material disables its style on purpose
	}

	FFXR_OverlayRecord& Record = Overlays.FindOrAdd(Mesh);

	// Rebuilt when the parent changes, not just when there is none: the hull and the blink share this
	// one slot, so a mesh whose state remaps from Outline to Guidance would otherwise keep pushing
	// hull parameters at a blink material that has never heard of them.
	if (Record.Instance && Record.Source == Source)
	{
		return Record.Instance;
	}

	if (!Record.Instance)
	{
		// Remember what was there first: clearing later restores it rather than blanking a mesh whose
		// overlay the project set for its own reasons.
		Record.Original = Mesh->GetOverlayMaterial();
	}

	Record.Instance = UMaterialInstanceDynamic::Create(Source, this);
	if (!Record.Instance)
	{
		Overlays.Remove(Mesh);
		return nullptr;
	}
	Record.Source = Source;
	Mesh->SetOverlayMaterial(Record.Instance);
	return Record.Instance;
}

void UFXR_HighlightSubsystem::ApplyOverlay(UMeshComponent* Mesh, EFXR_HighlightStyle Style, EFXR_HighlightState State, float Alpha, const UFXR_Highlight* Config)
{
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	if (!Settings)
	{
		return;
	}

	UMaterialInstanceDynamic* Instance = EnsureOverlayInstance(Mesh, Settings->OverlayMaterial.LoadSynchronous());
	if (!Instance)
	{
		return;
	}

	// Pushed every refresh, not just on creation, so a state change recolours without rebuilding.
	const FLinearColor Color = Config ? Config->ResolveColor(State) : Settings->GetColorFor(State);
	const float Intensity = Config ? Config->ResolveIntensity() : Settings->HighlightIntensity;
	const float PulseRate = Config ? Config->ResolvePulseRate() : Settings->HighlightPulseRate;
	const FVector Sweep = Config ? Config->GetSweepDirection() : FVector::UpVector;

	Instance->SetVectorParameterValue(TEXT("HighlightColor"), Color);
	Instance->SetScalarParameterValue(TEXT("HighlightIntensity"), Intensity);
	Instance->SetScalarParameterValue(TEXT("PulseRate"), PulseRate);
	Instance->SetScalarParameterValue(TEXT("SweepAmount"), Style == EFXR_HighlightStyle::Sweep ? 1.f : 0.f);
	Instance->SetScalarParameterValue(TEXT("FadeAlpha"), Alpha);
	Instance->SetVectorParameterValue(TEXT("SweepDirection"), FLinearColor(Sweep.X, Sweep.Y, Sweep.Z, 0.f));
}

void UFXR_HighlightSubsystem::ApplyHull(UMeshComponent* Mesh, EFXR_HighlightState State, float Alpha, const UFXR_Highlight* Config)
{
	const UFXR_InteractionSettings* Settings = UFXR_InteractionSettings::Get();
	if (!Settings)
	{
		return;
	}

	UMaterialInstanceDynamic* Instance = EnsureOverlayInstance(Mesh, Settings->OutlineHullMaterial.LoadSynchronous());
	if (!Instance)
	{
		return;
	}

	const FLinearColor Color = Config ? Config->ResolveColor(State) : Settings->GetColorFor(State);

	// The fade is the thickness, because a masked material has no opacity to fade. At zero the shell
	// sits exactly on the surface and the mesh's own front faces hide it, so it disappears cleanly
	// rather than flashing a black silhouette the way a faded emissive would.
	Instance->SetVectorParameterValue(TEXT("HighlightColor"), Color);
	Instance->SetScalarParameterValue(TEXT("HighlightIntensity"), Settings->OutlineIntensity);
	Instance->SetScalarParameterValue(TEXT("HullThickness"), Settings->OutlineHullThickness * Alpha);
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
