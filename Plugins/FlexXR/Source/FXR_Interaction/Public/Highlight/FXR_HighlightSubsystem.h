// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_FocusTypes.h"
#include "Types/FXR_HighlightTypes.h"
#include "FXR_HighlightSubsystem.generated.h"

class UFXR_Highlight;
class UFXR_InteractableBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;

/** What one mesh had before FlexXR took its overlay slot, so releasing it puts things back. */
USTRUCT()
struct FFXR_OverlayRecord
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Original = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Instance = nullptr;

	/** Parent the instance was made from — a mesh that switches between hull and overlay needs a new one. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Source = nullptr;
};

/** One interactable's live highlight: what it should look like, and how far it has faded there. */
USTRUCT()
struct FFXR_HighlightRecord
{
	GENERATED_BODY()

	EFXR_HighlightState State = EFXR_HighlightState::None;
	EFXR_HighlightStyle Style = EFXR_HighlightStyle::None;

	/** Where the fade is now, and where it is heading (1 while highlighted, 0 once not). */
	float Alpha = 0.f;
	float Target = 0.f;
};

/**
 * UFXR_HighlightSubsystem — turns semantic states into drawn highlights (design 5.6).
 *
 * It lives here, not on UFXR_Highlight, because the *default* highlight must work on an interactable
 * that has no highlight component at all. The optional component only supplies overrides when one
 * happens to be present.
 *
 * Hover and Selected arrive from the focus subsystem. Guidance is set deliberately by a training
 * step or game script and is sticky until cleared — it is an instruction, not a consequence of where
 * the player's hands are.
 *
 * Tickable because highlights fade rather than pop: an outline that appears at full strength on one
 * frame and vanishes on the next reads as a flicker, especially in a headset where the hand is never
 * perfectly still on the edge of a hover.
 */
UCLASS()
class FXR_INTERACTION_API UFXR_HighlightSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Mark or clear an interactable as "do this now". Outranks Hover, so an object the player is
	 * already reaching for keeps demanding attention until the step is satisfied.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Highlight")
	void SetGuidance(UFXR_InteractableBase* Interactable, bool bGuided);

	/** The state this interactable is drawn in right now, strongest first. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Highlight")
	EFXR_HighlightState GetHighlightState(const UFXR_InteractableBase* Interactable) const;

	/** How far this interactable has faded in, 0..1 — for tests and for anything syncing to the fade. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Highlight")
	float GetHighlightAlpha(const UFXR_InteractableBase* Interactable) const;

	/**
	 * A hand is approaching this interactable, Alpha 0..1 by distance. Null clears that hand.
	 *
	 * Kept separate from Hover: hover means "you can take this", and that promise would be diluted if
	 * an object 35 cm away counted as hovered — the far ray suppression reads it, for one.
	 */
	void SetProximity(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable, float Alpha);

	static UFXR_HighlightSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Stencil value for a state and fade level, packed as State + Level * 4.
	 *
	 * The outline is one full-screen pass, so it can read nothing per object except this byte — and
	 * it needs both which state to colour and how far faded it is. Two bits of state (1 Hover,
	 * 2 Guidance, 3 Selected) leave six for the fade, and 64 levels is finer than the eye resolves
	 * across a 150 ms fade.
	 */
	static int32 PackStencil(EFXR_HighlightState State, float Alpha);

private:
	UFUNCTION()
	void HandleFocusChanged(UFXR_InteractableBase* Interactable, EFXR_FocusState State, EFXR_HandSide Hand);

	/** Strongest approach glow any hand is casting on this interactable. */
	float GetProximityAlpha(const UFXR_InteractableBase* Interactable) const;

	/** Re-resolve one interactable.s target state and style; the fade itself happens on tick. */
	void Refresh(UFXR_InteractableBase* Interactable);

	/** Push one interactable's current fade to its primitives. */
	void Apply(UFXR_InteractableBase* Interactable, const FFXR_HighlightRecord& Record);

	/** Stop drawing entirely and hand back anything borrowed. */
	void Release(UFXR_InteractableBase* Interactable);

	/** The primitives that should light up, honouring the component's Scope when one is present. */
	void GatherTargets(const UFXR_InteractableBase* Interactable, TArray<UPrimitiveComponent*>& OutTargets) const;

	/**
	 * Put the outline pass on the player camera the first time something outlines. Lazy because the
	 * player controller need not exist at world BeginPlay, and free when a project never outlines.
	 */
	void EnsureOutlineBlendable();

	/** Borrow a mesh's overlay slot for Source, remembering what was there. Null if the slot is unusable. */
	UMaterialInstanceDynamic* EnsureOverlayInstance(UMeshComponent* Mesh, UMaterialInterface* Source);

	/** Drive one mesh's overlay slot for the Inner Blink and Sweep styles. */
	void ApplyOverlay(UMeshComponent* Mesh, EFXR_HighlightStyle Style, EFXR_HighlightState State, float Alpha, const UFXR_Highlight* Config);

	/**
	 * Draw the Outline style as an inverted hull on this mesh — the Mesh Hull tier. Shares the overlay
	 * slot with Inner Blink and Sweep, which is safe because an interactable draws one style at a time.
	 */
	void ApplyHull(UMeshComponent* Mesh, EFXR_HighlightState State, float Alpha, const UFXR_Highlight* Config);

	/** Hand the overlay slot back, restoring whatever the mesh carried before. */
	void ClearOverlay(UMeshComponent* Mesh);

	// Guidance is authored state, so it is stored rather than derived. Hover and Selected are not
	// stored at all — the focus subsystem already owns them and a second copy could drift.
	TSet<TWeakObjectPtr<UFXR_InteractableBase>> Guided;

	// What each hand is currently approaching, and how strongly. Per hand rather than a set, because
	// only the nearest candidate glows — a cloud of dimly lit objects is the noise this design avoids.
	TWeakObjectPtr<UFXR_InteractableBase> ProximityTarget[2];
	float ProximityAlpha[2] = { 0.f, 0.f };

	// Everything currently drawing or fading out. An entry leaves only once it has faded to nothing,
	// which is what keeps a released highlight from cutting off mid-fade.
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UFXR_InteractableBase>, FFXR_HighlightRecord> Active;

	// Held so the outline colours and thickness can be pushed without rebuilding the blendable.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMID;

	bool bOutlineBlendableAdded = false;

	// Only meshes FlexXR currently drives. A project may have its own overlay material on a mesh for
	// unrelated reasons, so the slot is borrowed and given back rather than blanked.
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UMeshComponent>, FFXR_OverlayRecord> Overlays;
};
