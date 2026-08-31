# FlexXR Framework — Architecture Summary

**Version:** 0.12 (far-ray pointer — where it starts, when it shows, what it is made of)
**Engine:** Unreal Engine 5.8 · C++ core, Blueprint-exposed API · OpenXR
**Targets:** PCVR (priority) · Meta Quest standalone (scalability tier) · MR-ready
**Author:** [your name]

---

## 1. Vision & Positioning

FlexXR is a premium, dual-purpose XR interaction framework for Unreal Engine:

1. **SOP-driven industrial training** — step-based procedure training (e.g. fire safety), with validation, mistake tracking, scoring, and replay.
2. **High-fidelity VR games** — Alyx-grade hand interaction feel, deterministic physics behavior, hand tracking + controller parity.

The core bet: **one interaction layer serves both.** Training modules sit *on top of* the interaction layer and only observe it — games simply never load them.

**Identity statement for the portfolio:**
> FlexXR proves AAA-*capable* interaction fidelity (physics hands, deterministic feel, 90 fps discipline) plus enterprise training infrastructure — in one plugin architecture. FlexXR does **not** ship gameplay systems (combat, AI, inventory); its job ends at "hands touch world, world responds beautifully."

---

## 2. Design Principles — the "Apple Vibe" Contract

| Principle | Meaning in practice |
|---|---|
| **Zero-code common path** | A door, a lever, a grabbable prop: add one component, pick a preset, done. |
| **Enabled & automatic by default** | Drop FXR_Grab → grabbable immediately. Default highlight appears on hover with no setup. Components exist to *customize*, never to switch basics on. |
| **Presets first** | Ship a library of tuned presets (Door — Heavy, Power Grip, Breaker Switch). Designers start from feel, not numbers. |
| **Visual authoring** | Swing-arc gizmos with draggable limit handles, ghost-hand pose preview in viewport. Tune by looking, not typing. |
| **One component per behavior** | Users never add "helper" components that exist for code-architecture reasons (two-hand grab is a checkbox, not a component; the interaction laser is a framework service, not a component). |
| **Consistent feel enforced centrally** | All constrained motion runs through one shared solver; all highlights through one focus manager → every project feels like the same product. |
| **Rich surface, minimal core** | Many components visible; few real systems underneath (one solver, one hand pipeline, one detection/focus pipeline, one input abstraction). |
| **Author-time validation** | The FlexXR editor panel flags misconfiguration (ambiguous driven mesh, missing collision on physics props) at edit time — never as a headset mystery. |
| **Desktop simulation mode** | Mouse-drive interactions in PIE without a headset for instant feel iteration. |

---

## 3. Module Architecture

One plugin, five modules, **strictly one-way dependencies**:

```
┌─────────────────────────────────────────────────────────────┐
│  FXR_Training     (optional — SOP layer)                    │  knows about everything below
├──────────────────────────────┬──────────────────────────────┤
│  FXR_UI                      │  FXR_Locomotion              │  siblings
│  (spatial UI kit, motion)    │  (teleport, smooth, turn)    │
├──────────────────────────────┴──────────────────────────────┤
│  FXR_Interaction  (components + solvers + detection)        │  knows nothing about Training
├─────────────────────────────────────────────────────────────┤
│  FXR_Core         (platform / OpenXR layer)                 │
└─────────────────────────────────────────────────────────────┘
```

### 3.1 FXR_Core — platform layer
- OpenXR abstraction; runtime **device capability detection** (hands? controllers? standalone or PCVR? passthrough?).
- `IFXR_Interactor` — unified input interface. Controller, tracked hand, hand-ray, and desktop-sim mouse all implement it; interaction code never knows which is active. Hot-swaps when the user puts controllers down (Quest).
- Each interactor owns the framework-managed **query shapes**: grab sphere, fingertip probes, far ray (§5.4).
- Input mapping: trigger/grip on controllers ⇄ pinch/index-squeeze on tracked hands.
- **MR-readiness flags** (§10): `EFXR_Mode { VR, MR }`, passthrough toggle, spatial-anchor abstraction.
- FXR event bus: interactions broadcast `InteractionId` events (training subscribes; games ignore).

### 3.2 FXR_Interaction — the heart
User-facing components (§4), the internal systems powering them (§5), the detection/focus pipeline, and the highlight system.

### 3.3 FXR_UI — where the premium feel lives
- Spatial UI kit: panels, buttons, sliders, keypads (auto ray-targetable).
- **One motion-design spec** (durations, easing, spatial-audio ticks, micro-haptics) enforced framework-wide.
- Diegetic guidance primitives: ghost-hand demonstrations, directional arrows (consumed by FXR_Training, usable by games).

### 3.4 FXR_Training — the SOP layer (optional)
Data-driven step graph that **watches** `InteractionId` events and validates them (§7). Scoring, mistake analytics, reports, replay. Games never load it.

### 3.5 FXR_Locomotion — moving the player

Depends on **FXR_Interaction** (never the reverse) for two reasons:
1. **Climbing is grab applied to world geometry** — it reuses `FXR_Grab` and the hand pipeline rather than
   duplicating them.
2. **Locomotion must yield to interaction.** A hand holding a valve cannot simultaneously drive a teleport arc.
   The locomotion component queries interactor state and suppresses input for any hand that currently owns an
   interaction.

Ships: `FXR_Locomotion` (pawn component), `FXR_TeleportAnchor`, `FXR_TeleportBlocker` and `FXR_ClimbHold`
(world components), plus `UFXR_TeleportRegistry` (world subsystem).

---

## 4. Component Reference (user-facing)

### 4.0 UFXR_InteractableBase — the shared foundation

FXR_Grab, FXR_Latch, FXR_Press, FXR_Socket, and FXR_Use all inherit from one base class, so **every interactable shows the same core properties** in its detail panel:

```
Interaction
├─ ☑ Interaction Enabled          (default ON — drop the component, it works)
├─ Interactor Filter:  Any ▾      (hand side / interactor type / gameplay-tag
│                                  conditions, e.g. "State.HasGloves")
├─ Driven Component:   Auto ▾     (see resolution rule below)
Training
├─ ☐ Expose to Training
└─ InteractionId:      "Open_FireDoor_A"
```

