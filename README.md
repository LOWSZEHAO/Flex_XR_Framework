# FlexXR

A C++ XR interaction framework for Unreal Engine 5.8 (OpenXR). Grab, two-handed grip, doors and
levers, buttons, climbing and teleport, all built as drop-in components that work with motion
controllers, tracked hands, or a desktop mouse simulator.

The idea behind it: VR training apps and VR games need the same thing underneath, which is solid
hand interaction. So FlexXR is one interaction layer, with an optional training module on top that
games simply don't load.

- **Engine:** Unreal Engine 5.8, C++ with a Blueprint-facing API
- **Platforms:** PCVR (main target), Meta Quest standalone (runs, confirmed on device)
- **Input:** motion controllers, hand tracking, desktop simulator (for testing without a headset)

<!--
## Demo

Drop GIFs or short clips into Docs/Images and link them here, e.g.

| Grab and two-handed grip | Doors, levers and valves |
|---|---|
| ![grab](Docs/Images/grab.gif) | ![latch](Docs/Images/latch.gif) |
| **Buttons (poke)** | **Climbing and teleport** |
| ![press](Docs/Images/press.gif) | ![locomotion](Docs/Images/locomotion.gif) |
-->

---

## Interactions

Every interaction is a component. Add it to an actor and it works, no collision setup needed.

**Grab (`FXR_Grab`)**
- One- and two-handed. A second hand can steer (rifles, tools, a broom) or just hold on.
- Grip points with authored hand poses, including rails so a hand can grab a pipe or handrail
  anywhere along its length.
- Built-in use events for "hold grip, pull trigger" (guns, torches) with an analog value.
- Throwing with the hand's tracked velocity, and optional distance grab with a hand ray.

**Latch (`FXR_Latch`)**
- Constrained motion for doors, levers, valves and drawers: rotational or linear, with limits.
- Optional detents (snap to N states on release) and value / state events.
- The hand is projected onto the constraint by a deterministic solver, so nothing jitters or
  explodes the way physics constraints do when a hand pulls on them.

**Press (`FXR_Press`)**
- Fingertip buttons and keypads. The cap follows the finger, clicks at an activation depth with
  hysteresis, gives a haptic tick and springs back.

**Socket (`FXR_Socket`)**
- Snap zones: bring an accepted object close, a ghost shows where it goes, let go and it seats.

**Climbing (`FXR_ClimbHold`)**
- Grab a rung, ledge or pipe and pull yourself along it. Hand over hand across holds.

**Locomotion (`FXR_Locomotion`)**
- Teleport with an arc, validation (NavMesh, surface angle, anchors only) and room-scale correct
  landing, so your head lands on the target rather than the pawn's origin.
- Smooth move, snap or smooth turn, comfort vignette, teleport anchors and no-teleport zones.
- Configured per hand, so each thumbstick's job is a details panel setting.
- Hand tracking: index pinch grabs, middle-finger pinch moves, so the two never clash.

**Presentation**
- Hover / guidance / selected highlights (outline, inner blink, sweep) with a post-process outline
  on PC and a mesh-hull outline on Quest. Objects glow as a hand approaches them.
- World-space UMG panels you can point at with the hand ray or press with a fingertip.
- A guidance arrow for objectives that are off screen.

---

## How it's built

```
FXR_Training     optional SOP / training layer (steps, modes, scoring, report panel)
     |
FXR_UI  |  FXR_Locomotion     spatial UI, highlight rendering  |  teleport, smooth move, climbing
     |
FXR_Interaction  interactables, constraint solver, detection, focus
     |
FXR_Core         rig / pawn, OpenXR, input, device capability detection, event bus
```

Some of the decisions I'm most happy with:

- **Kinematic while held, physics on release.** Held objects follow the hand through a
  deterministic solver instead of physics constraints. No jitter, same result on PC and Quest, and
  repeatable, which the training layer needs for replay. The solver is covered by automation tests.
- **Registry-based detection.** Interactables register themselves and are found by activation
  radius, not physics overlaps, so nobody has to configure collision to make something grabbable.
- **One-way module dependencies, enforced in Build.cs.** The interaction code doesn't know the
  training module exists; games ship without it.
- **Input is abstracted behind one interface** (`IFXR_Interactor`), so every interaction works the
  same with controllers, tracked hands and the desktop simulator, and the active device can change
  at runtime.
- **No per-frame allocation** in the solver, detection or hand code, and the subsystems are
  instrumented for Unreal Insights.
- **Extension by subclassing** `UFXR_InteractableBase` and overriding its virtuals.

The reasoning behind each of these is written up as decision records in [`Docs/adr/`](Docs/adr/),
and the full design is in [`Docs/FlexXR_Architecture.md`](Docs/FlexXR_Architecture.md).

---

## Training layer (in progress)

`FXR_Training` sits on top and only listens to interaction events, so the same interactables work
in a game without it.

- A procedure is two data assets: the steps and their sub-steps (the text a trainee reads), and a
  "GM" asset that says which Blueprint events to fire at each step and which objects belong to it.
- **Training** mode opens objects one step at a time and highlights them. **Exam** mode opens them
  up front and judges every action: wrong object, out of order, timeouts, critical steps and a
  mistake limit, with a score and a report at the end.
- A world-space panel shows the current step and its sub-steps, and the result.
- A small worked example lives in `Content/TrainingExample/`.

The fire-safety demo scenario that was meant to close this phase isn't built yet.

---

## Status

| Phase | | |
|---|---|---|
| 1. Core (rig, input, OpenXR) | done | `v0.1-core` |
| 2. Interaction (grab, latch, press, solver, grip points) | done, tested in headset | `v0.2-interaction` |
| 2.5 Locomotion (teleport, smooth move, turn, climbing) | done, tested in headset | `v0.3.1-locomotion` |
| 3. UI and presentation (highlight, far ray, sockets, panels) | done, tested in headset | `v0.4-ui` |
| 4. Training layer + fire-safety demo | framework done, demo not started | `v0.5-training` |
| 5. Optimisation and Quest performance study | planned (already runs on Quest) | |
| 6. Mixed reality pass and a small game demo | planned | |

Current state and what's next: [`Docs/PROJECT_STATUS.md`](Docs/PROJECT_STATUS.md).

---

## Building it

Requirements: Unreal Engine 5.8, Visual Studio 2022 with the C++ game development workload, an
OpenXR runtime (Meta Quest Link or SteamVR), and Git LFS.

```bash
git clone https://github.com/LOWSZEHAO/Flex_XR_Framework.git
cd Flex_XR_Framework
git lfs pull
```

Clone it somewhere with a short path (e.g. `C:\Dev\`). The build's intermediate files go a few
folders deep and a long path can hit Windows' 260 character limit. Use `git clone` with Git LFS
installed; the assets are stored in LFS and a plain ZIP download may only contain placeholders.

Right-click `FlexXR.uproject` -> Generate Visual Studio project files, open the solution and build
`Development Editor` (or just open the `.uproject` and let it compile the modules). The test level
is `FlexXR_Development`; the training example is `Content/TrainingExample/L_TrainingExample`.
Automation tests are under `FlexXR.*` in the Session Frontend.

---

## License

All rights reserved, see [LICENSE](LICENSE). The repository is public so it can be reviewed as
part of my portfolio; no usage licence is granted.
