// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_FocusTypes.h"
#include "FXR_FocusSubsystem.generated.h"

class UFXR_InteractableBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFXR_FocusChanged, UFXR_InteractableBase*, Interactable, EFXR_FocusState, State, EFXR_HandSide, Hand);

/**
 * UFXR_FocusSubsystem — the single source of truth for what each hand is paying attention to
 * (design 5.6): hovered, focused, selected.
 *
 * It stores and broadcasts; it does not detect. The interaction driver already owns the interactors
 * and already ticks per hand, so it computes hover from the same scoring pass that decides grabs and
 * publishes the result here — which is what guarantees the object that lights up is the object you
 * would actually take. A second system re-deriving hover could disagree with the first, and would
 * pay a second world query per hand per frame to do it.
 *
 * Consumers (highlight, ray targets, spatial UI, locomotion's yielding rule) read this rather than
 * asking the world themselves.
 */
UCLASS()
class FXR_INTERACTION_API UFXR_FocusSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** What this hand is in reach of, or null. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Focus")
	UFXR_InteractableBase* GetHovered(EFXR_HandSide Hand) const;

	/** What this hand has acted on — held or ray-selected — or null. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Focus")
	UFXR_InteractableBase* GetSelected(EFXR_HandSide Hand) const;

	/** The strongest state any hand currently has on this interactable. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Focus")
	EFXR_FocusState GetState(const UFXR_InteractableBase* Interactable) const;

	/** True if any hand is in reach of this interactable. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Focus")
	bool IsHoveredByAnyHand(const UFXR_InteractableBase* Interactable) const;

	//~ Publishing side — called by the interaction driver, not by gameplay code.
	/** Set (or clear, with null) what this hand is in reach of. No-op if unchanged. */
	void SetHovered(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable);

	/** Set (or clear, with null) what this hand has acted on. No-op if unchanged. */
	void SetSelected(EFXR_HandSide Hand, UFXR_InteractableBase* Interactable);

	/**
	 * Fires whenever an interactable's strongest state changes, including to None. Highlight
	 * listens here; anything that lights something up needs the closing event as reliably as the
	 * opening one, or it strands a glowing object.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Focus")
	FFXR_FocusChanged OnFocusChanged;

	/** Resolve the subsystem from any world context object (may return null). */
	static UFXR_FocusSubsystem* Get(const UObject* WorldContextObject);

private:
	/** Recompute an interactable's strongest state across both hands and broadcast if it moved. */
	void NotifyIfStateChanged(UFXR_InteractableBase* Interactable, EFXR_HandSide Hand);

	static int32 Index(EFXR_HandSide Hand) { return static_cast<int32>(Hand); }

	// Indexed by EFXR_HandSide. Weak so an interactable destroyed mid-hover cannot keep it alive
	// or leave a dangling read.
	TWeakObjectPtr<UFXR_InteractableBase> Hovered[2];
	TWeakObjectPtr<UFXR_InteractableBase> Selected[2];
};