**Runtime control API (Blueprint-callable on every interactable):**
- `SetInteractionEnabled(bool)` — the everyday gameplay switch (cutscene → disable; quest unlock → enable).
- **Already-Held Policy** when disabling mid-grab: `FinishNaturally` (can't re-grab after release) or `ForceRelease` (ripped from hand — disarm, stun).
- `ForceRelease()` standalone.
- Disabled objects automatically suppress hover highlights and grip attraction — the UI never advertises what you can't do.
- Games gate with this API; FXR_Training hard-lock steps call **the same API** — one mechanism, two callers.

**Driven Component resolution rule** ("which mesh do I move/affect?"):
1. **Auto (default):** the component's attach parent if it's a mesh/primitive; otherwise the actor's root primitive. Natural setups need zero assignment.
2. **Explicit:** multi-mesh actors → dropdown lists the actor's primitives, pick one. The editor validation panel flags ambiguity at author time.
3. Pivot/mechanism transforms are **cached in actor space at initialization** — so a latch component that is a child of the mesh it rotates never orbits its own pivot (circular-parenting trap, pre-solved).

**Highlight resolution order:** FXR_Highlight component (if present) → project settings. There is no per-interactable dropdown; see FXR_Highlight below for why.

---

### FXR_Grab
Free 6-DOF hold.
- Physics or kinematic hold, snap-to-pose via FXR_GripPoint.
- `☑ Allow Two-Handed` + secondary grip slots (internally the shared two-hand solver — **not** a separate user component).
- **Built-in use events** for the 80% case ("hold grip, pull trigger"): `OnUseStarted / OnUseEnded / UseValue (0–1)`. Trigger on controllers, index-squeeze on tracked hands. A gun or flashlight needs *only* FXR_Grab.
- On release: full physics simulation with inherited velocity.

### FXR_Latch
Constrained motion. **The component is a SceneComponent and its own transform IS the pivot** — attach it, drag it to the hinge edge, done. The swing-arc/travel gizmo draws from the component and redraws live.

```
Motion
├─ Motion Type:  Rotational ▾   (Rotational / Linear)
├─ Motion Axis:  Local Z ▾      (Local X / Y / Z — relative to the
│                                component's own transform, so odd angled
│                                hinges = just rotate the component)
├─ Limits:       0° … 110°      (draggable gizmo handles)
├─ ☑ Allow Palm Push            (push-only rule enforced by solver)
└─ ☑ Allow Two-Handed
```

Internally only **two motion primitives** — rotational and linear — everything else is preset configuration:

| Preset | Primitive | Configuration |
|---|---|---|
| Door | Rotational | Continuous; limits; detent at 0° (unlatch pop); `LatchValue 0–1` |
| Lever / Switch | Rotational | `bSnapToStates`; state array; spring-to-nearest; `OnStateChanged(int)` |
| Dial / Valve | Rotational | Unlimited/multi-turn; optional detents |
| Slide / Drawer | Linear | Limits, resistance, detents |

Grip drive (handle) and contact drive (palm) feed the **same solver** — identical weight and damping either way. Grabbing near the pivot vs. the handle naturally yields coarse vs. fine control (real leverage, free).

### FXR_Press
Poke interactions: buttons, keypads, touchscreens. Fingertip-depth driven, press travel + haptic tick + audio from the motion spec.

### Object structure: what a grab actually moves
A grab moves its **driven component** — the mesh `FXR_Grab` is attached under — and everything attached beneath it. A mesh sitting *beside* the driven one, rather than under it, moves with neither the hold nor the fall.

That is one rule, not two: whatever travels with the hold is whatever physics treats as part of that body. There is deliberately no separate "grab moves this set, physics moves that set", because the two disagreeing is the bug that structure error produces.

| The object is | Build it as |
|---|---|
| **Grabbable** — picked up, carried, dropped | One body. The simulating mesh is the **root**, other meshes attached beneath it (they weld into that body), or merged into a single mesh. |
| **Fixed** — a machine, cabinet or panel bolted in place | Any hierarchy. Parts are driven individually by `FXR_Latch` / `FXR_Press` off their own component transforms; the actor never moves, so no body structure is needed. |
| **A detachable part of a fixed object** | `FXR_Grab` on the part. It drives that part, and the rest of the actor stays put. |

This is not a FlexXR convention — it falls out of the physics engine, and every framework lands on it. Unity's XR Interaction Toolkit *requires* a Rigidbody on a grab interactable, and a Rigidbody owns the colliders beneath it; VRExpansion ships `GrippableStaticMeshActor` so the mesh is the root by construction.

**The symptom that is not general knowledge:** the actor's transform *is* its root component's, so a driven mesh that is not the root moves while `GetActorLocation` stays put. Blueprint logic, save/load and — worst — World Partition streaming relevancy all read that stale location, so a prop carried far enough can be streamed out while it is in your hand. It looks like a random disappearing-object bug and nobody traces it back to component hierarchy.

**Where the trap comes from:** dragging a mesh into a level gives `AStaticMeshActor`, whose root *is* the mesh — correct for free. Creating a **Blueprint Actor** gives `DefaultSceneRoot`, a transform-only node with no body, and every mesh added lands as its sibling. The validation panel (§13, Phase 3) reports this at author time rather than leaving it to be found in a headset.

### FXR_Socket
Snap zones — **pairs with FXR_Grab**: grab object → carry near socket → **ghost preview** appears if the object passes the filter → release in zone (or auto-snap on proximity, per setting) → detaches from hand, attaches to socket. Re-grab pulls it back out.
- Options: accepted-object tag filter, required orientation alignment (plug must face the right way), lock-in (explicit release action to remove).
- Events: `OnHoverStart/End`, `OnSocketed`, `OnRemoved`, and seating emits the socket's `InteractionId` ("docked extinguisher on wall mount" is a validatable SOP step).
- **The socket's own transform is the seat pose** — place the component where the object's origin should end up and point it the way the object should face. No separate offset to author, and the alignment check reads against the same facing.
- Filtering is by **actor tag**, not gameplay tags: a mount needs a short list of names, and pulling in the GameplayTags module for that would widen `FXR_Interaction`'s dependencies for no gain.
- **Nearest accepting socket wins**, so two mounts side by side resolve to the one being reached toward. The pass is driven by the interaction driver alongside the poke pass, so sockets never tick per object.
- Object-side events (the object learning it was docked, whichever socket took it) are deliberately not on `FXR_Grab` yet: they would grow the most-used component's panel for a case most objects never use. Bind the socket.



### FXR_Use *(optional child component)*
> **Rule of thumb:** using the thing is simple → FXR_Grab's built-in events. The usable part is a **physical mechanism on** the thing, or needs its own training ID → add FXR_Use where the mechanism lives.

**Flashlight (no FXR_Use):** add FXR_Grab; bind `OnUseStarted` → toggle light. Done.

**Fire extinguisher (what FXR_Use is for):**
1. FXR_Grab on the body → holdable.
2. FXR_Use child at the squeeze handle; preset **"Squeeze Lever"** (a spring-loaded mini-latch on the rotation solver: travel, resistance, haptics).
3. Assign the handle mesh (Driven Component) → the lever *visually moves* under trigger pull / finger curl.
4. Bind `OnUseValue (0–1)` → spray intensity. `InteractionId = "Squeeze_Handle_Ext01"`.

Justifications: real travel + spring feel; multiple distinct use points per object (pin + handle + nozzle); per-affordance SOP validation.

### FXR_RayTarget
**Not a laser-grab component — the "you can point at me from far away" marker.** The beam itself belongs to the interaction driver, not to this component or to the interactor (see *Far-ray pointer* below); RayTarget only says an object answers to one. Behavior composes with what else is on the object:

| On the object | Point + pinch/trigger does |
|---|---|
| RayTarget alone | Select/focus: hover highlight, `OnRaySelected` (training: "point to the correct extinguisher"; games: examine/scan) |
| FXR_Grab with **Distance Grab** ticked | **Distance grab**: the object flies to the hand, then **FXR_Grab takes over completely** — same grip point, pose blend, everything (gravity-gloves style). A checkbox on Grab rather than a second component: making a grabbable object grabbable-at-range should not need one. |
| FXR_UI panels | Pointer events route into UMG automatically (no manual RayTarget needed) |

**Deliberately no laser-Latch:** dragging doors/valves by ray feels cheap and destroys training fidelity (a trainee who laser-opened a valve learned nothing). Ray-select a latch object = fine; ray-drive it = a game-side custom interactable if truly wanted.

**No Far Interaction Policy.** An earlier draft put distance grab behind a project-wide toggle. Dropped: the per-object checkbox already says it, and a global setting that silently changes how a specific object behaves between projects is worse than the one tick that made it so. A training sim simply leaves the box unticked, which is also the default — physical performance of every motion is what it gets for free.

### Far-ray pointer *(rig-side, not per object)*

The beam lives on `FXR_InteractionDriver`, because near and far interaction have to be arbitrated in one
place: a hand that can reach something must never also point past it at the wall behind. Everything below
is one component's detail panel, under **Far Interaction**.

**Where it starts — `Left Ray` / `Right Ray` on the pawn.** Two `UFXR_RayOrigin` scene components, one per
hand. They carry no settings, because their transform *is* the setting: drag one in the viewport and the
beam follows. One per hand rather than per interactor — the ray should leave the same place whether that
hand is currently a controller or a tracked hand, and two sources to keep in sync would drift apart.

Their transform is read **relative to the parent and composed onto the interactor's tracked pose**, never
used as a world transform. A tracked hand's pose comes from joint data rather than from where a component
sits in the rig, so a world read would simply not follow the hand. Some pitch is normally wanted: a
controller reports its pose along the grip axis, so at zero rotation the beam aims at the floor.

**When it shows — `Ray Visibility`.**

| Mode | Behaviour |
|---|---|
| **On Target** *(default)* | Only while the hand is aimed at something that will answer |
| **Always** | Whenever the hand is free. Reads as a menu pointer; for far-UI-heavy scenes |
| **Never** | No beam; the hover highlight carries it alone |

An always-on beam says nothing and reads as a menu cursor in a scene that is not a menu. This is affordable
because the beam never carried "you can interact with that" alone — far targets hover through the focus
subsystem exactly like near ones, so a distant object lights up whether or not a beam is drawn. Acquisition
stays coarse and refinement is fine: point the hand, the beam arrives, aim from there. The beam is drawn
from the same target the driver already resolved for its own logic, so it cannot disagree with what a press
would do, and it can never point at nothing.

**What it is made of.** An opaque unlit tube. Translucent and additive both compiled clean, reported no
errors and rendered nothing at all on the beam mesh, while a plain opaque material on the very same
component drew immediately — and raising emissive to 60 did not bring the additive version back, so it was
never exposure. A pointer reads as a solid emissive tube in every shipping VR title regardless. An opaque
beam cannot fade its opacity, so **the fade is geometric**: the beam thins to nothing and the cursor scales
with it, which on a tube this narrow reads as retracting rather than dissolving.

`Ray Width` is honest centimetres — the scale comes from the mesh's own bounds, so 1.5 cm is 1.5 cm whether
the tube was authored at radius 1 or radius 10. Colour and brightness are `RayColor` / `RayIntensity` inside
`M_FXR_Ray`; keep intensity at or below 1, since unlit emissive above 1 clips after tonemapping and turns a
cyan beam white. The beam carries no collision at all: it is a picture of a trace, never a participant in
one.

### FXR_GripPoint
"A sticker on the object: hands go here, shaped like this."
- Stores: allowed hand (**Left Only / Right Only / Both**), `UFXR_HandPose` asset, priority, activation radius, snap mode, owners.
- **Left/right at different positions → add two GripPoints with hand filters** (rifle: right hand on pistol grip, left on foregrip). The scorer filters by hand side, so each hand only lands on its own point. **Both** = symmetric spots (mug handle); pose auto-mirrors for whichever hand arrives.
- **Rail variant:** an axis extent — hands grab anywhere along a handrail/hose and slide.
- Runtime scoring (distance + approach angle + side + priority) picks the best point; displayed hand blends into the authored pose over ~100 ms.

**Presence is authoritative (ADR-007).** A GripPoint owned by an interactable ⇒ GripPoints are the **only** way hands attach — the mesh becomes invisible to grab detection. No GripPoint ⇒ the mesh's collision is the grab surface (**procedural grip**: fingers sphere-cast and curl until contact). There is deliberately no enum/checkbox for this on the interactable: presence of the asset is the switch, so the invalid state ("grip-points-only with no grip point") is unrepresentable. All grip configuration lives on FXR_GripPoint. Palm-push (contact drive) is unaffected — a GripPoint-only door can still be shoved with an open palm — and the rotational min-lever-arm guard stays for point-free latches.

**Ownership — `Owners` is a list.** Auto-resolution order:
1. Nearest ancestor interactable in the component hierarchy → owner.
2. Otherwise the actor's interactables: exactly **one** → it owns the point (zero configuration); **more than one** → ambiguous — `Owners` must be set explicitly; validation errors at author time naming the candidates. Never guessed at runtime.
3. No interactable on the actor → validation warning; the point never registers.

A point may be owned by several interactables, but **at most one owner may be enabled at a time** (validated). One interactable claims a given hand at a time; multiple enabled interactables per actor are fine when they don't share points (extinguisher body Grab + squeeze-handle Use).

**The three authoring cases:**

*Case A — single interactable (the ~90% case).* Points parented to the mesh; the sole interactable owns them automatically. Zero configuration.
```
BP_Crate
├── CrateMesh
│   ├── FXR_Grab
│   ├── GripPoint_L        Owners: [Grab]  (auto-resolved)
│   └── GripPoint_R        Owners: [Grab]  (auto-resolved)
```

*Case B — multiple interactables, different grip locations.* Parent each point under its interactable (self-documenting), or set `Owners` manually. For holds that genuinely differ — carrying a loose panel by its edges vs. swinging a hung door by its handle.
```
BP_Door
├── DoorMesh
│   ├── FXR_Grab
│   │   ├── GripPoint_Edge_L     Owners: [Grab]
│   │   └── GripPoint_Edge_R     Owners: [Grab]
│   └── FXR_Latch
│       ├── GripPoint_Handle_L   Owners: [Latch]
│       └── GripPoint_Handle_R   Owners: [Latch]
```

*Case C — multiple interactables sharing one grip location.* One pair of points, owned by both; legal because only one owner is enabled at a time. The detachable-door pattern: the frame's FXR_Socket fires `OnSocketed` → `Grab.SetInteractionEnabled(false)`, `Latch.SetInteractionEnabled(true)` — the same handle now swings the door instead of picking it up; reverse on removal.
```
BP_Door
├── DoorMesh
│   ├── FXR_Grab        (enabled  — carry the loose panel)
│   ├── FXR_Latch       (disabled — not hung yet)
│   ├── GripPoint_L     Owners: [Grab, Latch]
│   └── GripPoint_R     Owners: [Grab, Latch]
```
Known limitation (deliberately unsolved): a shared point carries **one** pose. If two owners need different poses at the same location, fall back to Case B. No per-owner pose override until a real case demands it.

### FXR_Highlight *(optional)*
Every interactable already gets the default highlight automatically (Outline, neutral white on hover) — this component exists only to customize further:

```
├─ Per-state style overrides   (Hover / Guidance / Selected → any style)
├─ Color, intensity, pulse rate
└─ Scope:  Everything ▾   (Everything / Target Mesh)
```

- **Everything** *(default)* — all primitives on the actor + attached children glow as one object (extinguisher incl. pin, handle, hose). Hand meshes and other actors auto-excluded.
- **Target Mesh** — only the driven mesh: "look at this *part*" (SOP guidance on just the safety pin).

**No highlight fields on `UFXR_InteractableBase`.** Defaults come from project settings and this component is
the single place to override them. Putting Style/Color/Sweep on the base would grow every Grab, Latch, Press
and Socket panel with fields they do not need, and would give colour two homes that can disagree. The cost —
recolouring one object means adding a component rather than typing in a field — is the right trade for a rare
case, and it sets the pattern for optional presentation components generally.

**Three highlight styles**, each bound to a semantic state so training and games speak the same language:

| Style | Effect | Default state mapping |
|---|---|---|
| **Outline** | Silhouette edge glow; colour comes from the state, not the object (see §9) | Hover — "you can interact with this" *(framework default style)* |
| **Inner Blink** | Whole-mesh emissive pulse | Guidance — "interact with this NOW" (SOP attention) |
| **Sweep** | Gradient band travels across the object (direction configurable) | Selected/confirm, scan effects, "correct item" feedback |

State→style mapping lives in project settings; FXR_Training only ever says "highlight the pin, Guidance state" — never hardcoding visuals.

### FXR_Locomotion *(pawn component)*

**One component, not four.** Teleport, smooth movement, turning, and climbing are *modes that must arbitrate*:
smooth movement is illegal while a teleport arc is being aimed, snap and smooth turn are mutually exclusive,
comfort settings are global, and hand ownership is shared. Splitting them into separate components would
require an arbiter anyway — so the arbiter **is** the component. (Same reasoning as two-hand grab being a
checkbox, not a component. See ADR-005.)

**The panel describes each hand, not each mode** (ADR-008). A thumbstick has two axes, and the component spends
them explicitly: forward is that hand's movement, sideways is its turn — or strafe, when it has no turn mode.
An unplayable layout is therefore impossible to author rather than something to warn about.

```
── Hands ────────────────────────────────────────────────────
Left Hand:               Smooth Move ▾     (None / Teleport / Smooth Move)
Left Turn Mode:          None ▾            (None / Snap / Smooth)
Right Hand:              Teleport ▾
Right Turn Mode:         Snap ▾

── Smooth Move ──────────────────────────────────────────────
Move Direction Source:   Head Relative ▾   (Head / Hand / Hip Relative)
Smooth Move Speed:       2.5 m/s

── Teleport ─────────────────────────────────────────────────
Aim Style:               Projectile Arc ▾  (Projectile Arc / Straight Ray)
Transition:              Fade ▾            (Fade / Dash / Instant)
Max Distance:            10 m
Validation:              NavMesh ▾         (NavMesh / Surface Angle / Anchors Only / Custom Channel)
Max Surface Angle:       35°               [EditCondition: Validation == Surface Angle]
Landing Rotation:        Keep Facing ▾     (Keep Facing / Thumbstick Choose / Face Arc)
Fade Duration:           0.15 s            [EditCondition: Transition == Fade or Dash]

── Turning ──────────────────────────────────────────────────
Snap Angle:              30°               [EditCondition: either hand snaps]
Smooth Turn Rate:        90 °/s            [EditCondition: either hand turns smoothly]

── Climbing ─────────────────────────────────────────────────
Climb Fall Gravity:      980 cm/s²
Max Climb Fall Speed:    1200 cm/s

── Comfort ──────────────────────────────────────────────────
Vignette:                Dynamic ▾         (Off / Dynamic / Always)
Vignette Strength:       0.6
☑ Vignette On Turn      ☑ Vignette On Smooth Move

── Hand Tracking ────────────────────────────────────────────
Hand Pinch Threshold:    0.7               (middle-finger pinch; index pinch is grab)

── Input ────────────────────────────────────────────────────
Locomotion Context:      IMC_FXR_Locomotion
Left Stick Action:       IA_FXR_Stick_L    (Axis2D)
Right Stick Action:      IA_FXR_Stick_R    (Axis2D)
Teleport Activation Threshold: 0.6

── Visuals ──────────────────────────────────────────────────
Reticle Mesh · Reticle Scale · Reticle Ground Offset
Arc Mesh · Arc Width · Arc Valid/Invalid Material  (fall back to the reticle pair)
Valid Material · Invalid Material · Vignette Material
```

**Transitions.** `Fade` blacks out and back over Fade Duration — a short Fade Duration (~0.06 s) *is* the
"blink" comfort option, which is why there is no separate Blink mode: it was Fade with a hardcoded constant.
`Dash` slides the play space with the world visible, eased at both ends, and is the only transition that creates
optical flow, so it drives the comfort vignette. `Instant` cuts.

**Presets were removed.** Four preset-owned fields had grown to seven across two sections, and a preset that
silently rewrites half a panel makes every value in it untrustworthy. Defaults now read as the layout they
produce.

**Enums:** `EFXR_HandMovement`, `EFXR_TurnMode`, `EFXR_TeleportTransition`, `EFXR_TeleportAim`,
`EFXR_TeleportValidation`, `EFXR_LandingRotation`, `EFXR_VignetteMode`, `EFXR_MoveDirectionSource`.

**Runtime API:** `SetLocomotionEnabled(bool)`, `SetTeleportEnabled(bool)`, `SetTurnEnabled(bool)`,
`TeleportToLocation(FVector, FRotator)`, `IsAimingTeleport()`, `GetVignetteIntensity()`. Same enable/disable
semantics as `UFXR_InteractableBase` — one mechanism, two callers (games gate directly; SOP hard-lock steps call
the same API). `SetTeleportEnabled` gates a runtime flag separate from the authored hand assignment, so a lock
can suspend the mode without forgetting which hand owns it.

### FXR_TeleportAnchor *(world component)*
A fixed, legal destination. With `Validation = Anchors Only`, the player may *only* land on anchors — the strict
industrial variant ("you may stand at exactly these four positions at this machine"). Carries its own
`InteractionId` and optional facing direction. Games use it for designer-authored perches and cover positions.

### FXR_TeleportBlocker *(world component)*
A volume that invalidates any landing point inside it, regardless of what NavMesh says. Hazard zones, edges,
scripted no-go areas. `Box Extent` is a half-size in world cm along the component's own axes — component scale
does not apply, so what the debug box draws is exactly what is tested.

Both anchors and blockers draw in the **level viewport as well as in play** (`Draw Debug`), because a landing
spot and an invisible volume are things you position by eye. Both fire `On Aim` / `On Exit` as the reticle
enters and leaves them, and the anchor adds `On Teleported` — enough to drive a highlight or a sound without
polling. A visual is a Static Mesh Component added under them in the Blueprint.

### FXR_ClimbHold *(world component)*
A ladder rung, ledge or pipe the player can pull themselves along. Subclasses `UFXR_InteractableBase`, so
grabbing it is ordinary detection with grip points and hand poses; it only marks the hold as climbable. The
play space is moved by `FXR_Locomotion`, never from here — see ADR-009. Both hands may hold it, and
hand-over-hand across separate holds works because each grab re-anchors. Letting go above the floor falls.

---

## 5. Core Internal Systems

Many components on the surface, **four real systems** underneath: the constraint solver, the hand presentation pipeline, the detection & focus pipeline, and the input abstraction.

### 5.1 FFXR_ConstraintSolver — kinematic-while-held, physics-on-release

> **While you're touching it, it's a puppet. When you let go, it becomes a real object again.**

Pure physics constraints (`UPhysicsConstraintComponent` + physics handle) are rejected: jitter at 90 Hz, doors shoved through walls, constraint explosions, limit overshoot, non-determinism (poisons SOP validation and replay), wasted Quest CPU.

**While held/touched (per frame):**
1. **Project** the interactor's position onto the constraint manifold — for a hinge: "what angle around my axis (from the cached actor-space pivot) does the hand correspond to?" One scalar; hand-to-pivot distance handled implicitly (real leverage for free).
2. **Feel model:** inertia + damping (= perceived weight), resistance curves, detent torque wells, state springs for levers.
3. **Drive via kinematic physics target** (`SetKinematicTarget`, never raw `SetWorldTransform`) so a moving door still pushes physics bodies and characters correctly.

**Input sources (same solver, same feel):**
- **Grip drive** — hand on a GripPoint; push and pull both valid.
- **Contact drive** — open palm on the surface; targets accepted only in the push direction (a palm can't pull).

**On release:** hand off tracked velocity → doors get a damped kinematic swing integrator; levers spring to nearest state; free props go fully simulated with inherited velocity.

Deterministic, stable, identical on PCVR and Quest; haptic tick per detent for free. Consumers: FXR_Latch, FXR_Use mechanisms, FXR_Grab constrained modes.

### 5.2 Hand Presentation Pipeline (displayed hand ≠ real hand)
- Displayed hand glues to the grip point and obeys constraints; the real tracked hand keeps moving.
- **Tension model:** divergence drives stretch visuals + haptic scaling; past ~25–30 cm the grab breaks. The divergence scalar doubles as "how hard am I pulling" input to the solver.
- ~100 ms pose blend on grab (live → authored). Instant snapping looks cheap; the blend sells the wrap.
- Shared by Grab, Latch, and Use.

### 5.3 Pose Data & Retargeting — UFXR_HandPose

**Problem:** two hand skeletons — the chosen controller hand mesh (arbitrary bones) and the OpenXR tracked skeleton (26 standardized joints, scaled to the player's real hand). Different bone names/counts, rest poses, axis conventions; some runtimes omit metacarpal rotations. Raw bone-rotation poses break on every mesh swap and clip on every hand size.

**Solution — store the idea, not the bones:**
```
UFXR_HandPose  (~11 skeleton-agnostic values: 5 curls, 5 splays, thumb opposition)
        ↓
Retarget profile for the active skeleton   (one-time setup per skeleton)
        ↓
Actual bone rotations
        ↓
Fingertip micro-IK: tips cast to the surface, final curl adjusted
        → exact contact for any hand size; nobody clips
```

Free wins: in-VR recorded poses work on controller meshes instantly; L/R mirroring trivial; mesh swap = one new profile, whole pose library survives. Known cost: one fiddly profile per skeleton (axis flips, rest-pose offsets) — suffered once per skeleton, not per pose.

### 5.4 Detection & Focus Pipeline

Classic **broad phase + narrow phase**, run as a framework service — never per-object collision setup.

**Interactor query shapes (framework-created, designer never touches):**
- **Grab sphere** (~8–10 cm around the palm) → Grab/Latch/Socket/Use candidates
- **Fingertip probes** (tiny sphere casts) → FXR_Press travel, procedural grip, palm-contact points
- **Far ray/cone** → FXR_RayTarget channel

**Broad phase — registry, not physics events.** Every FXR component auto-registers into `UFXR_InteractionSubsystem` (world subsystem + spatial hash). Each frame the grab sphere queries: "which activation radii am I inside?" Why this beats `OnComponentBeginOverlap`:
- **Detection primitive = the GripPoint's activation radius**, not the render mesh — a tiny pin gets a generous grab radius; a huge door doesn't light up at its far corner. Detection shape ≠ visual shape is a major feel win.
- **Zero designer setup** — deletes the #1 VR bug class ("mesh had the wrong collision preset, can't grab").
- **Deterministic order** — overlap events fire unpredictably; polling a small list is stable (SOP replay needs this).
- **Quest-friendly** — sphere-vs-hash checks are near-free; no physics broadphase churn.

**Narrow phase — scoring (where premium feel lives).** Candidates scored by distance, approach angle vs. point facing, hand-side filter, designer priority, enabled state. Winner gets hover (highlight + grip attraction), then the grab claim. Two make-or-break details:
- **Hysteresis** — current best keeps a small score bonus → no highlight flicker between neighbors.
- **Claim locking** — once a hand begins a grab, the candidate locks to it; the other hand can't steal mid-blend.

**The one custom trace channel, `FXR_Interaction`** — for genuinely mesh-accurate hits only: FXR_RayTarget traces, fingertip probes, procedural-grip contact. The framework adds the channel via config and sets responses on its own query components at runtime. Designers touch nothing; if something isn't ray-targetable, that's a framework bug by definition.

### 5.5 Collision Policy (what designers actually do: almost nothing)

| Situation | Designer action |
|---|---|
| Grab / Latch / Socket detection | **Nothing.** Activation radii do detection; mesh collision is irrelevant to grabbing. |
| Physics after release (dropped/thrown props) | Normal UE prop setup: simple collision hull + physics enabled — standard asset work, nothing FlexXR-specific. Auto-generated hulls fine. |
| Finger-accurate contact (palm push, Press, procedural grip) | Default simple collision fine for ~90% of objects. Refine the hull only where fingers must conform closely **and** no authored GripPoint pose covers it. |
| FXR_Interaction trace channel | **Nothing** — auto-configured at registration. |

The editor validation panel backs this up: e.g. *"FXR_Grab with physics-on-release but no simple collision"* flagged at author time.

### 5.6 Shared Utility Systems
- **FFXR_TwoHandSolver** — blends two interactor transforms, twist torque, secondary grips. Surfaced only as a checkbox.
- **Focus & Ray Manager** — single source of truth for hover/focus/selection and laser visibility.
- **Highlight Manager** — applies state→style mapping (§4, FXR_Highlight); per-tier rendering in §9.
- **FXR Event Bus** — `☑ Expose to Training` interactions broadcast `InteractionId`; SOP graph, analytics, or game quest systems subscribe.

### 5.7 Extension Contract — ADR-003: no separate interactable interface

**Decision:** FlexXR does **not** ship an `IFXR_Interactable` interface. `UFXR_InteractableBase`'s virtual lifecycle — `CanBegin / OnBegin / OnUpdate / OnEnd(EFXR_EndReason)` — **is** the extension contract: subclass the base, override the lifecycle, and inherit registration, highlighting, enable/disable semantics, and event emission for free.

**Rationale:** a raw interface would uniquely serve only implementers who *cannot* inherit the base (single-inheritance conflict with another SDK) — a user that does not exist for this project. An interface with exactly one implementer is speculative abstraction; UE interface boilerplate and a second documented extension path carry a small permanent cost. Test mocks simply subclass the base inside the test module.

**Reversal path (cheap):** the detection subsystem is the interface's only would-be consumer. If a non-inheriting implementer ever appears, extracting `IFXR_Interactable` (native-only, single `End(Reason)` exit — no separate `Cancel`) from the existing virtuals is a mechanical refactor. Recorded in `Docs/adr/ADR-003.md`.

### 5.8 Locomotion Systems

**Arc prediction (allocation-free).** `FPredictProjectilePath` with a **persistent scratch buffer** owned by the
component — never a per-frame `TArray`. The spline-mesh pool size is **derived** from
`SimFrequency × MaxSimTime`, never a magic constant, so tuning arc distance cannot silently starve the pool.

**Material state changes are edge-triggered.** Valid/invalid materials are applied only when the validity state
*changes*, not every frame. (Per-frame `SetMaterial` is render-state churn for a value that rarely changes.)

**Room-scale teleport (ADR-006).** Teleport moves the **play-space origin** so that the *HMD* lands on the target
— not the pawn root. The component obtains the rig via an `IFXR_LocomotionOwner` interface implemented by the
FlexXR pawn, and falls back to `SetActorLocation` when the interface is absent. The locomotion component never
casts to a concrete pawn class.

**Comfort vignette.** A post-process/overlay driven by instantaneous linear and angular velocity, with
`Dynamic` scaling strength to speed. Quest tier uses an overlay mesh rather than full-screen post-process
(same per-tier strategy as the Outline highlight, §9).

**Hand-tracking parity — the design problem locomotion actually has.** Bare hands have no thumbstick. Every mode
therefore has a gesture binding resolved through `IFXR_Interactor`:

| Action | Controller | Tracked hand |
|---|---|---|
| Aim teleport | Thumbstick forward / grip hold | Point gesture, palm down |
| Commit teleport | Release | Pinch |
| Cancel teleport | Thumbstick centre | Open palm |
| Snap turn | Thumbstick L/R | Flick gesture, or `Landing Rotation` on teleport |
| Smooth move | Thumbstick | **Not offered by default** — imprecise and uncomfortable without a stick |

**Capability rule:** when the active interactor is a tracked hand and no controller is present, the framework
falls back to **teleport + rotation-on-landing** automatically, regardless of preset. Documented, not silent —
the validation panel reports it.

**Interaction yielding.** A hand that currently owns an interaction (grab, latch, press, ray focus) cannot drive
locomotion. Checked against interactor state each frame; no locomotion input is consumed for that hand.

---

## 6. Hand Pose Authoring — three tiers of effort

| Tier | Workflow | Time |
|---|---|---|
| **1 — Preset library** | Drop FXR_GripPoint, pick "Power Grip" / "Pinch" / "Trigger Grip" / "Flat Palm", nudge the ghost-hand gizmo. | ~30 s, most objects |
| **2 — Pose editor** | Posable ghost hand on the object; **5 curl sliders + splay**; auto-mirror L↔R; preview at multiple hand scales; save as reusable `UFXR_HandPose`. | minutes |
| **3 — In-VR pose recorder** | Wear the headset, grip the actual mesh the way it should look, pinch the off-hand → curls snapshot into the asset. | seconds; flagship demo clip |

Zero-authoring fallbacks: procedural grip (curl until contact) and grip rails.

---

## 7. FXR_Training — the SOP Step Graph

> **A judge, not a controller.** The world stays fully interactive; the graph *watches* `InteractionId` events and validates the performance.

Why watch-mode beats enable/disable gating:
- **Mistakes are the product.** Gated worlds can't record "would have grabbed the extinguisher before the safety check" — watch-mode logs exactly that, and that data is what a safety manager buys.
- The world feels real, not a locked-door theme-park ride.
- Games inherit nothing weird.

**Anatomy of one step** (DataAsset; designers fill a form):
```
Step 3: "Pull the safety pin"
├─ Complete when:  event == "Pull_Pin_Extinguisher01"
├─ Guidance:       Guidance-state highlight on the pin (Inner Blink),
│                  ghost-hand demo, voice line
├─ Wrong actions:  "Squeeze_Handle" → warn "Pin first!", log mistake
├─ Timeout 30 s:   escalate hint (arrow + stronger highlight)
└─ On complete:    → Step 4
```

**Graph, not list:** branching (wrong extinguisher on electrical fire → consequence + re-teach path), parallel steps ("gloves AND goggles, any order"), and modes — *Guided* (full hints) / *Practice* (hints on mistakes) / *Exam* (no hints, scored) — same graph, three dials.

**Opt-in hard-lock** per step (`☑ Hard-lock until reached`) for legally mandated interlocks — implemented via the same `SetInteractionEnabled` API games use. Gating is the exception, never the philosophy.

**Output:** session report — time per step, mistakes, hints consumed, score; replay powered by the deterministic solver.

**Implementation choice (decided — ADR-004):** a custom lightweight step runtime (`FFXR_StepRunner`) consuming a compiled step array, authored via `UFXR_StepGraph` DataAssets — **not** UE StateTree, and no bespoke node editor yet. The authoring format is deliberately separated from the runtime, so front-ends (DataAsset, CSV/JSON import for client SOPs, a future visual graph editor) are swappable with zero runtime churn. See `Docs/adr/ADR-004-sop-step-graph.md`.

Locomotion emits `InteractionId` events on the same bus as interactions. A step may therefore be completed by
*arriving somewhere*:

```
Step 1: "Approach the extinguisher station"
├─ Complete when:  event == "MoveTo_Station_A"      (FXR_TeleportAnchor)
└─ Guidance:       highlight anchor, directional arrow
```

With `Validation = Anchors Only` plus `FXR_TeleportBlocker` hazard volumes, an SOP scene can enforce legal
standing positions — the locomotion equivalent of the opt-in hard-lock.

---

## 8. Interaction Model & Platform Support

### 8.1 Two axes, one pipeline

**Input device** (controller / tracked hand / desktop sim) × **interaction range** (near / far) — every cell works because both axes pass through `IFXR_Interactor` and the far path *funnels into* the near path (distance-grab ends in a normal FXR_Grab hold):

| | Near | Far (ray) |
|---|---|---|
| **Controller** | Grip button on object | Aim ray + trigger |
| **Tracked hand** | Physically pinch/close on object | Point-gesture ray + pinch |
| **Desktop sim** | Mouse hover + click | Cursor as ray |

An object configured once works via all six cells, zero per-method setup. **Far Interaction Policy** (project setting) dials the far column per market: games → distance-grab everywhere; training → ray for UI/selection only, physical motions mandatory.

### 8.2 Platform matrix

| Platform / runtime | Controllers | Hand tracking | Notes |
|---|---|---|---|
| PCVR — Quest Link/Air Link | ✅ | ✅ (Meta path) | Primary hand-tracking dev target on PC |
| PCVR — SteamVR (Index, Vive) | ✅ | ❌ native | No XR_EXT_hand_tracking exposed; Ultraleap add-on only |
| PCVR — Varjo | ✅ | ✅ | XR_EXT_hand_tracking via built-in OpenXRHandTracking |
| Quest standalone | ✅ | ✅ | Scalability tier; smoke-tested every phase, never "ported later" |

No interaction may ever assume a specific input device; capability detection selects the interactor set at startup and hot-swaps at runtime.

---

## 9. Rendering & Performance Strategy

- **Forward renderer + MSAA** baseline; scalability tiers on one codebase: *PCVR high tier* (Lumen permitted, budgeted) and *Quest tier* (baked lighting, mobile feature set, strict draw-call/shader budgets).
- **Highlight rendering, two implementations behind one API:**
  - *Outline* — PCVR: custom depth + post-process material (crisp). Quest: auto-swaps to inverted-hull outline mesh (full-screen post-process is a Quest frame-budget killer).
    **The stencil carries the highlight *state*, not the style** (1 Hover, 2 Guidance, 3 Selected). One full-screen pass serves every outlined object, so it can never read a per-object colour; state is the only axis it can vary along, and encoding it there is what lets hover and guidance differ — and lets a project outline all three states in three colours. A replacement material only has to honour that contract.
    The pass is attached to the view target's camera component (found via the player camera manager), so outlines need no post-process volume and work with any project's pawn. Requires `r.CustomDepth=3` — at `1` the stencil is not written anywhere readable.
  - *Inner Blink & Sweep* — UE5 per-mesh **Overlay Material** slot (cheap on both tiers); Sweep = moving gradient mask in the overlay shader, direction is a vector parameter.
- 90 fps discipline as a framework value: per-tier frame budgets documented; Unreal Insights profiling from Phase 2, not as an afterthought.
- Solver, detection pipeline, and hand pipeline allocation-free per frame; registry queries are sphere-vs-spatial-hash (no physics broadphase churn); hand meshes instanced where possible.
- Phase 5 produces the formal **performance case study** (Portfolio Project 4): Insights captures, budget tables, before/after optimization, PCVR-vs-Quest comparison.

---

## 10. MR Readiness

**Decision:** architectural commitment now, feature pass later. Enterprise training is moving to passthrough (SOPs practiced in the real facility with virtual hazards overlaid); retrofitting MR kills projects, deferring features doesn't.

Day-one in FXR_Core: `EFXR_Mode { VR, MR }` threaded through pawn/rig and rendering setup; passthrough compositing toggle; spatial-anchor abstraction (OpenXR anchor extensions behind an interface). Later MR phase ships: room-aware placement, plane detection + snapping, anchored SOP scenes in real spaces.

---

## 11. Event & Data Flow (one picture)

```
 Player hand / controller / desktop sim
        │  (IFXR_Interactor: grab sphere · fingertip probes · far ray)
        ▼
 UFXR_InteractionSubsystem  — broad-phase registry (activation radii, spatial hash)
        │
        ▼  scored narrow phase (distance · angle · side · priority · hysteresis)
 FXR interactable (Grab / Latch / Press / Socket / Use / RayTarget)
        │
        ├─► Highlight Manager ─► state→style (Outline/Blink/Sweep) per tier
        │
        ├─► FFXR_ConstraintSolver ─► kinematic physics target ─► world responds
        │
        ├─► Hand Presentation Pipeline ─► pose blend, tension, break
        │
        └─► FXR Event Bus: broadcast InteractionId ("Pull_Pin_Extinguisher01")
                    │
        ┌───────────┴─────────────┐
        ▼                         ▼
  FXR_Training step graph   Game systems (quests, scoring)
  (validate, guide, score)  — or nothing at all
```

---

## 12. Naming Conventions

| Thing | Convention | Examples |
|---|---|---|
| User-facing components | `FXR_` + behavior | `FXR_Grab`, `FXR_Latch`, `FXR_Press`, `FXR_Socket`, `FXR_Use`, `FXR_RayTarget`, `FXR_GripPoint`, `FXR_Highlight` |
| C++ component classes | UE prefix + FXR name | `UFXR_Grab`, `UFXR_GripPoint`, `UFXR_InteractableBase`, `UFXR_Locomotion`, `UFXR_TeleportAnchor`, `UFXR_TeleportBlocker` |
| Subsystems / internal systems | `UFXR_` / `FFXR_` | `UFXR_InteractionSubsystem`, `FFXR_ConstraintSolver`, `FFXR_TwoHandSolver`, `FFXR_InteractorState`, `FFXR_ArcPredictor` |
| Interfaces | `IFXR_` | `IFXR_Interactor`, `IFXR_LocomotionOwner` |
| Data assets / settings | `UFXR_` | `UFXR_HandPose`, `UFXR_LatchPreset`, `UFXR_StepGraph`, `UFXR_ProjectSettings` |
| Enums | `EFXR_` | `EFXR_Mode`, `EFXR_EndReason`, `EFXR_HighlightStyle`, `EFXR_HighlightScope`, `EFXR_TurnMode`, `EFXR_VignetteMode` |
| Plugin modules | `FXR_` | `FXR_Core`, `FXR_Interaction`, `FXR_UI`, `FXR_Training`, `FXR_Locomotion` |
| Trace channel | `FXR_` | `FXR_Interaction` |

---

## 13. Development Roadmap

| Phase | Scope | Est. |
|---|---|---|
| **1 — FXR_Core** | Repo scaffolding (README skeleton, `CODING_STANDARDS.md`, CI, ADR seed); pawn/rig, `IFXR_Interactor` (controller + tracked hand + desktop sim), input mapping, capability detection, event bus, MR flags | 3–4 wks |
| **2 — Interaction core** | `UFXR_InteractionSubsystem` + detection pipeline, `FFXR_ConstraintSolver`, InteractableBase (enable API, driven-component rule), FXR_Grab (+ use events, two-hand), FXR_GripPoint + pose pipeline + retargeting, FXR_Latch, FXR_Press | 4–6 wks |
| **2.5 — FXR_Locomotion** | Teleport (arc, validation, room-scale origin, all four transitions, all three landing modes), per-hand control layout (ADR-008), smooth move, snap/smooth turn, comfort vignette, hand-tracking pinch gesture + fallback, anchors & blockers, climbing (ADR-009) | 2–3 wks |
| **3 — FXR_UI + presentation** | Spatial UI kit, motion-design spec, FXR_RayTarget + focus manager, FXR_Socket, highlight system (3 styles, per-tier impls), guidance primitives, validation panel | 3–4 wks |
| **4 — FXR_Training + SOP demo** | Step graph (`FFXR_StepRunner` + `UFXR_StepGraph` DataAssets, per ADR-004), modes, reporting; **fire safety training demo** built entirely on FlexXR | 4–6 wks |
| **5 — Optimization + standalone** | Quest build, Insights profiling, budget enforcement, **performance case study** | 3–4 wks |
| **6 — MR pass + game demo** | Passthrough, planes, anchors; **small action game demo** on FlexXR | 4–6 wks |

Standing rule: Quest build smoke-tested at the end of every phase from Phase 2 onward.

---

## 14. Portfolio Mapping & Scope Boundaries

| Portfolio project | Delivered by |
|---|---|
| Project 1 — Custom XR Runtime Framework | FlexXR itself (FXR_Core + FXR_Interaction) |
| Project 2 — VR Fire Safety SOP Training Sim | Phase 4 demo on FlexXR |
| Project 3 — PC/VR Game Dev Proof | Phase 6 game demo on FlexXR (gameplay systems live in the game project, e.g. GAS — not in FlexXR) |
| Project 4 — Performance & Optimization Case Study | Phase 5 report |
| Project 5 — XR Plugin / Tooling System | FlexXR's plugin architecture, pose editor, in-VR recorder, gizmos, presets, validation panel |

**Explicit non-goals (scope protection):**
- No combat framework, AI, or inventory — those belong to game projects built on FlexXR.
- No claim of "AAA framework" — the claim is **AAA-capable interaction fidelity**, evidenced by the solver, hand pipeline, detection design, and performance discipline.
- SteamVR native hand tracking out of scope (runtime limitation, documented, not fought).
- No default ray-driven Latch (feel + training-fidelity decision, documented in §4).

**Marquee demo clips to record along the way:** in-VR pose recorder ("I record grab poses by grabbing"), palm-push vs handle-pull on the same door, broken-vs-retargeted pose before/after, hysteresis on/off comparison, Guided-vs-Exam split screen, PCVR-vs-Quest side-by-side, highlight styles showcase (Outline → Blink → Sweep).

---

## 15. Engineering Standards & Repository Discipline

### 15.1 C++ Quality Bar (binding, not aspirational)
- **Epic Coding Standard** compliance: naming, brace style, `F/U/A/E/I` prefixes — the first thing studio reviewers judge.
- **Const-correctness & explicit ownership**: UE smart pointers and UObject lifetime rules; never raw `new/delete`.
- **Hot paths allocation-free**: solver, detection queries, and hand pipeline never heap-allocate per frame; pre-sized containers, no per-tick `TArray` churn.
- **Solver as plain C++** (POD structs + pure functions, thin UObject wrapper) → deterministic *and* unit-testable via UE Automation; tests ship in the repo.
- **Module hygiene**: the §3 dependency diagram enforced in `Build.cs`; editor-only code (pose editor, gizmos, validation panel) split into `FXR_*Editor` modules so runtime stays lean.
- **API surface quality**: documented public headers; tidy `UPROPERTY` categories and meta specifiers — this is what makes the detail panels feel designed, not dumped.
- **Insights instrumentation from day one**: `TRACE_CPUPROFILER_EVENT_SCOPE` on every subsystem; Phase 5's case study depends on it.
- `CODING_STANDARDS.md` lands in the repo at Phase 1 and is binding for every commit after it.

### 15.2 Repository & Commit Discipline
- **Commit convention**: imperative mood + module scope — `[FXR_Interaction] Add detection subsystem with spatial hash broad phase`. One coherent system change per commit; never `fix` / `update` / end-of-day dumps.
- **Branch-per-phase, PR into main** (even solo) with real descriptions — a solo repo with clean PRs reads as team-ready workflow.
- **Release tag per phase** (`v0.1-core`, `v0.2-interaction`, …) so project persistence is visible in the release timeline, not just the contribution graph.
- **README from day one**: overview, features, architecture diagram, GIFs (the §14 marquee clips), build instructions, roadmap — sections filled as phases complete.
- **CI**: GitHub Actions runs the solver automation tests on push. An Unreal plugin with CI-run unit tests is a rare, noticed portfolio signal.

### 15.3 Architecture Decision Records
Every non-obvious decision gets a short ADR in `Docs/adr/` (context → decision → consequences → reversal path). Seeded with:
- **ADR-001** — kinematic-while-held, physics-on-release solver (over physics constraints)
- **ADR-002** — registry detection over physics overlap events
- **ADR-003** — no separate interactable interface (§5.7)
- **ADR-004** — custom SOP step runtime + DataAsset authoring, over StateTree / a node editor (§7)

ADRs are the written answer to "can you explain your architecture?" — considered-and-rejected beats cargo-culted-in.

---

## 16. Future-Proofing — UE6, Verse & Scene Graph

**Context (announced June 2026):** UE6 merges UE5 + UEFN around a Verse-based Scene Graph gameplay framework. Early Access targeted end of 2027, full release ~12–18 months later. Actors and Blueprints are supported in early UE6 releases and deprecated only after the new framework matures, with conversion tools promised. UE 5.8 is the last planned UE5 release (a 5.9 is reserved).

**Position:** FlexXR ships on UE 5.8. UE6 is *not* a design target — Scene Graph APIs don't yet exist to target. Two disciplines already binding elsewhere in this doc are the portability insurance:
1. **Pure-logic core** (§15.1): solver math, detection/scoring, and retargeting live in plain C++ operating on transforms — engine-version-agnostic algorithms and data.
2. **Thin exposure layer**: framework logic never lives in Blueprint graphs; Blueprints (later: Verse) are only the user-facing binding surface — swappable by design.

**Migration outlook, when the day comes:** data assets (poses, presets, step graphs) and core algorithms port untouched. The real work is re-housing components in the Scene Graph entity model and re-exposing the API to Verse — the *entity model*, not Blueprints, is the actual moving part. Editor tooling (Slate) is the most fragile layer. Estimated as adapter work, not a rewrite. A "FlexXR UE5→UE6 migration case study" is earmarked as a future portfolio piece.

---

## Changelog

**v0.12 — Far-ray pointer**
- New §4 *Far-ray pointer*: the beam belongs to the interaction driver, not to `FXR_RayTarget` and not to the
  interactor. Near and far have to be arbitrated in one place, or a hand that can reach something also points
  past it.
- `Left Ray` / `Right Ray` (`UFXR_RayOrigin`) join the pawn — the beam is aimed by dragging a component, not
  by typing offsets. One per hand, not per interactor, and read as an offset from the tracked pose rather
  than as a world transform: a tracked hand's pose comes from joint data, so a world read would not follow
  the hand. A plain SceneComponent rather than an ArrowComponent, which would have drawn its own arrow for
  free — Epic guards those behind `WITH_EDITORONLY_DATA`, so the offset would exist in the editor and vanish
  from a packaged build.
- `Ray Visibility` (Never / On Target / Always) replaces a `Show Ray` bool, defaulting to **On Target**. An
  always-on beam says nothing and reads as a menu cursor. Affordable because far targets already hover
  through the focus subsystem, so the beam was duplicating information the highlight already carried.
- **The beam is opaque.** Translucent and additive both compiled clean and rendered nothing on the beam mesh
  while opaque drew immediately; emissive at 60 did not recover the additive version, so it was never
  exposure. The fade moved into the geometry — the beam thins to nothing instead of fading its opacity.
- Emissive intensity capped at 1: above that, unlit emissive clips after tonemapping and the cyan beam turns
  white. The same trap the highlight overlay hit at v0.8.
- Distance grab now selects the grip point **for the hand that claimed it**. It reused the near-grab
  selection, which requires the point inside the hand's grab sphere — never true across a room, so no point
  was returned and the object flew to its own origin regardless of which hand had pointed.
- `Tools/regen_fxr_materials.py` deletes and recreates rather than clearing in place:
  `delete_all_material_expressions` leaves nodes behind, so every rebuild stacked a fresh graph on the
  survivors while the material's inputs stayed wired to the stale chain. Materials grew duplicate
  parameters and edits appeared to do nothing. Safe only because every reference to these assets is now soft.

**v0.11 — Object structure**
- New §4 note: a grab moves its driven mesh and whatever is attached beneath it. One rule governs both the
  carry and the fall, because that set is exactly what physics treats as one body. A `Grab Scope` enum
  briefly existed to move the whole actor instead; it was removed. With a correctly structured object it
  is identical to driving the driven mesh, and with a badly structured one it made things worse — the
  object carried perfectly and then came apart on release, which reads as a physics bug rather than a
  hierarchy error. Matching the standard (Unity requires a Rigidbody; VRExpansion roots the mesh) and
  letting a bad hierarchy fail visibly is the better trade.
- Driven-component resolution gained a last resort: an interactable under a bare `DefaultSceneRoot`
  previously resolved to nothing and silently did nothing at all.
- A hold now parks *every* simulating body it carries, not just the driven one, and re-places them on
  release — a parked body stops following its parent by attachment, so it was snapping back to the
  pickup point the instant simulation resumed.

**v0.10 — Motion**
- Nothing pops. Highlights fade over `Highlight Fade Time`, sockets ease objects into the seat pose, and
  the ghost fades in and out. Beyond feel, the fade fixes a real flicker: on the edge of a hover the
  hand is never quite still, so a binary highlight strobes there.
- **The outline stencil packs state *and* fade**: `State + Level * 4`, two bits of state and six of
  level. A full-screen pass can read nothing per object but that byte, and it needs both.
- **Proximity ramp instead of always-on highlighting.** Interactables glow as a hand approaches and
  reach full strength only at grab range. Lighting every interactable permanently was rejected: it
  reads as a tutorial level, and in a training sim it removes the competency being tested — a trainee
  who never has to *find* the extinguisher has not been assessed on finding it. Proximity stays a
  separate channel from Hover, because Hover means "you can take this" and the far-ray suppression
  leans on that.
- Socket seat pose takes the socket's position and facing but the object's own scale; taking the
  socket's scale resized whatever docked.
- Plugin materials are generated by `Tools/regen_fxr_materials.py`, which is now their source of truth.

**v0.9 — Sockets**
- `FXR_Socket` ships. The socket's own transform is the seat pose, filtering is by actor tag rather than
  gameplay tags (no new module dependency for a short list of names), and the nearest accepting socket
  wins so adjacent mounts resolve sensibly. Driven by the interaction driver alongside the poke pass,
  so sockets never tick per object.
- Parked-physics hand-off (`NotifyParkedPhysics`): anything that parks an object kinematic — a socket
  seating it, a distance-grab flight — tells the object what its physics were beforehand. Without it a
  later grab reads the parked body, concludes it never simulated, and the object can never fall again.
- Object-side socket events deferred: they would grow `FXR_Grab`'s panel for a case most objects never use.

**v0.8 — Highlight rendering & far ray**
- The Outline stencil carries the highlight *state* (1 Hover, 2 Guidance, 3 Selected), not the style. One
  full-screen pass serves every outlined object, so it can never read a per-object colour; state is the only
  axis it can vary along, and this also lets a project outline all three states in three colours. Requires
  `r.CustomDepth=3`, and the pass attaches to the view target's camera component so no post-process volume
  or FlexXR-specific pawn is needed. Per-object colour therefore applies to Inner Blink and Sweep only.
- Outline and overlay carry separate intensities: the overlay draws unlit, so a shared multiplier above 1
  clipped every colour to white and made Highlight Color decorative.
- `FXR_RayTarget` ships: far-ray focus and selection, traced on the `FXR_Interaction` channel per ADR-002.
  Far yields to near, arbitrated in the interaction driver because only it sees both.
- Distance grab is a **checkbox on `FXR_Grab`**, not a second component and not a project policy. The Far
  Interaction Policy is dropped: the tick already says it, and a global setting that changes how one object
  behaves between projects is worse than the tick that made it so. Off by default. The flight runs on a fixed
  duration and is interpolated from elapsed time and the live hand pose, so it stays deterministic for SOP
  replay — a physics impulse toward the hand would not be (ADR-001).

**v0.7 — Presentation config**
- Highlight configuration lives only on the optional `FXR_Highlight`; the shared base panel no longer carries
  Style/Color/Sweep Direction. Defaults come from project settings, so colour has one home rather than two that
  can disagree. Sets the pattern for optional presentation components.
- Scope enum `Parent Only` → `Target Mesh`: the old name described the attach hierarchy, the new one the intent.

**v0.6 — Per-hand locomotion & climbing**
- §4 `FXR_Locomotion` rewritten: the panel describes each hand, not each mode (ADR-008). Presets removed;
  `Transition` filed under Teleport beside the Fade Duration it governs; Movement renamed Smooth Move.
- `Blink` dropped from `EFXR_TeleportTransition` — it was Fade with a hardcoded constant, not a distinct
  mechanism. A short Fade Duration is the blink comfort option.
- New `FXR_ClimbHold` (§4) and ADR-009: an ordinary interactable marks the hold, the locomotion arbiter does the
  moving, the hand is the fixed point, and gravity is scoped to the fall after letting go.
- New `IFXR_Interactor::GetNavigateValue()` — the middle-finger pinch, so hand-tracking locomotion and grabbing
  are never the same gesture.
- Aim visuals ship as plugin content wired to C++ defaults: reticle ring, arc tube, valid/invalid materials,
  vignette post-process.

**v0.4 — Locomotion**
- New module `FXR_Locomotion` (§3.5), sibling to FXR_UI, depending on FXR_Interaction.
- New components: `FXR_Locomotion` (single component with mode arbiter — ADR-005), `FXR_TeleportAnchor`,
  `FXR_TeleportBlocker`.
- New §5.8: allocation-free arc prediction, derived pool sizing, edge-triggered material swaps, room-scale
  origin teleport (ADR-006), comfort vignette per rendering tier, hand-tracking gesture parity with automatic
  teleport fallback, interaction-yielding rule.
- Locomotion events join the training event bus; `Anchors Only` validation as the locomotion hard-lock.
- Roadmap: new Phase 2.5 (2–3 wks); later phases shift.

**v0.3 — Engineering Standards Pass**
- New §15: binding C++ quality bar (Epic standard, allocation-free hot paths, plain-C++ solver + automation tests, `Build.cs`-enforced module boundaries, editor-module split, Insights instrumentation), repository discipline (commit convention, branch-per-phase PRs, per-phase release tags, README skeleton, CI-run tests), and the ADR practice.
- New §5.7 / ADR-003: no separate `IFXR_Interactable` interface — the base class virtuals are the extension contract, with a documented mechanical reversal path.
- New §16: UE6/Verse/Scene Graph position — ship on UE 5.8; portability via pure-logic core + thin exposure layer; migration outlook and future case-study note.
- Phase 1 scope now includes repo scaffolding (README, `CODING_STANDARDS.md`, CI, ADR seed).

**v0.2 — Interaction Detail Pass**
- New §4.0 `UFXR_InteractableBase`: shared detail-panel properties, enabled-by-default, `SetInteractionEnabled` + Already-Held Policy + `ForceRelease`, interactor filters, Driven Component Auto-resolution + actor-space pivot caching.
- Highlight system: automatic default highlight (Outline, bright yellow, color adjustable per interactable); optional FXR_Highlight for per-state overrides; Scope enum (Everything / Parent Only); three styles (Outline / Inner Blink / Sweep w/ direction); semantic state mapping; per-tier rendering implementations.
- FXR_Latch: component-transform-as-pivot method, Motion Axis dropdown (Local X/Y/Z), leverage note.
- FXR_GripPoint: L/R-at-different-positions pattern (two filtered points) vs. Both (auto-mirror).
- FXR_Socket: full Grab pairing flow (ghost preview, options, events).
- FXR_Use: setup walkthrough (flashlight vs. extinguisher) + rule of thumb.
- FXR_RayTarget: clarified as far-field marker; behavior-by-composition table; no ray-driven Latch; Far Interaction Policy.
- New §5.4 Detection & Focus Pipeline (registry broad phase, scored narrow phase, hysteresis, claim locking, `FXR_Interaction` trace channel) and §5.5 Collision Policy.
- New §8.1 input-device × interaction-range matrix.

**v0.1** — Initial design: vision, modules, components, solver, hand pipeline, pose retargeting, SOP graph, platforms, rendering, MR readiness, roadmap.
