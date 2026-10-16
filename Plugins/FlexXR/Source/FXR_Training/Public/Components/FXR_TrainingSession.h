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
 * UFXR_TrainingSession — runs one procedure against the live world.
 *
 * Point it at a GM. As each step arrives - or each sub-step, where a step has them - it fires the
 * custom events the GM set up for it on the Blueprints the GM names, which is where objects are
 * opened and closed, doors unlocked, sounds played, and in Training it highlights that part's
 * objects. It never decides anything is done: the project calls Complete Step from whatever event
 * means "done". And it hands every interaction to the runner to be judged, which in Exam is what
 * can fail the run.
 *
 * Drop it on a manager actor. Nothing in FXR_Interaction knows it exists, which is what lets a game
 * ship the same interactables with no training module loaded at all.
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

	/** Compile the GM and begin at the first step. Does nothing without a GM, and logs why. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StartSession();

	/** End early. Any highlight this session put up is taken down. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void StopSession();

	/**
	 * Finish the open step and move to the next - or, while a step with sub-steps is open, tick off
	 * its open sub-step. The step finishes with its last one. The Complete Step node calls this for you.
	 *
	 * With an id, only that step or sub-step can be finished, and only while it is open - wire an
	 * event that fires every time something is grabbed and it still advances the procedure exactly
	 * once. A step with sub-steps is finished through them, so its own id does nothing. With no id,
	 * it finishes whatever is open. Returns whether anything was finished.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	bool CompleteStep(FName StepId);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool IsRunning() const { return Runner.IsRunning(); }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	float GetElapsedSeconds() const { return Runner.GetElapsedSeconds(); }

	/** Training or Exam. A panel states the mode; it never offers to change it. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	EFXR_DeliveryMode GetMode() const { return Mode; }

	/** The mistake limit in force: Max Mistakes in Exam, zero in Training. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetMaxMistakes() const { return Mode == EFXR_DeliveryMode::Exam ? MaxMistakes : 0; }

	/** The procedure's name, falling back to the Steps asset's own so it is never nameless on a wall. */
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

	/** The open step as a one-entry list, or empty. Kept for layouts written against parallel steps. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FName> GetActiveStepIds() const;

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	TArray<FFXR_Mistake> GetMistakes() const { return Runner.GetMistakes(); }

	/**
	 * How many steps the procedure has. Sub-steps are not counted: they are parts of a step. Before
	 * the first run this is read from the Steps asset, so a Start screen can say it.
	 */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepTotal() const;

	/** How many of them are done. A step with sub-steps is done when its last one is. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	int32 GetStepsCompleted() const;

	/** The open step's instruction, from the Steps asset. For a panel that appears mid-run. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetActiveInstruction() const;

	/** Everything the run produced, built on demand. Safe mid-session for a live panel. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FFXR_SessionReport BuildReport() const;

	/** The only session in the world, or null. Warns rather than picking when there are several. */
	static UFXR_TrainingSession* FindInWorld(const UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepActivated;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_StepEvent OnStepCompleted;

	/** A sub-step opened. Its step's On Step Activated has already fired when it is the first. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SubStepEvent OnSubStepActivated;

	/** A sub-step was ticked off. On Step Completed follows when it was the step's last. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SubStepEvent OnSubStepCompleted;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_MistakeEvent OnMistake;

	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_SessionFinishedEvent OnSessionFinished;

protected:
	/** What happens at each step - and, through it, the steps themselves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	TObjectPtr<UFXR_TrainingGM> GM;

	/** Training teaches one step at a time; Exam opens what the GM marks and judges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;

	/**
	 * Exam only: the run fails on the mistake that reaches this many. Zero means no limit. Timeouts
	 * and mistakes on a Critical step fail the run by themselves and are not counted toward it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (ClampMin = "0", EditCondition = "Mode == EFXR_DeliveryMode::Exam", EditConditionHides))
	int32 MaxMistakes = 0;

	/** Start as soon as play begins. Clear it to begin from the panel's Start screen or your own logic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bAutoStart = true;

	/**
	 * Training only: put the Guidance highlight on the open step's objects. The highlight system is
	 * asked for a semantic state, not an appearance, so this never decides what guidance looks like.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (EditCondition = "Mode == EFXR_DeliveryMode::Training", EditConditionHides))
	bool bHighlightCurrentStep = true;

	/** Exam: what a mistake costs. The defaults are a starting point, not a standard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training",
		meta = (EditCondition = "Mode == EFXR_DeliveryMode::Exam", EditConditionHides))
	FFXR_ScoringWeights Scoring;

private:
	void HandleBusEvent(const FFXR_InteractionEvent& Event);

	/** The interactable with this id, via the registry. Null when nothing matches. */
	UFXR_InteractableBase* ResolveTarget(FName InteractionId) const;

	/** An entry's own events, as it opens. In Exam, those marked Auto Start In Exam are skipped. */
	void FireEntryEvents(int32 EntryIndex);

	/** Exam: every event marked Auto Start In Exam, before the first step arrives. */
	void FireExamStartEvents();

	/** Call one event on every matching Blueprint in the level. */
	void FireEvent(const FFXR_StepAction& Action) const;

	/** Check every event against the level before the run, so a typo is found now rather than at step nine. */
	void ValidateEvents(const TArray<FFXR_CompiledStep>& Entries) const;

	/** Training's highlight on an entry's objects. */
	void SetGuidance(int32 EntryIndex, bool bOn);

	//~ The runner sees a flat list; these put the steps back. Neighbours sharing a StepIndex are one step.
	bool IsFirstOfStep(int32 EntryIndex) const;
	bool IsLastOfStep(int32 EntryIndex) const;

	/** The Steps asset's text for an entry's step, or for its sub-step. Null when the asset has none. */
	const FFXR_StepInfo* GetStepInfo(const FFXR_CompiledStep& Entry) const;
	const FFXR_SubStepInfo* GetSubStepInfo(const FFXR_CompiledStep& Entry) const;

	/** Take down everything this session put up, whatever state the run ended in. */
	void ReleaseAll();

	const UFXR_TrainingSteps* GetStepsAsset() const;

	FFXR_StepRunner Runner;
	FDelegateHandle BusHandle;

	/** Each entry's events, parallel to the runner's entries. Copied from the GM at start. */
	TArray<TArray<FFXR_StepAction>> StepEvents;

	/** Steps in the run that was last started, sub-steps folded into their step. */
	int32 StepCount = 0;

	/** Ids currently showing Guidance, so they can be cleared without re-resolving. */
	TSet<FName> GuidedIds;
};
