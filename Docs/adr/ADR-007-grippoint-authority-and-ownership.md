# ADR-007 — GripPoint authority and ownership

**Status:** Accepted
**Date:** 2026-07-14
**Modules:** FXR_Interaction

## Context

Mesh grabbing and GripPoint grabbing were implemented as parallel paths: detection tested the
interactable's activation radius around the driven mesh, and grip points only refined the snap
and pose *after* a grab began. A player could therefore grab a door panel anywhere on the mesh —
including near the hinge, the exact near-zero lever-arm singularity the rotational solver has to
guard against (`MinLeverArm`).

The architecture summary (§5.4) had already stated the intended rule — "detection primitive =
the GripPoint's activation radius, not the render mesh" — so the code had drifted from the
design's intent rather than the design being silent.

Separately, GripPoint ownership on actors with several interactables was undefined: every
interactable on the actor scanned *all* grip points, so a door handle authored for the latch was
equally claimed by a sibling grab.

## Decision

1. **GripPoint presence is authoritative.** An interactable that owns one or more grip points is
   grabbable **only** at those points — the mesh becomes invisible to grab detection. An
   interactable that owns none uses its mesh (activation radius around the driven component,
   procedural grip) as before. There is **no configuration flag** for this on
   `UFXR_InteractableBase`, `UFXR_Grab`, or `UFXR_Latch`; the presence of the asset *is* the
   switch. All grip configuration (hand filter, pose, radius, priority) lives on `FXR_GripPoint`.

2. **`Owners` is a list.** `UFXR_GripPoint` gains an `Owners` array (explicit owning
   interactables). Auto-resolution order when empty:
   1. Nearest ancestor interactable in the component hierarchy.
   2. Otherwise, the actor's interactables: exactly one → it owns the point (zero
      configuration); more than one → **ambiguous** — `Owners` must be set explicitly, and
      author-time validation raises an error naming the candidates.
   3. No interactable on the actor → validation warning; the point never registers.

   Ambiguity is never resolved by priority or guessed at runtime — it fails loudly at author
   time. A grip point may be owned by several interactables, but **at most one owner may be
   enabled at any time**; two simultaneously-enabled owners sharing a point is a validation
   error.

3. **Claim arbitration.** One interactable may claim a given hand at a time. Multiple enabled
   interactables on one actor are legal when they don't share grip points (e.g. `FXR_Grab` on an
   extinguisher body + `FXR_Use` on its squeeze handle).

## Consequences

- **Zero invalid states.** The bad combination ("grip-points-only with no grip point") is
  unrepresentable rather than merely validated.
- **One place to look.** Everything about how hands attach lives on `FXR_GripPoint`; nothing
  bleeds into the interactable's detail panel.
- **The zero-code common path survives.** Drop `FXR_Grab` on a crate → grabbable anywhere. Add a
  handle GripPoint → precise. A natural authoring progression.
- **No duplicated grip points** for multi-state objects (carry-then-hang door): one shared point,
  owners swapped via `SetInteractionEnabled`.
- Registration is push-based: grip points resolve owners at `BeginPlay` and register themselves,
  so grab-time selection iterates a pre-built list (no per-grab actor scan or allocation).

Unchanged by this rule:

- **Palm-push** (contact drive on `FXR_Latch`) is not grabbing — a GripPoint-only door can still
  be shoved with an open palm when contact drive lands.
- **The min-lever-arm guard stays** — a latch without grip points (a drawer front grabbable
  anywhere) can still be gripped near the pivot.
- **Discoverability:** the implicit rule is stated in the `FXR_GripPoint` class tooltip
  ("adding a grip point makes it the only grab surface on this interactable") and enforced by
  the author-time validation messages.

## Rejected alternative

An `EFXR_GrabSource` enum on the interactable (`GripPointsOnly / GripPointsElseProcedural /
ProceduralOnly`): configuration ceremony for a fact the data already declares, and it permits the
invalid "GripPointsOnly with no GripPoint" state — an object that silently can't be grabbed.
Designing so bad states can't be expressed beats validating against them. This is the same
instinct as two-hand grab being a checkbox rather than a component (ADR-005 reasoning) and
Hinge/Lever being presets rather than classes.

## Revisit trigger

A genuine case wanting both handles **and** grab-anywhere on one object (a crate with handles
that is also grabbable by any edge). The escape hatch is additional GripPoints or a rail
GripPoint; if that proves insufficient in practice, revisit — do not add the knob speculatively.
