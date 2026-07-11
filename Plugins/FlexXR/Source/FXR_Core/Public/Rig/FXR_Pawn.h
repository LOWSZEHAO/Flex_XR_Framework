// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_Pawn.generated.h"

class UCameraComponent;
class UFXR_ControllerInteractor;
class UFXR_HandInteractor;
class UFXR_DesktopSimInteractor;
class UFXR_InteractorComponent;
class UFXR_InputConfig;
class UFXR_XRSubsystem;
class IFXR_Interactor;

/**
 * AFXR_Pawn — the FlexXR VR / MR rig.
 *
 * A camera plus two hand roots hosting the controller and tracked-hand interactors, with
 * a desktop-sim fallback. On play it resolves device capabilities via UFXR_XRSubsystem,
 * activates the appropriate interactor set (controllers, hands, or desktop-sim), applies
 * the EnhancedInput mapping, and hot-swaps when the capability picture changes. Drop it
 * into a level (auto-possesses Player 0) to try the rig.
 */
UCLASS()
class FXR_CORE_API AFXR_Pawn : public APawn
{
	GENERATED_BODY()

public:
	AFXR_Pawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** The active interactor for a hand (controller / tracked hand / desktop-sim, per capabilities). */
	IFXR_Interactor* GetActiveInteractor(EFXR_HandSide Side) const;

	/** BlueprintPure fallback for Anim BPs without UFXR_HandVisual: the active interactor's grip (Select) value 0..1. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Rig")
	float GetHandGripAlpha(EFXR_HandSide Side) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	TObjectPtr<USceneComponent> VROrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	TObjectPtr<USceneComponent> LeftHandRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	TObjectPtr<USceneComponent> RightHandRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactors")
	TObjectPtr<UFXR_ControllerInteractor> LeftController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactors")
	TObjectPtr<UFXR_ControllerInteractor> RightController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactors")
	TObjectPtr<UFXR_HandInteractor> LeftHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactors")
	TObjectPtr<UFXR_HandInteractor> RightHand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactors")
	TObjectPtr<UFXR_DesktopSimInteractor> DesktopSim;

	/** EnhancedInput mapping + per-hand Select/Use actions. Author a data asset to enable controller input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Input")
	TObjectPtr<UFXR_InputConfig> InputConfig;

	/** Presentation mode applied on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	EFXR_Mode StartupMode = EFXR_Mode::VR;

	/** Override which interactor set is active. Auto decides from capabilities; Force modes help test hands vs controllers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Rig")
	EFXR_InteractorPreference InteractorPreference = EFXR_InteractorPreference::Auto;

	/** Draw active grab spheres + far rays and print interactor state on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Debug")
	bool bDrawDebug = false;

private:
	UFXR_XRSubsystem* GetXRSubsystem() const;
	void UpdateActiveInteractors();
	void ApplyInputMapping();
	void DrawDebugInteractors();
};
