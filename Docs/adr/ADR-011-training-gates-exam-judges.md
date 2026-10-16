# ADR-011 — Training gates, Exam judges: a GM asset replaces the step graph

**Status:** Accepted · **Date:** 2026-10-16 · **Phase:** 4 · **Revises:** ADR-004 (authoring format and
mode model; its core decision stands)

## Context
ADR-004 gave the training layer a custom runtime over a compiled step array, authored as a `UFXR_StepGraph`
DataAsset: a graph whose steps finished when a named interaction arrived in a named phase, branching and
fanning out, run in three modes — Guided, Practice, Exam — that differed only in how much help was shown.
The runner *watched* and never gated, on the principle that a procedure a trainee cannot fail measures
nothing.

Running it for the first time surfaced three problems that the principle did not survive:

- **The world ran ahead of the procedure.** In Guided and Practice a trainee could attach the part early or
  close the valve early. The runner recorded "out of order" — correctly — but could not undo the change, so
  from then on the steps and the world disagreed. That is a finding in an exam and plain confusion while
  teaching.
- **A step could only finish on what the framework knew to watch.** Completion matched grab, release, press
  and socket events. A latch *reaching closed* was invisible, so "close the valve" finished the moment it was
  let go of, half-turned or not.
- **The modes did not map to how training is actually run.** Guided versus Practice needed explaining twice.
  The owner's industry practice is two modes: train step by step, then examine.

## Decision
1. **Two modes.** **Training** teaches: objects open step by step, so nothing can be done early; the
   highlight and every instruction are shown; timeouts do nothing; there is no score. **Exam** assesses:
   objects are open from the start, every exposed object is judged, and a critical step, a timeout or Max
   Mistakes fails the run.
2. **Two assets.** `UFXR_TrainingSteps` is the procedure as the trainee reads it — ids, names, instructions,
   in order. `UFXR_TrainingGM` is what happens at each step: custom events to fire on Blueprints in the
   level, the step's objects, a timeout and a Critical flag. The GM picks step ids
   from the Steps asset and reports any disagreement between the two before the run starts.
3. **The project finishes a step.** A global `Complete Step` node, wired to whatever event means "done" — a
   grab, a latch's state change, a button. An optional Step Id makes it advance only while that step is
   open, so an event that fires on every use advances the procedure exactly once.
4. **One flow serves both modes.** Each GM event can be marked *Auto Start In Exam*: Training fires it when
   its step arrives, Exam fires it when the run begins. That is how Exam opens objects early enough to get
   wrong, without a second procedure.
5. **Steps run in order.** Branching and fan-out are removed.

## Rationale
- **Gating is right for teaching and wrong for assessment, so each mode gets one.** ADR-004's "watch, don't
  gate" is kept — for Exam, which is the mode that has to measure. Training's job is to leave the trainee
  knowing the procedure, and a world that has drifted from it does the opposite.
- **The project knows what "done" means; the framework cannot.** Moving completion to a node the project
  calls removes the need for the framework to understand every interactable's notion of a finished state.
- **Data over a Blueprint graph.** The same wiring could live in a manager's event graph as a switch on step
  id. As data it reads top to bottom, step by step, and can be checked before the run: an event name that
  does not exist, a Blueprint not in the level, a step one asset has and the other does not. Event names
  are chosen from a dropdown of the Blueprint's own events, so a typo cannot be entered at all.
- **Two assets because two people touch them.** The steps are content a writer or translator edits; the GM
  is wiring a designer edits. Neither should have to read past the other's half.

## What survives from ADR-004
The core decision: a small engine-free runner over a compiled step array, with authoring separate from the
runtime. The array's shape changed — a step now carries its objects, messages, timeout and Critical flag
instead of transitions — and the runner still never learns which assets produced it.

## Consequences
- **Positive:** the world cannot run ahead of the procedure in Training; a step can finish on any condition
  the project can express; a procedure is readable data and is validated before it runs; Exam's judging,
  Max Mistakes and scoring are unchanged in spirit.
- **Negative:** branching and parallel steps are gone. More is wired by hand — every step needs a
  `Complete Step` hooked to something, and Training's gating is only as good as the author's enable and
  disable events. Events are called by reflection and can take no inputs. And `Complete Step` is only as
  useful as the events interactables expose: `FXR_Grab` has no On Grabbed / On Released yet.

## Revisit trigger
If a real procedure needs a branch — the wrong extinguisher leading to a re-teach path — add a "next step"
override to `Complete Step` rather than restoring the graph. If gating by hand proves error-prone across
many steps, add a per-step "open these objects" list that the session gates automatically in Training.
