# FlexXR - Project Status & Handoff

**Last updated:** 2026-10-09 · **Last active development:** 2026-10-09
**Current branch:** `phase-4-training` · **Architecture doc version:** 0.15

This document is the single place to find *where the project actually is*. The architecture
document says what FlexXR is and why; this says what is built, what is half-built, what was
deliberately not built, and what to do next.

If you are picking this project up cold, read in this order:

| # | Document | What it gives you |
|---|---|---|
| 1 | **This file** | Current state, next actions, known gaps |
| 2 | `Docs/FlexXR_Architecture.md` | The design and the reasoning. Changelog at the bottom is a history of every decision |
| 3 | `Docs/adr/` | Ten decision records. **Do not relitigate these without writing a new ADR** |
| 4 | `CODING_STANDARDS.md` | Binding: Epic style, allocation-free hot paths, module rules, commit format |

---

## 1. The one-paragraph version

FlexXR is a C++ XR interaction framework for Unreal Engine 5.8 (OpenXR), built so that **one
interaction layer serves two markets**: SOP-driven industrial training and high-fidelity VR games.
The architectural bet that makes that possible is **determinism** - kinematic-while-held,
physics-on-release (ADR-001) - because SOP replay and scoring need an interaction that reproduces
exactly, and games need hand feel that doesn't jitter. Training sits *on top of* interaction and
only observes it; games simply never load it. That separation is enforced mechanically in
`Build.cs`, not by convention.

---

## 2. Phase status

| Phase | State | Tag |
|---|---|---|
| 1 - `FXR_Core` | ✅ Complete | `v0.1-core` |
| 2 - Interaction core | ✅ Complete, **fully verified in-headset** | `v0.2-interaction` |
| 2.5 - `FXR_Locomotion` | ✅ Complete, **fully verified in-headset** | `v0.3.1-locomotion` |
| 3 - `FXR_UI` + presentation | ✅ Complete, verified in-headset | `v0.4-ui` |
| **4 - `FXR_Training` + SOP demo** | 🔶 **In progress - current work** | - |
| 5 - Optimization + Quest standalone | ⬜ Not started | - |
| 6 - MR pass + game demo | ⬜ Not started | - |

As of 2026-10-08 the framework also **runs on Quest standalone** - built, packaged, installed and
confirmed in-headset (§6). That is the Phase 5 target platform reached early, which de-risks it:
Phase 5 is now optimisation on a device that already runs, not a port.

Full roadmap with time estimates: architecture doc §13.

---

## 3. Phase 3 - what is built

All on branch `phase-3-ui-presentation`, unmerged. Builds clean. Verified in-headset as it landed,
except where noted in §4.

**Focus and highlight**
- `UFXR_FocusSubsystem` - hover/selected, precedence Selected > Guidance > Hover.
- `UFXR_HighlightSubsystem` - three styles (Outline, Inner Blink, Sweep) bound to *semantic states*,
  never to appearances. Training says "highlight the pin, Guidance state" and never learns what
  Guidance looks like.
- **Outline has two implementations** behind one API (`Highlight Tier`: Auto / Post Process / Mesh
  Hull). Post Process reads a packed stencil (`State + Level * 4`) in one full-screen pass. Mesh Hull
  is an inverted hull for mobile - the mesh drawn again, pushed along its normals, front faces masked
  by `TwoSidedSign`. `Auto` resolves on **feature level**, so the editor's mobile preview shows the
  Quest path without a Quest.
- **Proximity ramp** - interactables glow as a hand approaches, full strength only at grab range.
  Deliberately *not* always-on: that reads as a tutorial level, and in a training sim it removes the
  competency being tested.
- Everything fades. Nothing pops.

**Far interaction**
- `FXR_RayTarget` - the "you can point at me" marker. The beam itself belongs to the driver.
- Far-ray pointer with per-hand `Left Ray` / `Right Ray` origin components on the pawn - the beam is
  aimed by dragging a component, not by typing offsets.
- `Ray Visibility`: Never / **On Target** (default) / Always. An always-on beam says nothing and
  reads as a menu cursor.
- **Distance grab** - a checkbox on `FXR_Grab`, not a second component.

**Sockets**
- `FXR_Socket` - seat pose is the socket's own transform, actor-tag filtering, nearest accepting
  socket wins, ghost preview with Off / On Approach / Always modes.

**Guidance**
- `UFXR_GuidanceArrow` (`FXR_UI`) - a world-space arrow for "the thing you need isn't here, it is
  that way". The one guidance problem highlighting can't solve, since a highlight only reaches what
  is already on screen. Hides itself once the target is within `Hide Within Angle` of where the
  player is already looking, or nearer than `Arrive Radius`.
- The visual is swappable. `Arrow Mesh` and `Arrow Material` take your own assets, and
  `Mesh Forward Axis` says which way that mesh points so it is not assumed to be the engine
  cone's +Z. `Visual Class` takes a Blueprint instead, for anything a single static mesh
  cannot be, and keeps its own scale while the fade multiplies it. Placement is `Distance`,
  `Height Offset` and `Lateral Offset`, all anchored on camera yaw so the arrow holds still
  while the player looks up and down.

