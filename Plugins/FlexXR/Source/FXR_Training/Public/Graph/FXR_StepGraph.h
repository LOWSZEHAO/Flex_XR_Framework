// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Events/FXR_InteractionEvent.h"
#include "Runtime/FXR_StepRunner.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_StepGraph.generated.h"

/** An action the author expects here and wants named when it happens. */
USTRUCT(BlueprintType)
struct FFXR_WrongActionDefinition
{
	GENERATED_BODY()

	/** The interaction that counts as this mistake. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName InteractionId;

	/** What to tell the trainee. Shown only when the mode allows a hint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText Message;
};

/**
 * One way out of a step. Several next steps opens them all, which is how an unordered pair like
 * gloves and goggles is written. Several transitions on one step is a branch.
 */
USTRUCT(BlueprintType)
struct FFXR_TransitionDefinition
{
	GENERATED_BODY()

	/** The interaction that completes the step by this route. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName InteractionId;

	/** Which phase counts. Ended is the usual answer: the action happened and finished. Began suits a
	    step that is about reaching for something rather than completing it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_InteractionPhase Phase = EFXR_InteractionPhase::Ended;

	/** Steps to open when this route fires. Leave empty to end the procedure here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> NextSteps;
};

/** One step of a procedure, as an author writes it. */
USTRUCT(BlueprintType)
struct FFXR_StepDefinition
{
	GENERATED_BODY()

	/** Unique within the graph. Other steps point at this name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** Short label for the report and the validation panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	/** What the trainee is asked to do, in their language. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (MultiLine = "true"))
	FText Instruction;

	/** Any one of these completes the step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_TransitionDefinition> Transitions;

	/** Actions worth naming as wrong while this step is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_WrongActionDefinition> WrongActions;

	/** Seconds before the hint escalates. Zero means this step never times out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/**
	 * Hold the interaction closed until this step opens. For legally mandated interlocks only.
	 * Gating is the exception: a world the trainee cannot get wrong measures nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (DisplayName = "Hard-lock until reached"))
	bool bHardLock = false;
};

/**
 * UFXR_StepGraph — a procedure, authored as data.
 *
 * This is one front-end, not the format. ADR-004 keeps authoring separate from the runtime on
 * purpose: this asset compiles down to an array of FFXR_CompiledStep, and a CSV import or a visual
 * graph editor can be added later as another way of producing the same array, with no change to the
 * runner. Enterprise procedures arrive as spreadsheets far more often than as engine assets.
 */
UCLASS(BlueprintType)
class FXR_TRAINING_API UFXR_StepGraph : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Where the procedure starts. Must name one of the steps below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName EntryStep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_StepDefinition> Steps;

	/**
	 * Resolve every name to an index and hand back the runtime format. Reports problems rather than
	 * failing quietly: a step pointing at an id that does not exist is the single easiest mistake to
	 * make in a details panel, and the worst one to debug in a headset.
	 *
	 * Returns false when the graph cannot run at all. OutErrors is filled either way, so a graph
	 * with one bad link still reports it while the rest compiles.
	 */
	bool Compile(TArray<FFXR_CompiledStep>& OutSteps, int32& OutEntryIndex, TArray<FString>& OutErrors) const;
};
