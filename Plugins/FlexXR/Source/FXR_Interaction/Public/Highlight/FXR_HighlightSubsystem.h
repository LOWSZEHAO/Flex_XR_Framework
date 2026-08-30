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
 */
UCLASS()
class FXR_INTERACTION_API UFXR_HighlightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * Mark or clear an interactable as "do this now". Outranks Hover, so an object the player is
	 * already reaching for keeps demanding attention until the step is satisfied.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Highlight")
	void SetGuidance(UFXR_InteractableBase* Interactable, bool bGuided);

	/** The state this interactable is drawn in right now, strongest first. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Highlight")
	EFXR_HighlightState GetHighlightState(const UFXR_InteractableBase* Interactable) const;

	static UFXR_HighlightSubsystem* Get(const UObject* WorldContextObject);

private:
	UFUNCTION()
	void HandleFocusChanged(UFXR_InteractableBase* Interactable, EFXR_FocusState State, EFXR_HandSide Hand);

	/** Re-resolve one interactable's state and push the result to its primitives. */
	void Refresh(UFXR_InteractableBase* Interactable);

	/** The primitives that should light up, honouring the component's Scope when one is present. */
	void GatherTargets(const UFXR_InteractableBase* Interactable, TArray<UPrimitiveComponent*>& OutTargets) const;

	/**
	 * Stencil value written for a state, which is how the one full-screen outline pass knows what
	 * colour to draw. State rather than style, because a shared pass cannot read a per-object colour
	 * but can read the stencil: state is the only axis it can vary along.
	 */
	static int32 StencilFor(EFXR_HighlightState State);

	/**
	 * Put the outline pass on the player camera the first time something outlines. Lazy because the
	 * player controller need not exist at world BeginPlay, and free when a project never outlines.
	 */
	void EnsureOutlineBlendable();

	/** Drive one mesh's overlay slot for the Inner Blink and Sweep styles. */
	void ApplyOverlay(UMeshComponent* Mesh, EFXR_HighlightStyle Style, EFXR_HighlightState State, const UFXR_Highlight* Config);

	/** Hand the overlay slot back, restoring whatever the mesh carried before. */
	void ClearOverlay(UMeshComponent* Mesh);

	// Held so the outline colours and thickness can be pushed without rebuilding the blendable.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMID;

	bool bOutlineBlendableAdded = false;

	// Only meshes FlexXR currently drives. A project may have its own overlay material on a mesh for
	// unrelated reasons, so the slot is borrowed and given back rather than blanked.
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UMeshComponent>, FFXR_OverlayRecord> Overlays;

	// Guidance is authored state, so it is stored rather than derived. Hover and Selected are not
	// stored at all — the focus subsystem already owns them and a second copy could drift.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UFXR_InteractableBase>> Guided;

	// Only the objects currently drawing a highlight, so clearing never has to walk the registry.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UFXR_InteractableBase>> Lit;
};
