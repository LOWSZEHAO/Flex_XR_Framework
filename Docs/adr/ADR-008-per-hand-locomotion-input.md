# ADR-008 — Locomotion input is described per hand, not per mode

**Status:** Accepted · **Date:** 2026-08 · **Phase:** 2.5

## Context
The first locomotion input model was organised by *mode*: `Allow Teleport` and `Allow Smooth Move` booleans, a
`Teleport Hand` and a `Move Hand` picker, a `Turn Hand` picker, and one Input Action per mode
(`IA_FXR_Teleport`, `IA_FXR_Move`, `IA_FXR_Turn`).

Three problems followed from that shape, and none of them were fixable by validation:

1. **It could express layouts that cannot work.** Teleport and smooth move both consume the stick's forward
   axis, so `Teleport Hand = Left` with `Move Hand = Left` is not a mistake to warn about — it is a state that
   should not be representable. The same is true of a hand asked to strafe *and* snap-turn with one X axis.
2. **A per-mode action cannot say which hand pressed it.** Enhanced Input reports the action, not the source
   binding. Binding both thumbsticks to one `IA_FXR_Teleport` so that either hand may teleport meant the *left*
   stick could raise the *right* hand's arc, because the handler had no way to know which stick moved.
3. **A hand-layout change was an asset edit.** Swapping which hand teleports meant reopening the mapping
   context, not changing a value on the component.

Presets (`Comfort` / `Standard` / `Free` / `Custom`) sat on top of this, silently rewriting seven fields across
two sections whenever one was touched.

## Decision
The panel describes **each hand**, and the input layer supplies **one Axis2D action per hand**:

```
Hands
  Left Hand        None | Teleport | Smooth Move
  Left Turn Mode   None | Snap | Smooth
  Right Hand       None | Teleport | Smooth Move
  Right Turn Mode  None | Snap | Smooth

Input
  Left Stick Action    IA_FXR_Stick_L   (Axis2D)
  Right Stick Action   IA_FXR_Stick_R   (Axis2D)
```

A thumbstick has two axes and the component spends them explicitly:

| Axis | Meaning |
|---|---|
| **Y** (forward) | that hand's movement — teleport aim, or smooth-move forward. Never both. |
| **X** (sideways) | that hand's turn mode; strafe instead when its turn mode is `None`. |

Hand-tracking has no stick, so it supplies its intent through a third interactor channel,
**`IFXR_Interactor::GetNavigateValue()`** — the middle-finger pinch. Index pinch stays `GetSelectValue()`
(grab). Controllers return `0` and are steered through Enhanced Input as above.

**Presets are removed.** Defaults are chosen so the fields read as the layout they produce.

## Rationale
- An action per *hand* carries the one fact a per-mode action structurally cannot: which hand acted. Every
  routing decision the component makes needs it.
- Making a clash unrepresentable beats detecting it. There is no "both modes on one hand" warning to write,
  because the enum cannot say it.
- It is *fewer* moving parts, not more: three Input Actions became two, and seven preset-controlled fields
  became four that mean what they say.
- Two fingers for two verbs is what the Meta Interaction SDK does, and what the Quest system UI trains every
  user on. Distinguishing grab from teleport by *proximity* instead — the alternative considered — leaks at the
  boundary in three ways: a pinch that just misses its target teleports you, an idle pinch teleports you, and
  standing beside a prop stops you travelling at all.
- Presets that rewrite half a panel make the panel unreadable: a value no longer tells you what it does,
  because touching a neighbour may change it.

## Consequences
- **Positive:** invalid layouts cannot be authored; the mapping context never needs reopening to change a hand
  layout; grab and locomotion gestures never collide on hand tracking; one mental model — hold to aim, release
  to commit — covers stick and pinch alike.
- **Negative:** `IFXR_Interactor` carries a third channel that only tracked hands implement. It is given a
  `{ return 0.f; }` default rather than being pure virtual, so controller and desktop-sim sources opt out by
  saying nothing — the interface states a capability difference rather than forcing two stub overrides.
- A hand set to `Smooth Move` cannot also turn: its X is strafe. `Turn Mode = Both` falls through to whichever
  hand can actually turn rather than silently doing nothing.
- Both hands set to `Smooth Move` sum their contributions, capped to `Smooth Move Speed`, rather than one
  silently winning.
- Projects upgrading from the preset model must re-author the four Hands fields once; the old properties are
  gone, so they read as defaults rather than misbehaving.
