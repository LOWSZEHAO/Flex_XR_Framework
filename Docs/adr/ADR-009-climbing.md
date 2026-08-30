# ADR-009 — Climbing is a locomotion mode driven by an ordinary interactable

**Status:** Accepted · **Date:** 2026-08 · **Phase:** 2.5

## Context
Climbing needs two things that live on opposite sides of the module boundary: a **grabbable hold** (detection,
grip points, two-hand support — all `FXR_Interaction`) and **play-space movement** (`FXR_Locomotion`). The
obvious implementation puts both in the hold: grab it, and it moves the rig.

That would make the hold a second locomotion system. ADR-005 exists because teleport, smooth move and turn have
to arbitrate — and climbing arbitrates with all three. A hold moving the origin on its own would bypass the
comfort vignette, the enable/disable API, and the "one mode at a time" rule that the single component enforces.

There is also a question the naive version ignores: what happens when you let go.

## Decision
Two pieces, split along the existing dependency direction:

- **`UFXR_ClimbHold`** subclasses `UFXR_InteractableBase` (ADR-003 — subclass the base, no interface) and lives
  in `FXR_Locomotion`. It is almost empty: it marks a hold as climbable, allows a second hand
  (`CanBeginSecondary`), counts hands so releasing one of two does not end the hold, and broadcasts
  `OnGrabbed` / `OnReleased`. **It never moves anything.**
- **`UFXR_Locomotion`** recognises the hold. Each tick it asks the interaction driver what each hand holds,
  casts to `UFXR_ClimbHold`, and does the movement itself.

The movement treats **the hand as the fixed point**:

```
on grab:   Anchor[hand] = hand grip world location
each tick: Origin += Anchor[driver] − hand grip world location
```

Pull your hand down and the rig rises, because the hand is being put back where it was. Anchors are re-taken on
every fresh grab, and the **newest grip leads** — that is what makes hand-over-hand work. If the leading hand
lets go while the other still holds, the survivor becomes the driver *and re-anchors*, or the rig would snap by
however far that hand had travelled since it grabbed.

**Letting go above the floor falls.** Gravity is scoped to exactly this case: a downward accelerating slide that
line-traces beneath the *head* (not the origin — after a climb the two can be far apart) and stops on the first
surface. Walking off a ledge deliberately does not fall.

## Rationale
- Keeping the movement in the arbiter is what makes climbing yield correctly: both hands are busy by definition,
  so the stick modes stand down, and `SetLocomotionEnabled(false)` stops a climb like it stops everything else.
  A self-moving hold would need every one of those rules re-implemented, and would drift out of step.
- Anchoring absolutely, rather than accumulating frame-to-frame deltas, means tracking jitter cannot integrate
  into drift over a long climb.
- Shipping climbing *without* a fall would repeat the mistake this phase already had to fix elsewhere: an option
  that looks finished and leaves the player hanging in mid-air. Falling is what makes the feature true.
- Scoping gravity to post-climb is a deliberate boundary, not an oversight. General gravity means ground
  detection, step height, slope limits and collision response — a movement layer `AFXR_Pawn` does not have and
  should not grow by accident. Climbing is the only thing in the rig that puts the player in the air, so it is
  the only thing that has to put them back down.

## Consequences
- **Positive:** climbing costs one nearly-empty class; it inherits grip points, hand poses, two-hand support and
  detection for free; it obeys every locomotion rule without restating any of them; the dependency
  `FXR_Locomotion → FXR_Interaction` is now justified by shipping behaviour rather than by intent.
- **Negative:** the locomotion component knows a concrete interactable type. The alternative — a callback from
  the hold — would invert the dependency and put locomotion knowledge inside interaction, which the module rules
  forbid outright. Locomotion depending on interaction is the sanctioned direction.
- No climb-off velocity: releasing at the top of an arc drops straight down rather than carrying momentum.
  Vaulting and jump-from-climb want a velocity model, and belong with the wider movement layer.
- Climbing does not drive the comfort vignette. The motion is driven by the player's own arm at their own pace,
  which is the case vignettes are least needed for.
