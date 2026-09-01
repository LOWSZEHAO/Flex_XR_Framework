# ADR-010 — Extension surface: what is open, what is closed

**Status:** Accepted · **Date:** 2026-09 · **Phase:** 3

## Context
FlexXR is pitched as a framework a studio can adopt, which raises a fair question: how much of it can be
extended without forking? Auditing the code at the end of Phase 3 gives an honest answer.

**Open, and proven so:**
- **New interactable types** — subclass `UFXR_InteractableBase` (ADR-003). Six subclasses exist, and the
  important one is `UFXR_ClimbHold`, which lives in **`FXR_Locomotion`** — a higher module added an entirely
  new interactable type without touching `FXR_Interaction`. The contract works across a module boundary, not
  just internally.
- **New input sources** — subclass `UFXR_InteractorComponent`. Three exist (controller, tracked hand, desktop
  sim); everything downstream talks to `IFXR_Interactor`, so a new device is invisible to it.
- **Presentation assets** — outline, overlay, hull, ray mesh and ray material are soft pointers in project
  settings. The stencil contract (1 Hover, 2 Guidance, 3 Selected) is documented, so a replacement
  post-process material only has to honour that.
- **Presetting** — a Blueprint subclass of any component sets defaults without a wrapper. The subclass *is*
  the type, so detection, the driver and the solver treat it identically.
- **Training** — the event bus plus `Expose To Training` / `Interaction Id`, so `FXR_Interaction` never
  learns that training exists.

**Closed, deliberately or otherwise:**
1. **Narrow-phase scoring.** `UFXR_InteractionSubsystem` has no virtuals. Changing candidate selection means
   editing framework source.
2. **Highlight styles and tiers.** `EFXR_HighlightStyle` and `EFXR_HighlightTier` are closed enums.
3. **Locomotion modes.** `UFXR_Locomotion` is a single-component arbiter (ADR-005).
4. **Blueprint-authored interactable types.** The lifecycle virtuals are plain C++ virtuals, not
   `UFUNCTION`s, and they take `IFXR_Interactor*`, which is not Blueprint-exposable as-is.

## Decision
Open (1). Keep (2), (3) and (4) closed for now, and record why, so the reasoning is inherited rather than
re-derived.

**(1) Scoring opens, as a policy object rather than a virtual.** It is the most likely adopter request — gaze
weighting, approach angle, a game with its own notion of "best candidate". `UFXR_ScoringPolicy::ScoreCandidate`
ranks one already-in-reach candidate, lower wins, and a project points **Project Settings → FlexXR —
Interaction → Scoring Policy** at its subclass.

A virtual on `UFXR_InteractionSubsystem` was tried first and abandoned. A world subsystem cannot be cleanly
substituted in UE 5.8: `FSubsystemCollectionBase::SubsystemMap` keys every instance by its **concrete** class,
so the moment a project subclasses the subsystem, `GetSubsystem<UFXR_InteractionSubsystem>()` returns null and
detection silently stops. The 5.8 replacement, `GetSubsystemArrayCopy`, returns the array **by value** — a heap
allocation per lookup, in a path required to be allocation-free. A separate object sidesteps the whole
lifecycle question, is discoverable in the settings panel rather than only in source, and is resolved and
cached once.

Ranking only, with no veto: eligibility is already decided by `IsGrabTarget` / `IsInteractionEnabled` /
`CanBegin`, and two rejection paths can disagree.

**(2) Styles stay a closed enum.** The closed set is what keeps the state→style map coherent and gives
training a fixed vocabulary: `FXR_Training` says "highlight the pin, Guidance state" and never learns what
Guidance looks like. Widening the enum weakens exactly the guarantee that makes one interaction layer serve
both markets. Nothing has asked for a fourth style; three cover Hover / Guidance / Selected with headroom,
and a project that wants a different *look* already repoints the materials.

**(3) Locomotion modes stay closed** because ADR-005 decided that, and the single arbiter is what makes
"locomotion yields to interaction" tractable. Opening it relitigates that ADR and needs its own.

**(4) Extension is C++; binding is Blueprint.** A studio builds products in Blueprint — placing components,
setting properties, handling `On Use Started` / `On Ray Selected` / `On Teleported` — and writes C++ to add
new mechanics. This is the Gameplay Ability System's position and it has not held anyone back. The detail
panel is treated as the API surface it is: categories, `EditCondition`, tooltips, defaults that mean a
dropped-in component already works.

## Consequences
- **Positive:** three deliberate seams instead of a dozen speculative ones. Every extension point is a
  contract that cannot be changed once adopted, so surface area is spent where demand is demonstrated.
- **Positive:** the closed enum keeps training's vocabulary fixed, which is the mechanism behind the
  one-layer-two-markets claim, not a side effect of it.
- **Negative:** a project that wants a genuinely new highlight style must fork or wait. Accepted: they can
  restyle the existing three through materials, which covers the realistic cases.
- **Negative:** no Blueprint-only studio can author a new interactable type. Accepted — a team adopting an
  interaction framework has C++ engineers.
- The audit itself is the point. "Extensible" is a stronger claim when the exceptions are named.

## Reversal path
- **Styles → data assets.** Replace the enum with a `UFXR_HighlightStyle` data asset and map state → style
  asset, shipping the current three as the curated default set. Training is unaffected, because it names
  states and never styles. This is the migration to reach for if a real project needs a fourth style; do not
  simply widen the enum, which gets the extensibility and loses the coherence.
- **Locomotion modes** — needs a new ADR superseding ADR-005, not a code change.
- **Blueprint lifecycle** — `BlueprintNativeEvent` on selected hooks (most plausibly a "may this grab begin"
  validation), which requires replacing `IFXR_Interactor*` in those signatures with a Blueprint-exposable
  handle. Cheaper to do deliberately for two hooks than reflexively for all of them.
