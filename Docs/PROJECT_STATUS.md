# Project status

Last updated: 2026-10-24

## Where it's at

The interaction framework itself is done and has been tested in the headset: grabbing (one and two
handed), latches, buttons, sockets, climbing, teleport and smooth locomotion, highlighting, the far
ray and world-space UI panels. It also builds and runs on Quest standalone (checked on device on
2026-10-08).

The training layer (Phase 4) is working but not finished. The step / GM data assets, Training and
Exam modes, sub-steps, the judge, scoring and the in-world panel are all built and covered by
automation tests, and the example level has been run end to end in PIE. What's missing is the
fire-safety demo scenario and a proper Exam run in the headset.

| Phase | State |
|---|---|
| 1. Core: rig, input, OpenXR, device detection | Done (`v0.1-core`) |
| 2. Interaction: grab, latch, press, solver, grip points | Done, tested in headset (`v0.2-interaction`) |
| 2.5 Locomotion: teleport, smooth move, turn, climbing | Done, tested in headset (`v0.3.1-locomotion`) |
| 3. UI and presentation: highlight, far ray, sockets, panels | Done, tested in headset (`v0.4-ui`) |
| 4. Training layer and fire-safety demo | Framework done, demo not built (`v0.5-training`) |
| 5. Optimisation and Quest performance study | Not started |
| 6. Mixed reality pass and game demo | Not started |

## Next

1. A short interaction demo level and clips for the README (grab, latch, press, climb, teleport).
2. Blueprint events on all interactables (On Grabbed / On Released exist on `FXR_Grab`; the other
   components need the same treatment).
3. The fire-safety demo, built on the training layer.
4. Phase 5: an Unreal Insights performance pass on Quest. The subsystems are already instrumented.

## Things I decided not to do

- **Ghost-hand guidance** (a translucent hand showing the grip). Dropped; highlighting plus the
  guidance arrow cover it.
- **Candidate-selection hysteresis** (a sticky bonus for the current best target). Described in the
  architecture doc but never needed in practice; the input thresholds already have hysteresis.
- **A `Grab Scope` setting and a project-wide distance-grab toggle.** Both were built and then
  removed: with a correctly set up object they changed nothing, and distance grab is a per-object
  checkbox instead.
- **The old step graph** (branching, parallel steps, Guided/Practice/Exam). Replaced by the simpler
  Training/Exam model in ADR-011 after running it showed the world could get ahead of the procedure.

## More detail

- [`FlexXR_Architecture.md`](FlexXR_Architecture.md) - the design, with a changelog of decisions
- [`adr/`](adr/) - architecture decision records
- [`DEV_NOTES.md`](DEV_NOTES.md) - engine gotchas I ran into, and known issues
