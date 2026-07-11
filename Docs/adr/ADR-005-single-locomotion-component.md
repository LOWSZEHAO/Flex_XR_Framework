# ADR-005 — Single `FXR_Locomotion` component with a mode arbiter

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 2.5

## Context
FlexXR must support teleport, smooth movement, snap/smooth turning, climbing, and comfort vignetting. The
obvious decomposition — one component per mode (`FXR_Teleport`, `FXR_SmoothMove`, `FXR_Turn`, `FXR_Climb`) —
matches how most VR templates ship, and superficially matches FlexXR's own "one component per behavior" rule.

## Decision
Ship **one** `FXR_Locomotion` pawn component containing all modes, gated by checkboxes and driven by presets.
`FXR_TeleportAnchor` and `FXR_TeleportBlocker` remain separate — they are *world* components, not pawn modes.

## Rationale
The modes are not independent; they **must arbitrate**:
- Smooth movement is illegal while a teleport arc is being aimed.
- Snap turn and smooth turn are mutually exclusive.
- Comfort settings (vignette) are global across all modes.
- Hand ownership is shared: a hand aiming a teleport arc cannot simultaneously drive movement, and a hand
  holding an interactable may drive neither.
- Capability fallback (tracked hands → teleport only) is a decision *about the whole set*, not per mode.

Separate components would require an arbiter object to referee them — at which point the arbiter is the real
component and the others are its internals. This is the same reasoning that made two-handed grab a checkbox on
`FXR_Grab` rather than a `FXR_TwoHand` component: **do not surface internal architecture as user-facing
components.**

"One component per behavior" is satisfied: the behavior is *locomotion*, not *teleporting*.

## Consequences
- **Positive:** arbitration is trivial and local; presets can configure the whole locomotion feel in one
  dropdown; a designer adds exactly one component to the pawn; hand-tracking fallback is enforceable in one place.
- **Negative:** the component is larger than any single-mode component would be, and its detail panel is long
  (mitigated by presets, categories, and `EditCondition` so no dead fields are shown).
- Modes are implemented as internal strategy objects, keeping the class from becoming a monolith.

## Reversal path
Modes are internal strategies behind a stable component API. Extracting one into its own component would be a
mechanical move, but would require introducing the arbiter this ADR exists to avoid.
