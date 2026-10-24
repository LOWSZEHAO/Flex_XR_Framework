// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/FXR_TrainingGM.h"
#include "Runtime/FXR_SessionReport.h"
#include "Runtime/FXR_StepRunner.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_TrainingSession.generated.h"

class UFXR_InteractableBase;
class UFXR_TrainingSteps;
struct FFXR_InteractionEvent;
struct FFXR_StepInfo;
struct FFXR_SubStepInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFXR_StepEvent, FName, StepId, FText, Instruction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFXR_SubStepEvent, FName, StepId, FName, SubStepId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_MistakeEvent, FFXR_Mistake, Mistake);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_SessionFinishedEvent, FFXR_SessionReport, Report);

/**
 * UFXR_TrainingSession - runs a procedure in the level.
 *
 * Point it at a GM. As each step (or sub-step) arrives it fires that step's GM events on the
 * Blueprints in the level, which is where objects get enabled/disabled, and in Training it
 * highlights the step's objects. It also passes every interaction event to the runner, which
 * judges it and, in Exam, can fail the run. Steps are completed by the project calling Complete Step.
 *
 * Put it on a manager actor. FXR_Interaction doesn't know about it, so a game can use the same
 * interactables without loading the training module at all.
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

	/** Compile the GM and start at the first step. Logs and does nothing if there's no GM. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StartSession();

	/** End early and clear any highlight the session added. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StopSession();

	/**
	 * Finish the open step, or tick off the open sub-step if the step has sub-steps. The Complete
	 * Step node calls this.
	 *
	 * With an id it only works when that step or sub-step is open, so an event that fires on every
	 * grab still only advances once. A step with sub-steps ignores its own id. With no id it finishes
	 * whatever is open. Returns whether anything was finished.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	bool CompleteStep(FName StepId);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool IsRunning() const { return Runner.IsRunning(); }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	float GetElapsedSeconds() const { return Runner.GetElapsedSeconds(); }

	/** Training or Exam. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	EFXR_DeliveryMode GetMode() const { return Mode; }

	/** Max Mistakes in Exam, 0 in Training. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetMaxMistakes() const { return Mode == EFXR_DeliveryMode::Exam ? MaxMistakes : 0; }

	/** Procedure name, or the Steps asset's name if it doesn't have one. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetProcedureName() const;

	/** The course this procedure belongs to, or empty. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetCourseName() const;

	/** The open step's id, or None before and after the run. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FName GetCurrentStepId() const;

	/** The open sub-step's id, or None when the open step has none. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FName GetCurrentSubStepId() const;

	/** The open step as a one-element array, or empty. Left over from when steps could run in parallel. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FName> GetActiveStepIds() const;

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FFXR_Mistake> GetMistakes() const { return Runner.GetMistakes(); }

	/** Number of steps (sub-steps not counted). Before the first run it's read from the Steps asset. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepTotal() const;

	/** Steps completed so far. A step with sub-steps counts once its last sub-step is done. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepsCompleted() const;

	/** The open step's instruction. Useful for a panel created mid-run. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetActiveInstruction() const;

	/** Builds the report on demand. Fine to call mid-run. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FFXR_SessionReport BuildReport() const;

	/** The session in this world, or null. Warns if there's more than one. */
	static UFXR_TrainingSession* FindInWorld(const UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepActivated;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepCompleted;

	/** A sub-step opened. For a step's first sub-step this comes right after On Step Activated. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SubStepEvent OnSubStepActivated;

	/** A sub-step was ticked off. If it was the last one, On Step Completed follows. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SubStepEvent OnSubStepCompleted;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_MistakeEvent OnMistake;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SessionFinishedEvent OnSessionFinished;

protected:
	/** The GM to run (it points at the Steps asset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	TObjectPtr<UFXR_TrainingGM> GM;

	/** Training: one step at a time. Exam: objects open early and mistakes count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;

	/**
	 * Exam only: fail when the mistake count reaches this. 0 = no limit. Timeouts and Critical
	 * mistakes fail the run on their own.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (ClampMin = "0", EditCondition = "Mode == EFXR_DeliveryMode::Exam", EditConditionHides))
	int32 MaxMistakes = 0;

	/** Start on BeginPlay. Turn off to start from the panel's Start button or your own logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bAutoStart = true;

	/** Training only: put the Guidance highlight on the open step's objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (EditCondition = "Mode == EFXR_DeliveryMode::Training", EditConditionHides))
	bool bHighlightCurrentStep = true;

	/** Exam: how much each kind of mistake costs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (EditCondition = "Mode == EFXR_DeliveryMode::Exam", EditConditionHides))
	FFXR_ScoringWeights Scoring;

private:
	void HandleBusEvent(const FFXR_InteractionEvent& Event);

	/** The interactable with this id, from the registry, or null. */
	UFXR_InteractableBase* ResolveTarget(FName InteractionId) const;

	/** Fire an entry's events for the current mode (see FFXR_StepAction::FiresOnArrival). */
	void FireEntryEvents(int32 EntryIndex);

	/** Exam: fire every Auto Start In Exam event before the first step opens. */
	void FireExamStartEvents();

	/** Call one event on every matching actor in the level. */
	void FireEvent(const FFXR_StepAction& Action) const;

	/** Check every event against the level up front, so a typo shows up at the start, not at step nine. */
	void ValidateEvents(const TArray<FFXR_CompiledStep>& Entries) const;

	/** Training's highlight on an entry's objects. */
	void SetGuidance(int32 EntryIndex, bool bOn);

	//~ The runner has a flat list; these group it back into steps (same StepIndex = same step).
	bool IsFirstOfStep(int32 EntryIndex) const;
	bool IsLastOfStep(int32 EntryIndex) const;

	/** Steps asset text for an entry's step or sub-step, or null. */
	const FFXR_StepInfo* GetStepInfo(const FFXR_CompiledStep& Entry) const;
	const FFXR_SubStepInfo* GetSubStepInfo(const FFXR_CompiledStep& Entry) const;

	/** Clear everything this session added (highlight). */
	void ReleaseAll();

	const UFXR_TrainingSteps* GetStepsAsset() const;

	FFXR_StepRunner Runner;
	FDelegateHandle BusHandle;

	/** Each entry's events, parallel to the runner's entries. Copied from the GM on start. */
	TArray<TArray<FFXR_StepAction>> StepEvents;

	/** Number of steps in the last run. */
	int32 StepCount = 0;

	/** Ids currently highlighted, so they can be cleared later. */
	TSet<FName> GuidedIds;
};
