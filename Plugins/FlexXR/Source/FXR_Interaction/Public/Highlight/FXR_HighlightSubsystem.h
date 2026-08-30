// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_FocusTypes.h"
#include "Types/FXR_HighlightTypes.h"
#include "FXR_HighlightSubsystem.generated.h"

class UFXR_InteractableBase;
class UPrimitiveComponent;

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

	/** Stencil value a style writes, so one post-process material can branch on style. */
	static int32 StencilFor(EFXR_HighlightStyle Style);

	// Guidance is authored state, so it is stored rather than derived. Hover and Selected are not
	// stored at all — the focus subsystem already owns them and a second copy could drift.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UFXR_InteractableBase>> Guided;

	// Only the objects currently drawing a highlight, so clearing never has to walk the registry.
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UFXR_InteractableBase>> Lit;
};
