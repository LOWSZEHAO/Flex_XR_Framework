# ADR-006 — Room-scale teleport moves the play-space origin, not the pawn root

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 2.5

## Context
In a room-scale VR setup the player physically walks within their guardian, so the HMD is offset from the pawn
root by an arbitrary, changing amount. The naive teleport — `SetActorLocation(TargetLocation)` on the pawn —
moves the *root* to the target, which means the player's *head* lands somewhere else entirely: offset by however
far they had physically walked. Players consistently land beside, behind, or inside the thing they aimed at.

## Decision
Teleport moves the **play-space origin** such that the HMD's horizontal position lands on the target:

```
NewOrigin = Target − (HMDWorldLocation − OriginWorldLocation)   // horizontal components only
```

The locomotion component obtains the rig through an **`IFXR_LocomotionOwner`** interface implemented by the
FlexXR pawn (exposing the origin/tracking-space component and the HMD component). If the owner does not
implement the interface, the component falls back to `SetActorLocation` and logs a warning; the editor
validation panel reports it at author time.

The locomotion component **never casts to a concrete pawn class.**

## Rationale
- Correct landing is the difference between a teleport that feels precise and one that feels broken — and in
  SOP training, standing in the wrong spot invalidates the procedure.
- Interfacing (rather than casting) keeps `FXR_Locomotion` reusable in any project's pawn, including a game
  team's own `ACharacter` subclass. A concrete cast would hard-couple the framework to one pawn class — the exact
  layering violation FlexXR's module rules exist to prevent.
- `Landing Rotation` modes (Keep Facing / Thumbstick Choose / Face Arc) rotate the origin *about the HMD*, not
  about the pawn root, for the same reason.

## Consequences
- **Positive:** the player's head lands exactly where they aimed, at any guardian offset; framework works with
  any pawn implementing the interface; rotation-on-landing is correct by construction.
- **Negative:** one more interface to implement for teams bringing their own pawn (documented, one-time, and the
  fallback keeps things working meanwhile).
- Seated/standing setups where origin ≈ HMD see no behavioural difference — the maths degenerates correctly.
