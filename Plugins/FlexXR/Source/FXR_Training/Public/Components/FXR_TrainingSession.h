// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Runtime/FXR_SessionReport.h"
#include "Runtime/FXR_StepRunner.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_TrainingSession.generated.h"

class UFXR_InteractableBase;
class UFXR_StepGraph;
struct FFXR_InteractionEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFXR_StepEvent, FName, StepId, FText, Instruction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_MistakeEvent, FFXR_Mistake, Mistake);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_SessionFinishedEvent, FFXR_SessionReport, Report);

/**
 * UFXR_TrainingSession — runs one procedure against the live world.
 *
 * The runner is the judge and deliberately cannot reach the world; this is the part that can. It
 * subscribes to the interaction event bus, feeds what arrives to the runner, and turns the runner's
 * answers back into things the player can see: the Guidance highlight on the step's target, and,
 * where an author has opted in, the interlock.
 *
 * Drop it on a game mode, a manager actor, or the pawn. Nothing in FXR_Interaction knows it exists,
 * which is what lets a game ship the same interactables with no training module loaded at all.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_TRAINING_API UFXR_TrainingSession : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_TrainingSession();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Compile the graph and begin. Does nothing without a graph, and logs why. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StartSession();

	/** End early. Any interaction this session locked is handed back unlocked. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StopSession();

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool IsRunning() const { return Runner.IsRunning(); }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	float GetElapsedSeconds() const { return Runner.GetElapsedSeconds(); }

	/** Every step currently open. More than one means the procedure allows them in any order. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FName> GetActiveStepIds() const;

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FFXR_Mistake> GetMistakes() const { return Runner.GetMistakes(); }

	/** How many steps the running procedure has. Zero when nothing is running. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepTotal() const { return Runner.GetSteps().Num(); }

	/** How many of them are done. Cheap, for a panel; BuildReport is the whole picture. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepsCompleted() const;

	/**
	 * The instruction for the first step currently open, read back out of the authoring graph.
	 * For a panel that appears part-way through a run, which otherwise has nothing to show until
	 * the next step opens.
	 */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetActiveInstruction() const;

	/**
	 * Everything the run produced, built on demand. Safe to call mid-session for a live panel
	 * and after it for the record; a step still open simply has no close time.
	 */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FFXR_SessionReport BuildReport() const;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepActivated;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepCompleted;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_MistakeEvent OnMistake;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SessionFinishedEvent OnSessionFinished;

protected:
	/** The procedure to run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	TObjectPtr<UFXR_StepGraph> Graph;

	/** How much help. One graph, three dials; the run is scored the same either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Guided;

	/** Start as soon as play begins. Clear it to drive the session from your own logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bAutoStart = true;

	/**
	 * Put the Guidance highlight on the open step's target while hints are allowed. The highlight
	 * system is asked for a semantic state, not an appearance, so this never decides what guidance
	 * looks like.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bHighlightCurrentStep = true;

	/** What a mistake costs. Expand to tune; the defaults are a starting point, not a standard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	FFXR_ScoringWeights Scoring;

private:
	void HandleBusEvent(const FFXR_InteractionEvent& Event);

	/** The interactable a step is authored against, via the registry. Null when nothing matches. */
	UFXR_InteractableBase* ResolveTarget(FName InteractionId) const;

	/** Guidance highlight and, for a hard-locked step, the interlock. */
	void ApplyStepPresentation(int32 StepIndex, bool bOpen);

	/** Close every hard-locked interaction before the run starts. */
	void LockAllGatedSteps();

	/** Hand back everything this session touched, whatever state the run ended in. */
	void ReleaseAll();

	FFXR_StepRunner Runner;
	FDelegateHandle BusHandle;

	/** Ids this session disabled, so it only ever re-enables what it closed itself. */
	TSet<FName> LockedIds;

	/** Ids currently showing Guidance, so they can be cleared without re-resolving. */
	TSet<FName> GuidedIds;
};