**Motion design**
- `UFXR_MotionSettings` in **`FXR_Core`** - one `Fade Duration` governing highlights, the socket
  ghost, the beam and the arrow, plus `FFXR_Motion::EaseFade` for one curve. The spec is settings the
  components read, not a document they are meant to honour. See architecture §5.9.

**Extensibility**
- `UFXR_ScoringPolicy` - the one seam in narrow-phase detection. See ADR-010 for what is open, what
  is closed, and why.

**Tooling**
- `Tools/regen_fxr_materials.py` is the source of truth for every plugin material. **Run it from the
  editor console, never unattended** - see §7.
- `Tools/make_fxr_training_panel.py` generates the default validation panel (§4). Unlike the material
  tool it *is* safe unattended, because it rebuilds in place and never deletes - the two reasons the
  material tool half-succeeds headlessly. The layout lives in `UFXR_PanelBuilder`, not in the script.

---

## 4. Phase 4 - what is built

On branch `phase-4-training`, pushed, no PR yet. Builds clean. **The Guided flow has run end to end
in VR preview** (2026-10-16), on the worked example below: grabs advanced the steps, the manager's
own events fired, and the panel followed. Exam mode, the Start screen, the report's buttons, poke and
Quest have not been exercised yet (§8). Phase 3's two deferred items landed here, as planned, so a
real consumer would define them.

