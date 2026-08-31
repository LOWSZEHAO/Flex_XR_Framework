// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_Socket.generated.h"

class UFXR_Grab;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_SocketEvent, UFXR_Grab*, Object);

/**
 * UFXR_Socket — a snap zone: "this object belongs here".
 *
 * Pairs with FXR_Grab. Carry an accepted object near the socket and a ghost shows where it would
 * land; let go and it seats. Re-grab pulls it back out. Seating emits this component's
 * InteractionId, which is what makes "docked the extinguisher on its wall mount" a validatable
 * SOP step rather than something a training author has to detect by hand.
 *
 * The socket's own transform is the seat pose — place the component where the object's origin
 * should end up, and point it the way the object should face.
 *
 * Not itself grabbable: a socket receives, it is never picked up.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Socket : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	UFXR_Socket();

	/** Sockets are found by the driver's socket pass, never by a hand's grab sphere. */
	virtual bool IsGrabTarget() const override { return false; }

	/** A socket receives; it is never picked up. */
	virtual bool CanEverBeHeld() const override { return false; }

	virtual void OnRegister() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The object seated here, or null. */
	UFUNCTION(BlueprintPure, Category = "Socket")
	UFXR_Grab* GetSocketedObject() const { return Socketed.Get(); }

	UFUNCTION(BlueprintPure, Category = "Socket")
	bool IsOccupied() const { return Socketed.IsValid(); }

	/** Let go of whatever is seated — the way out of a Lock In socket. */
	UFUNCTION(BlueprintCallable, Category = "Socket")
	void Eject();

	/** An accepted object came within range while held. */
	UPROPERTY(BlueprintAssignable, Category = "Socket")
	FFXR_SocketEvent OnHoverStart;

	/** That object left range, or stopped qualifying, without seating. */
	UPROPERTY(BlueprintAssignable, Category = "Socket")
	FFXR_SocketEvent OnHoverEnd;

	/** An object seated here. */
	UPROPERTY(BlueprintAssignable, Category = "Socket")
	FFXR_SocketEvent OnSocketed;

	/** The seated object left — grabbed back out, or ejected. */
	UPROPERTY(BlueprintAssignable, Category = "Socket")
	FFXR_SocketEvent OnRemoved;

	//~ Driven by UFXR_InteractionDriver's socket pass, which picks the nearest accepting socket.
	/** Whether this socket would take that object right now; OutDistanceSq ranks it against others. */
	bool CanAccept(const UFXR_Grab* Object, float& OutDistanceSq) const;
	bool IsAutoSnap() const { return bAutoSnap; }
	void BeginPreview(UFXR_Grab* Object);
	void EndPreview();
	/** Seat an object, ending any hold on it first. */
	void Seat(UFXR_Grab* Object);
	/** Notice a seated object that has been grabbed back out. */
	void RefreshOccupancy();
	UFXR_Grab* GetPreviewObject() const { return Preview.Get(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginPlay() override;
	virtual void DrawInteractionDebug() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Actor tags this socket accepts. Empty accepts any grabbable object — which is right for a
	 * generic shelf and wrong for a wall mount that should only take an extinguisher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	TArray<FName> AcceptedTags;

	/** How close the object has to be before this socket offers to take it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket", meta = (ClampMin = "1.0", Units = "cm"))
	float SocketRadius = 15.f;

	/**
	 * Seat the moment the object comes in range rather than waiting for release. Off by default:
	 * pulling an object out of your own hand is startling unless the socket is meant to be magnetic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	bool bAutoSnap = false;

	/** Require the object to be facing roughly the right way before it will seat — plugs, keys, tools in a rack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	bool bRequireAlignment = false;

	/** How far off the socket's facing the object may be and still seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket", meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg", EditCondition = "bRequireAlignment"))
	float AlignmentTolerance = 45.f;

	/**
	 * Once seated it cannot simply be grabbed back out; something has to call Eject. For a training
	 * step that ends when the part is fitted, and for anything that should not fall out again.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket")
	bool bLockIn = false;

	/**
	 * How long the object takes to settle into the seat pose. Eased rather than teleported: an object
	 * that jumps into place reads as a glitch, and the short travel is what sells the docking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
	float SeatDuration = 0.25f;

	/** When the translucent preview of the seated object is shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket|Preview")
	EFXR_SocketGhostMode GhostMode = EFXR_SocketGhostMode::OnApproach;

	/**
	 * Actor whose shape the preview takes when nothing is being carried — required by Always, which
	 * has no approaching object to borrow from. Every static mesh on it is drawn, so an extinguisher
	 * previews as body, pin, handle and hose rather than one part of itself.
	 *
	 * A class rather than a level actor: a mount says "an extinguisher belongs here", which is a
	 * statement about a type. An instance reference would break when that actor is deleted or
	 * streamed out, and would not survive the socket being reused in another level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket|Preview", meta = (EditCondition = "GhostMode != EFXR_SocketGhostMode::Off"))
	TSoftClassPtr<AActor> GhostActor;

	/**
	 * Component tags left out of the preview — an effect mesh, a collision proxy, a part that should
	 * read as missing. Components already hidden are skipped regardless, so this is only for things
	 * that are visible on the real object but unwanted on its ghost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket|Preview", meta = (EditCondition = "GhostMode != EFXR_SocketGhostMode::Off"))
	TArray<FName> GhostIgnoreTags;

	/**
	 * Material the ghost draws with. Defaults to the one shipped with the plugin.
	 *
	 * Soft rather than hard, matching the outline and overlay materials in project settings. A hard
	 * default roots the asset through this CDO, which makes it impossible to rebuild from a script and
	 * takes the editor down with an IsRooted assertion if anything tries. It also stops every project
	 * loading a preview material it may never show.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket|Preview", meta = (EditCondition = "GhostMode != EFXR_SocketGhostMode::Off"))
	TSoftObjectPtr<UMaterialInterface> GhostMaterial;

	/** How long the ghost takes to fade in or out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Socket|Preview", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s", EditCondition = "GhostMode != EFXR_SocketGhostMode::Off"))
	float GhostFadeTime = 0.15f;

private:
	/** Seat pose for an object: the socket's position and facing, but the object's own scale. */
	FTransform GetSeatTransform(const UPrimitiveComponent* Driven) const;

	/** Tick only while something is moving — an idle socket costs nothing. */
	void RefreshTickState();
	void ApplyGhostAlpha();

	/** One drawable piece of a ghost: a mesh and where it sits relative to the object's root. */
	struct FGhostPart
	{
		UStaticMesh* Mesh = nullptr;
		FTransform RelativeToRoot = FTransform::Identity;
	};

	/** Decide what the preview should be showing right now, and start it fading there. */
	void RefreshGhost();

	/** Collect the pieces of the preview from the carried object, else from the Ghost Actor class. */
	void GatherGhostParts(const UFXR_Grab* Approaching, TArray<FGhostPart>& OutParts) const;

	/** Pieces of a live actor — exact, including anything a construction script built. */
	void GatherFromActor(const AActor* Actor, TArray<FGhostPart>& OutParts) const;

	/** Pieces of a class, read from its component templates rather than by spawning one. */
	void GatherFromClass(UClass* Class, TArray<FGhostPart>& OutParts) const;

	/** Whether a mesh belongs in the preview at all. */
	bool ShouldGhost(const UStaticMeshComponent* Mesh) const;

	/** Grow or shrink the pool of ghost mesh components to match what is being drawn. */
	void ResizeGhostPool(int32 Count);

	TWeakObjectPtr<UFXR_Grab> Socketed;
	TWeakObjectPtr<UFXR_Grab> Preview;

	// One component per mesh in the previewed object. Created on first use rather than in the
	// constructor: a component may not create subobjects there without breaking Blueprint
	// reconstruction, and the count is not known until something is actually previewed.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GhostParts;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	// Seat animation.
	TWeakObjectPtr<UFXR_Grab> Seating;
	FTransform SeatStart = FTransform::Identity;
	FTransform SeatTarget = FTransform::Identity;
	float SeatElapsed = 0.f;
	bool bSeating = false;

	float GhostAlpha = 0.f;
	float GhostTarget = 0.f;

	// What the seated object's physics were before this socket parked it, handed back so a later
	// grab can restore simulation on release.
	bool bSocketedPhysics = false;
};