**The worked example - `Content/TrainingExample/`.** Written because the split between steps, GM,
session and panel was the part that confused a first-time author, and a scene that shows each piece in
its own place answers that faster than any paragraph. Rewired to the GM on 2026-10-16.
`DA_TrainingExample` is the Steps asset, "Load the weapon": step 1 *Pick up the magazine* is a whole
step, step 2 *Handle the weapon* has two sub-steps, *Pick up the weapon* and *Put the weapon down*.
`DA_TrainingExample_GM` wires it: step 1 calls *Enable Grab* on the prop tagged `Magazine`; step 2 owns
the `Weapon` as its Step Object, and its first sub-step calls *Enable Grab* on the prop tagged `Weapon`.
Both events are marked Auto Start In Exam. `BP_TrainingExampleProp` (a copy of `BP_Interactable1`, so
the dev map's props are untouched) starts with its grab **locked**, has *Enable Grab* / *Disable Grab*
for the GM to call, and two per-copy ids, *Complete On Grab* and *Complete On Release*, which its On
Grabbed / On Released hand to `Complete Step` when set. `L_TrainingExample` places three of them -
Magazine (cube), Weapon (cylinder, completes on grab *and* on release) and Decoy (cone, wired to
nothing) - each with an Actor Tag the GM targets. `BP_TrainingExampleManager` is now only the session,
debug prints for steps, sub-steps and mistakes, and the lamp, which turns on when the run finishes;
the old Branch-on-Step-Id chain is gone, because the GM does that job. `BP_TrainingExamplePanel` is an
`FXR_Panel` and nothing else. `BP_FXR_Pawn` carries one `FXR_WidgetPointer` per hand.

Checked in PIE without hands, by firing the props' own On Grabbed / On Released handlers: Training opens
only the Magazine at the start, out-of-turn calls are ignored, the Magazine opens step 2 and its first
sub-step unlocks the Weapon, the panel turns to step 2's page and ticks its sub-steps off, and the
release ends the run - Training complete, the ring and tick, the lamp on. In Exam both Auto Start events
fire at the start and the Decoy stays locked. Not checked: real grabs in a headset, and Exam's judging,
which needs a real grab to put an event on the bus.

**The SOP runtime (ADR-004, revised by ADR-011)**
- `UFXR_TrainingSteps` - the procedure as the trainee reads it: ids, names, instructions, in order. A step
  may have **sub-steps** (id + line of text), done strictly in order.
- `UFXR_TrainingGM` - what happens at each step: custom events to fire on Blueprints in the level (picked
  from a dropdown of each Blueprint's own events), the step's objects (Step Objects - Interaction Ids, not Step Ids), a timeout
  and a Critical flag. Points at the Steps asset and picks its step ids from it. A step's sub-steps
  sit under it in the GM, each with its own events, objects, timeout and Critical, so each part opens
  its own objects; the step's own row applies across all of them.
- **Complete Step** - a global Blueprint node the project wires to whatever event means "done". An
  optional Step Id makes it advance only while that step is open; while a step with sub-steps is open,
  it ticks off the open sub-step (by its Sub Step Id), and the step finishes with the last.
- `FFXR_StepRunner` - the judge. No UObject, no world, no tick group. Runs the steps in order, judges every
  exposed object against them, and in Exam fails the run on a critical step, a timeout or Max Mistakes.
  It never changes the world and never decides a step is done. It sees sub-steps as ordinary entries in a
  flat list; the session folds them back into steps. Covered by automation tests (`FlexXR.Training.*`).
- `UFXR_TrainingSession` - the part that reaches the world. Fires the GM's events as steps arrive (and, in
  Exam, the ones marked Auto Start In Exam when the run begins), highlights the step's objects in Training,
  and checks every event against the level before the run starts.
- `FFXR_SessionReport` - per-step timings, mistakes, the outcome and why, and a score that is earned by
  finishing and only shown in Exam.

**The spatial UI kit**
- `UFXR_Panel` - world-space UMG surface. Sized in centimetres, optional face-the-player, fades on
  the framework's one fade duration, ray-targetable with no collision setup.
- `UFXR_WidgetPointer` - one per hand on the pawn. Slate's own pointer, fed from the rig's cached far
  hit and from the fingertip inside poke range, and yielding to interaction like locomotion does.
- `UFXR_TrainingPanel` - the panel. A `UUserWidget` base to reparent a Widget Blueprint to; it
  reads the session and never computes anything about the procedure. It drives its own widgets
  through `meta = (BindWidgetOptional)`, so a panel needs no event graph and no property bindings:
  name a widget `StatusLabel` or `ScoreRing` and it is found and kept current. **Four screens, one
  asset** - Start, Explanation, Procedure and Complete are the children of one Widget Switcher, and
  **the screen is derived rather than set**: a finished run shows the report, a running one shows
  the procedure, anything else shows Start. An explanation is the one exception, because it is the
  one thing the runner has no opinion about.
  **One page per step:** the heading is the open step and the list is its sub-steps, numbered from one
  (a step without any shows one row, its instruction); the meter above has a segment per step. Both are
  built in C++ at runtime, because their length is the procedure's and nothing else can know it.
  It reaches back into the session in exactly one place, and switchably: `Buttons Drive Session`
  lets Begin and Try again call `StartSession`, which is what makes a dropped-in panel work on its
  own. The request delegates fire either way.
- `UI_Design/panel.html` - **the approved design** (2026-10-09) and the spec the panel is built to.
  Eight boards at 900 x 870 px, which is 90 x 87 cm at component scale 0.1 - so **1 px is 1 mm** and
  every number in the CSS types straight into UMG. If the page and the code disagree, the page is
  right.
- `FXR_PanelStyle.h` - the design system as code: every colour, size, type style and brush in one
  public header, read by both the runtime panel and the editor builder. A token defined twice is a
  token that will disagree with itself, and a runtime-built row that does not match the list it sits
  in is the most visible kind of wrong.
- `WBP_FXR_TrainingPanel` at `/FlexXR/UI` - the generated panel, 119 widgets. The builder proves its
  own work: it checks every `BindWidgetOptional` against the tree by name **and class**, the way UMG
  does, and logs the result rather than leaving it to be inferred. Last run: *39 resolve, 5 declared
  names unused by this layout, 119 widgets*. Those five are deliberate - `InstructionLabel`,
  `ProgressLabel`, `ProgressFill`, `SummaryLabel` and `CompletionCard` exist for simpler layouts
  somebody else builds.
- `F_Montserrat` at `/FlexXR/UI/Fonts` - a composite font over three `UFontFace` assets, Medium,
  SemiBold and ExtraBold, with the face data embedded. Three faces and no italics because that is
  what the design uses and an unused face is still a cooked megabyte. Sources and the SIL Open Font
  License live in `Plugins/FlexXR/Resources/Fonts`; they are needed only to reimport.
- **No buttons, sliders or keypads.** Those are Slate's and work inside a panel unchanged - see the
  architecture doc's v0.15 entry for why that reverses §3.3's original line.

**New module: `FXR_TrainingEditor`** - the second editor-only module. Holds `UFXR_PanelBuilder`,
which writes the panel above. It has to be C++: `UWidgetBlueprint::WidgetTree` and
`UWidgetTree::RootWidget` are bare `UPROPERTY()`s and so are not exported to script, `UWidgetTree`
has no `UFUNCTION` at all, and `ConstructWidget` is a template. Python can create the asset and then
do nothing with it, which is why `Tools/make_fxr_training_panel.py` is a three-line wrapper.

**Still to come:** the fire-safety demo scenario. Deliberately last; the framework gets finished
first.

Phase 3 leftovers: the guidance arrow is done and confirmed in-headset (§3). Ghost-hand guidance was
**deliberately dropped** - see §5.

---

## 5. Deliberate omissions - do not "fix" these

Things that look missing but are decisions. Reopening them needs a reason, and in two cases a new ADR.

**Ghost-hand guidance (dropped 2026-09-01).** A translucent hand demonstrating the grip was considered
and rejected. Grip points already make hand placement automatic - the framework snaps the hand to the
point in the right pose - so a ghost hand would demonstrate something that happens anyway. It also
repeats the always-on-highlight mistake one level deeper: a trainee *shown* the exact grip has not
been assessed on knowing it. Revisit only if the Phase 4 demo surfaces a real case, most likely a
two-handed safety control where both hands must be placed specifically.

**Four things the approved design asks for that the panel does not do.** Each was checked, not
assumed, and each has a one-line reason:
- *Frosted glass.* `backdrop-filter: blur(30px)` has no equivalent for a world-space widget - there
  is no scene colour behind the quad to blur. The translucent fill and the bright inner ring carry
  the look without it.
- *The check mark on a finished step.* Montserrat contains no U+2713. That is read off the font's
  own cmap table rather than guessed, so a completed step is its number on a green disc. A
  missing-glyph box would be worse than no tick. `U+25B6` (play) and `U+203A` (chevron) **are**
  present in all three faces and are used.
- *The list's bottom fade.* A gradient needs a texture or a material; the list card's ring already
  gives the list an edge.
- *Line clamping.* UMG has no `line-clamp`. A closed row is one line that ellipses and the open row
  wraps freely, so a very long instruction grows its row rather than being cut at four lines.
- *The score ring's start.* The page starts the arc at 12 o'clock; `URadialSlider`'s zero is at 6,
  and it was kept there by choice on 2026-10-16 - it reads well. `AngularOffset = 180` in the
  builder is the change if that is ever revisited.

**Candidate-selection hysteresis.** Architecture §5.4 describes a sticky-best score bonus. It is
**documented but never implemented**. All hysteresis in the codebase is on input thresholds (grab
value, use value, press depth). Highlight flicker between two adjacent objects is therefore a real
pre-existing gap, not a regression - and the `UFXR_ScoringPolicy` seam is now its natural home.

**`Grab Scope` enum.** Was built, then removed. With a correctly structured object it was identical
to driving the driven mesh; with a badly structured one it made things worse - the object carried
perfectly and then came apart on release, which reads as a physics bug rather than a hierarchy error.

**Far Interaction Policy.** A project-wide toggle for distance grab. Dropped: the per-object checkbox
already says it, and a global setting that silently changes how a specific object behaves between
projects is worse than the one tick that made it so.

**Extensibility, three closed areas.** ADR-010 records what is open and what is closed. Styles and
tiers are closed enums *on purpose* - that's what keeps the state→style map coherent and training's
vocabulary fixed. If a fourth style is ever genuinely needed, migrate styles to **data assets**; do
not simply widen the enum. Locomotion modes are closed by ADR-005 and need a superseding ADR.

---

## 6. Quest smoke test - PASSED 2026-10-08

The standing rule is that a Quest build closes every phase from 2 onward. It had never been run,
because the project carried no Android configuration at all. It has now run end to end on a Quest 3
(Android 14 / SDK 34) and **interaction was confirmed working and smooth in-headset**.

Getting there took two fixes, both invisible on PC and neither producing an error. Both are recorded
as gotchas in §7 and are the clearest argument yet for the device-build-per-phase rule:

1. **Stereo was collapsed** - an identical image in both eyes. Multi-view was fine
   (`bMobileMultiViewEnabled = 1`); the project was on the mobile **deferred** shading path, whose
   lighting pass never sets `RenderTargets.MultiViewCount`. Now `r.Mobile.ShadingPath=0`.
2. **None of FlexXR's own content shipped** - every material and mesh is addressed by a hardcoded
   `FSoftObjectPath` in C++, which leaves the cooker no dependency to follow. All 11 assets were
   absent from the first APK, so the Mesh Hull highlight tier could not have worked on device either.
   Now named in `DirectoriesToAlwaysCook`.

**Known cosmetic difference, not a fault:** the scene reads as sunset on device and midday on PCVR.
That is the template sky stack meeting Android's platform defaults - see §7. It is a property of the
test level's lighting, not of the framework.

### Installing the build - the APK alone isn't enough

The package is split: a ~124 MB APK and an ~84 MB OBB holding the cooked content. `adb install` by
itself gives you an app with no content. The full sequence (paths relative to `Binaries/Android`):

```
adb install -r FlexXR-arm64.apk
adb shell rm -r /sdcard/UnrealGame/FlexXR
adb shell rm -r /sdcard/Android/obb/com.LowSzeHao.FlexXR
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/Android/UnrealAndroidFileTool/win-x64/UnrealAndroidFileTool.exe" ^
    -p com.LowSzeHao.FlexXR -k 951BD0864FB0127E76F8C7B2C85A6672 push main.1.com.LowSzeHao.FlexXR.obb "^mainobb"
```

UE generates `Install_FlexXR-arm64.bat` to do exactly this, but it invokes the file tool as
`.\win-x64\UnrealAndroidFileTool.exe` - **a folder that isn't deployed next to the APK** - so the bat
fails at the OBB step. Call the tool by absolute path as above. The `-k` key is per-package and is
printed in the generated bat.

Rebuild and repackage with:

```
RunUAT.bat -ScriptsForProject="G:\Flex_XR_Framework\FlexXR.uproject" BuildCookRun ^
  -project="G:\Flex_XR_Framework\FlexXR.uproject" -nop4 -utf8output -nocompileeditor -skipbuildeditor ^
  -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -package -pak -iostore -compressed
```

About 6–7 minutes from clean config. Verify afterwards without a headset: `UnrealPak <utoc> -List`
should list all 11 `/FlexXR/` assets, and extracting `*Engine.ini` from the pak shows the renderer
settings that actually shipped.

Under Git Bash, prefix adb shell commands with `MSYS_NO_PATHCONV=1` or a device path like `/sdcard/...`
is rewritten into a Windows path. `adb pull` needs a **Windows** destination path.

**A headless launch proves nothing about rendering.** With the headset off a head the proximity sensor
suspends the app before the renderer initialises - the log stops around 120 lines, well short of any
stereo or asset-load information. `adb shell am broadcast -a com.oculus.vrpowermanagement.prox_close`
makes the device behave as if worn, if a full runtime log is needed without wearing it.

**Watch for on device:** `Highlight Tier = Auto` resolves to **Mesh Hull** on Quest, not the
post-process outline used on PC. If highlights look wrong on device but right on PC, start there.

---

## 7. Hard-won gotchas

Each of these cost real time. They are not obvious from the code.

- **`r.CustomDepth=3`** (Enabled *with* Stencil) is required or the Post Process outline draws
  nothing. At `1` the stencil goes nowhere readable. Mesh Hull needs neither custom depth nor a
  post-process chain.
- **Materials referenced from C++ must be `TSoftObjectPtr`.** A hard `ConstructorHelpers` reference
  roots the asset, and `MaterialEditingLibrary` asserts `!IsRooted()` - the regen script takes the
  editor down with it.
- **`regen_fxr_materials.py` must run from the editor console, not unattended.** Deleting an existing
  asset needs a confirmation a commandlet can't give, so `create_asset` then refuses the name and
  every already-existing material is silently skipped - while the commandlet reports success.
- **A component built at runtime defaults to `Static` mobility** and silently discards every transform
  update. Set `Movable` (and usually `SetAbsolute`). This cost a long hunt for an invisible laser.
- **Translucent and additive materials rendered nothing** on plugin meshes where opaque drew
  immediately, and raising emissive to 60 didn't recover them. The beam, the arrow and the hull are
  all opaque, and they fade **geometrically** (thinning or growing) rather than by opacity.
- **Unlit emissive above 1.0 clips after tonemapping** - a cyan beam comes out white. Keep intensity
  at or below 1 unless you want the clip.
- **Parked physics:** anything that parks an object kinematic must call
  `UFXR_Grab::NotifyParkedPhysics(bWasSimulating)`. Otherwise the next grab reads the parked body,
  records "never simulated", and the object can never fall again after release. This bit twice.
- **Never `CreateDefaultSubobject` in a *component* constructor** - it breaks the Blueprint SCS and
  hard-crashes the editor on load.
- **A module that directly references another module's `UCLASS` must list it in `Build.cs`.** A
  transitive public dependency gives header access and still fails to link.
- **Grab never writes scale.** A grip point attached beneath a mesh inherits its scale, so
  `GripPose.GetRelativeTransform(Held)` divides the scale by itself and yields a unit-scale offset
  that would overwrite the authored size. Handled centrally in `SetHeldTransform`.
- **Activation radius is absolute cm; rail length scales with the object.** The radius is hand
  ergonomics - a hand is a hand - while a rail is geometry. Different questions, different answers.
- **Mobile deferred shading silently destroys stereo.** `r.Mobile.ShadingPath` must be `0` (forward)
  for Quest. `MobileDeferredShadingPass` builds its own PSO render-target info and never sets
  `RenderTargets.MultiViewCount`, while every other mobile pass does - so with mobile multi-view the
  lighting pass resolves one view and **both eyes receive the same image**. Nothing warns. The engine
  logs `bMobileMultiViewEnabled = 1` and the HMD agrees, so the usual multi-view checks all look
  healthy. Forward also admits MSAA (deferred can't - `RendererSettings.cpp` quietly forces mobile AA
  back to `None`) and turns on the tonemap subpass, which wants Mobile HDR **on**, multi-view on and
  deferred off. That is why `r.MobileHDR=True` is correct here and must not be "fixed".
- **Plugin content referenced only by `FSoftObjectPath` in C++ is never cooked.** A soft path built
  from a string literal in a constructor leaves no asset-registry dependency edge, so the cooker
  never learns the package exists. The first Quest build shipped with **none** of FlexXR's materials
  or meshes. They can't be hard references (see the `MaterialEditingLibrary` note above), so the
  folders are named in `DirectoriesToAlwaysCook` in `DefaultGame.ini`. Add any new plugin content
  folder there. Symptom on device: `LogStreaming: Warning: SkipPackage: /FlexXR/... does not exist on
  disk or in the loader`.
- **PC and Quest will never agree on the UE5 default sky, and it isn't a bug.** `FlexXR_Development`
  is lit by the template stack - SkyAtmosphere, VolumetricCloud, a real-time-capture SkyLight,
  directional light and height fog. On Android the engine disables volumetric clouds outright
  (`r.VolumetricCloud.Support=0`, `BaseAndroidEngine.ini`) and clamps SkyAtmosphere to the cheap path
  at **every** quality level - `[EffectsQuality@0]` through `@3` in `AndroidScalability.ini` all set
  `FastSkyLUT=1`, a 96×50 LUT, 1–8 samples, aerial perspective at one sample per slice. Undersampled
  scattering underestimates transmittance, so the sky comes out warmer and dimmer. Because the
  SkyLight captures in real time, it then feeds that sky back as ambient on **every surface**, which
  is why the whole scene reads as sunset on device while PCVR looks like midday. Raising scalability
  can't fix it; the Android values are identical at all four levels.
  The fix is to stop lighting a Quest target from a live desktop sky: capture the SkyLight to a fixed
  cubemap, drop the cloud component (it only ever renders on PC, so it actively misleads), and bake
  the demo level. Do **not** raise the Android sky cvars to chase parity - that spends GPU on a
  90 fps target for a sky the training demo doesn't care about.
- **Rebuilding a Widget Blueprint in place crashes unless the old widgets are renamed away.**
  `UWidgetTree::RemoveWidget` detaches a widget from the tree and leaves the object outered to the
  tree under its own name. Constructing a new `MistakeCard` when the old one was a different class
  is then a fatal error in `UObject` creation - *"Cannot replace existing object of a different
  class"* - not a tidy failure. Walk `GetAllWidgets` and `Rename` each one into the transient
  package first.
- **Empty `WidgetVariableNameToGuidMap` when you rebuild a tree in code**, or the widget compiler
  ensures once per widget: *"Widget [X] was added but did not get a GUID"* - 125 of them, each with
  a symbolised callstack, which turned an 84 ms build into 14 seconds and looked like a hang.
  `FWidgetBlueprintCompilerContext::ValidateAndFixUpVariableGuids` populates the map
  **deterministically** when it is empty and self-heals noisily when it is stale, and the
  determinism is the point: those GUIDs are how an external reference to a widget survives a
  rename. Resetting the map is the whole fix.
- **UMG font sizes are points, not pixels.** Slate draws a glyph at (96 / 72) x Size
  (`FontCache.cpp`, RenderDPI = 96), so a size typed in from a CSS design draws a third too big:
  "24" is 32 px. Every line on the panel was oversized until `FXR_PanelStyle::Type` started
  converting, which is what the report's missing rows actually were. **Line height has a different
  base too**: CSS `line-height` multiplies the font size, UMG's Line Height Percentage multiplies the
  font's natural line - 1.219 em for Montserrat, read from its hhea table - so a CSS value copied
  straight across spaces every paragraph about a fifth too loose. Keep design tokens in the design's
  pixels and convert both in one place.
- **`UImage::SetDesiredSizeOverride` stores nothing.** It forwards to the live Slate widget and
  that is all, so on an image that has not been constructed into Slate yet - every image built in
  code, in a tool or at runtime - it is a silent no-op, and the image falls back to its brush's
  default **32 x 32**. Every "3 px" rule on the panel came out 32 px tall, which is what pushed the
  report's last three rows off the bottom, and the progress meter would have been a 32 px slab in
  PIE. Size an image through its brush's `ImageSize`: the brush is a property, and a property is
  saved. `FXR_PanelStyle::Band` takes the thickness for exactly this reason.
- **`UWidget` has a member called `Slot`**, so a local named `Slot` inside any widget method is
  C4458 and therefore a build error here. Name slot locals something else.
- **`bDrawAtDesiredSize` must be off for a world panel with a Canvas Panel root**, with `DrawSize`
  set explicitly. The designer's Width/Height is `DesignTimeSize`, which lives inside
  `#if WITH_EDITORONLY_DATA` (`UserWidget.h:1528`) and never reaches runtime, and
  `SConstraintCanvas::ComputeDesiredSize` (`SConstraintCanvas.cpp:356`) returns the max of its
  children's slot offsets rather than the canvas size - which `CurrentDrawSize` then becomes
  (`WidgetComponent.cpp:1432`). A widget authored at 900 x 870 otherwise renders into a ~500 x 500
  target and is magnified to fit. Every other quality decision about a panel is downstream of it.
- **Compiling a Widget Blueprint destroys its live instance in the editor world and does not rebuild
  it.** The placed `WidgetComponent` goes blank and stays blank; save, then reload the level.
  Iterating without knowing this makes it look like the edit did nothing. Widget components are also
  **one-sided** - from behind, the panel is invisible, which reads as a broken widget when the camera
  moves rather than as the camera being on the wrong side.
- **Editor file locks:** the editor holds `Content/*.uasset` open. `git checkout` of a content file
  fails with `unable to unlink ... Invalid argument` while it is running, and a branch switch can
  abort half-applied. Close the editor first. **Never `git clean -fd` in this repo.**

---

## 8. Known flaws, unfixed

Small and recorded so they are not rediscovered as mysteries.

- **Only the Guided path has run.** The worked example ran end to end in VR preview on 2026-10-16 and
  the owner confirmed it working. It auto-starts, so it never showed the Start screen or pressed
  Begin; it was never failed, so Exam's timeout ending the run is still designed rather than seen;
  and nobody has pressed Done or Try again, poked the panel with a finger, or run any of it on Quest.
  Running the example taught two things reading the code had not: the procedure heading was only ever
  filled by the designer preview, and one wrong grab was counted as two mistakes. Both are fixed.
- **The panel has been seen, in the designer and in VR preview, and the type was a third too big
  until it was.** The fixes are in §7. What still has to be judged by eye is in the next three.
- **Row heights are left to content rather than enforced.** The design gives a closed row a 60 px
  minimum and an open one 96 px. The numeral disc is 42 px and the padding brings a closed row to
  about 62, and an open row's wrapped text exceeds 96 on its own, so no `SizeBox` was spent on it -
  but a one-word instruction in a large graph is the case that would show a short row.
- **The designer preview is sample content, not the run.** `NativePreConstruct` fills whichever
  screen `ScreenSwitcher` is on with the design page's own sample procedure - five steps, the open
  one third - and hides the bar on the screens that never carry it, so the designer shows what a
  headset will. Before this it showed a mostly blank layout with the bar on every screen, which
  made the report look 107 px shorter than it is. Tick **Preview Failed Run** on the root widget to
  see the taller of the two reports. None of it runs outside the editor.
- **The scroll rail is the engine's, not the design's.** `UScrollBox` draws its own bar; the design
  draws a 4 px pill. The list is inset 18 px to keep clear of it, which is the part that mattered.
- **The pointer beam over a panel is unverified, and so is the hover highlight.** `FXR_Panel` adds an
  `FXR_RayTarget` at BeginPlay, because the rig only lights its beam for something that will answer
  and a laser you cannot see is one you cannot aim. That also makes the panel an interactable, so the
  highlight system may put a hover outline on it. If that looks wrong in a headset, clear
  `Pointer Beam` on the panel - the pointer itself does not need it.
- **A curved panel cannot be poked.** `UFXR_Panel::TracePoke` refuses Cylinder geometry mode and says
  so; ray pointing still works on one. Flattening a fingertip onto a cylinder is a different
  calculation and there was no case for it yet.
- **The validation panel's step number is "completed + 1", not an index.** A procedure that allows
  steps in any order has no single current step, so a fan-out reads as fewer steps than are open. It
  is the right number for a trainee reading how far through they are and the wrong one for anything
  that needs the graph position.
- **`UFXR_ScoringPolicy` is marked `Blueprintable` but `ScoreCandidate` is a plain C++ virtual**, not
  a `UFUNCTION`. A Blueprint subclass compiles and its override is never called - worse than not
  being Blueprintable at all. Should either drop `Blueprintable` (matching ADR-010, which says
  extension is C++) or be made properly overridable.
- **The guidance arrow requires manual wiring per step.** Today something must fetch the pawn, get
  the component, and call `Point To Component`. A design exists but wasn't built: have the arrow
  *follow* `UFXR_HighlightSubsystem::SetGuidance` automatically, so one call drives both the glow and
  the arrow, and nothing touches the pawn. Dependency direction allows it (`FXR_UI` → `FXR_Interaction`).
- **The plugin isn't self-contained for packaging.** FlexXR's materials and meshes ship only because
  *this project* names them in `DirectoriesToAlwaysCook`. Anyone who drops the plugin into their own
  project gets a build with no ray, no reticle, no arc, no vignette and no outline hull, and no error
  explaining why. That is a poor first impression for a framework meant to be consumed. The proper fix
  keeps it inside the plugin: subscribe to `UE::Cook::FDelegates::ModifyCook` from a module's
  `StartupModule`, or give the plugin an asset-manager rule, so enabling it is enough. Worth doing
  before any 1.0 claim.
- **Black tearing/ghosting at the outer edge of the right eye on Quest.** Reported 2026-10-08.
  **Open, and accepted as minor** - the owner's call after investigation stalled; it is small, and
  standalone is otherwise smooth. Not reproducible on PCVR.
  Characterisation, which is the useful part: it appears only where a **high-contrast silhouette**
  crosses that region - the ridge line where the landscape meets the sky or the flat ground. Looking
  at uniform sky or uniform ground, it is invisible. "Needs an edge to be visible" is the signature
  of a shading-rate or reconstruction artifact rather than of geometry or lighting.
  **Ruled out, each confirmed from a device log rather than inferred:**
  - *Foveated rendering.* The project asks for it off and was being overridden by the engine's
    `Android_OpenXR` device profile; that's now corrected (see `Config/DefaultDeviceProfiles.ini`),
    the log confirms `xr.VRS.DynamicFoveation 0 → 0` and `r.Vulkan.AllowFDMOffset 0 → 0`, and the
    artifact survives. Foveation is **not** the cause.
  - *`r.Mobile.Oculus.ForceSymmetric`*, set by the `Meta_Quest_3` profile. It is a dummy: the log
    says *"Creating unregistered Device Profile CVar"* / *"deferred - dummy variable created"*,
    because Meta's own plugin isn't installed. It does nothing.
  **Still untested, in the order worth trying:**
  1. *Image-based VRS itself.* `r.Vulkan.VRSFormat=3` is still pushed by the device profile and the
     RHI still reports *"Image-based Variable Rate Shading supported via EXTFragmentDensityMap
     extension. Selected VRS tile size 16 by 16 pixels per VRS image texel"* even with foveation
     off. A density map attached but no longer meaningfully populated would shade the periphery at
     the wrong rate - invisible in flat colour, visible across an edge. Test
     `r.Vulkan.VRSFormat=0,r.VRS.Support=0`.
  2. *The 4x MSAA resolve under mobile multi-view.* Note there's **no device baseline without it**:
     MSAA was enabled in the same session as the first successful device run, and the pre-existing
     Sep 6 build was never run on a headset. Test `r.Mobile.AntiAliasing=0`.
  3. *The render target being marginally smaller than the compositor's distortion samples.*
  **How to A/B on device without repackaging** (this harness is the reusable part): push a one-line
  command-line override. It **replaces** the entire command line, so it must carry `-project`, and
  `ForceDPCVars=` applies at command-line priority, above the device profile:
  ```
  adb push UECommandLine.txt /sdcard/Android/data/com.LowSzeHao.FlexXR/files/UnrealGame/FlexXR/UECommandLine.txt
  ```
  ```
  -project="../../../FlexXR/FlexXR.uproject" -ForceDPCVars=r.Vulkan.VRSFormat=0,r.VRS.Support=0
  ```
  With `UseExternalFilesDir=True` that external-files path is the one the engine reads; `adb install`
  doesn't remove it, but the generated install bat does. Delete the device's `FlexXR.log` before a
  run so the log is unambiguous, confirm with `Using override commandline file:` in it, and remember
  a headless launch proves nothing - the proximity sensor suspends the app before the renderer runs.
- **PC and Quest brightness won't match, and level editing can't fix it.** Attempted once and
  reverted (`5713778`). The cause is the renderer and post-process stack, not the level: mobile
  forward plus the mobile tonemapper, and the `Meta_Quest_3` profile dropping post-process, GI,
  reflection and effects quality to 1, shadows to 2, and disabling `r.LocalExposure` - while the
  project configures local exposure contrast at 0.8, so that tuning only ever applies on PC. The sun
  is *not* the culprit: it is an atmosphere light, but `GetTransmittanceAtGroundLevel` is pure CPU
  maths with no LUT, so its colour is identical on both platforms. If the device needs to be
  brighter, tune it for the device in `Config/Android/AndroidEngine.ini` and leave PC alone. Chasing
  parity is the wrong goal.

---

## 9. Next actions, in order

1. **Interaction events.** `Complete Step` is wired to an interactable's own events. `FXR_Grab` now has
   On Grabbed and On Released - added ahead of the rest because a "pick it up" step had nothing else to
   hook to. The owner is preparing the full list for every component; those two fold into it.
2. **Play the worked example in the headset** (§4), in Training and then Exam: real grabs, Exam judging
   every exposed object, failing on a Critical step, a timeout and Max Mistakes; the GM's three dropdowns.
   **Open question:** an object that should be live only in Exam - the example's Decoy - cannot be
   expressed, because an Auto Start In Exam event also fires in Training when its step arrives. The Decoy
   is therefore locked in both modes. Candidate fix: an *Exam only* option on GM events.
3. **The fire-safety demo.** Phase 4's actual deliverable, built on the GM.
4. **Close the phase.** Quest smoke test, PR into `main`, tag.

Done: Phase 3 closed, merged and tagged `v0.4-ui`; the Quest smoke test (§6); the guidance arrow,
confirmed in-headset 2026-10-09; the SOP runtime, the spatial UI kit and the panel (§4); the panel
design, approved 2026-10-09; the approved design built, the font bug fixed and Exam auto-fail
implemented, 2026-10-11; the worked example, and the Guided flow run end to end in VR preview,
2026-10-16; the GM redesign (ADR-011), built 2026-10-16; sub-steps, built and unit-tested 2026-10-16; the
worked example rewired to the GM and run in PIE, 2026-10-16.

Phase 4 and 5 matter most for the portfolio: a training demo built entirely on the framework, and a
performance case study, are what prove the thesis.

---

## 10. Repo facts worth knowing

- **Repo root is `G:\Flex_XR_Framework\`** (flattened 2026-07-11). An empty leftover `FlexXR\` folder
  from the old nesting may still be pinned as a working directory by tooling - run git and build
  commands against the parent.
- `origin` = `https://github.com/LOWSZEHAO/Flex_XR_Framework.git`. Branch per phase, PR into `main`,
  tag at the end of each phase.
- `.uasset` / `.umap` / media are on **Git LFS**.
- Test content (`BP_Table`, `BP_Interactable`) is scaffolding for trying features in the level, not
  framework content.
