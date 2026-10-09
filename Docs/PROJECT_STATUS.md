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

On branch `phase-4-training`, pushed, no PR yet. Builds clean. **None of it has been run yet** - no graph has
been authored and no session played, in a headset or in PIE. Phase 3's two deferred items landed
here, as planned, so a real consumer would define them.

**The SOP runtime (ADR-004)**
- `FFXR_StepRunner` - the judge. No UObject, no world, no tick group: fed events and a delta time, it
  answers with outcomes, which is what makes a run reproducible and testable without an engine
  running. It deliberately cannot reach the world, because a procedure a trainee cannot fail measures
  nothing.
- `UFXR_StepGraph` - the authoring DataAsset, compiled down to the runner's step array. Authoring and
  runtime are separate by ADR-004, so a CSV import or a node editor later compiles to the same
  contract.
- `UFXR_TrainingSession` - the part that *can* reach the world. Subscribes to the interaction event
  bus, drives the Guidance highlight on the open step's target, and applies the opt-in hard-lock
  interlock for steps that must gate.
- `FFXR_SessionReport` - per-step timings, mistakes, hints and a weighted score. Built before cleanup
  so it describes the run and not the teardown, and safe to call mid-session for a live panel.

**The spatial UI kit**
- `UFXR_Panel` - world-space UMG surface. Sized in centimetres, optional face-the-player, fades on
  the framework's one fade duration, ray-targetable with no collision setup.
- `UFXR_WidgetPointer` - one per hand on the pawn. Slate's own pointer, fed from the rig's cached far
  hit and from the fingertip inside poke range, and yielding to interaction like locomotion does.
- `UFXR_TrainingPanel` - the validation panel. A `UUserWidget` base to reparent a Widget Blueprint
  to; it reads the session and never drives it. It drives its own widgets through
  `meta = (BindWidgetOptional)`, so a panel needs no event graph and no property bindings: name a
  widget `InstructionLabel` or `ProgressFill` and it is found and kept current.
- `WBP_FXR_TrainingPanel` at `/FlexXR/UI` - the default panel, generated. **Its look was rejected
  and is being rebuilt** (§9.1), and the asset on this branch renders every glyph as a missing-glyph
  box (§8). The geometry under it is sound, so what lands is a restyle, not a rewrite.
- `UI_Design/panel.html` - **the approved design** (2026-10-09), and the spec the rebuild follows.
  Eight boards at 900 x 870 px, which is 90 x 87 cm at component scale 0.1 - so **1 px is 1 mm** and
  every number in the CSS can be typed straight into UMG. The page draws them full size and shrinks
  them with a CSS transform only, never by changing type sizes, which would throw away the point of
  checking legibility at true proportions. It supersedes the 60 cm / 20-px-per-cm scale the generated
  asset was built to. The eight: a start board, Explanation idle / playing / read, Procedure normal /
  mistake, and the report passed / failed.
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

- **Nothing in §4 has been executed.** The SOP runtime, the spatial UI kit and the validation panel
  all build clean and the logic has been read, but no graph has been authored, no session played and
  no panel put in a level. Treat every behaviour described in §4 as designed, not demonstrated.
- **The generated panel renders no readable text at all.** Every glyph comes out as a missing-glyph
  box showing its Unicode block name. `UFXR_PanelBuilder::MakeLabel` takes its font from
  `FCoreStyle::GetDefaultFontStyle`, which returns the **Slate editor style** font rather than a UMG
  font object, so the `FSlateFontInfo` has no typeface to resolve against. The geometry underneath is
  correct - only the font is wrong. Left unpatched on purpose, so the font object and the real
  typeface land in one pass with the approved design (§9.1).
- **The generated panel has been verified as structure, not as a picture.** All eleven widgets are
  present with the right classes and the asset is parented to `UFXR_TrainingPanel`, checked by
  loading it back. Past the font above, nobody has looked at it: not in the designer, not in PIE, not
  in a headset.
- **The `BindWidgetOptional` pointers are inferred to resolve, not observed to.** UMG matches them by
  name, the names were verified to match, and the properties compile - but an optional binding that
  fails to resolve is silent by design, so a typo would look like a panel that never updates.
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

1. **Build the approved design into the panel.** `UI_Design/panel.html` is signed off and is the
   spec. Four pieces, one pass: import Montserrat 500/600/800 to `/FlexXR/UI/Fonts` and add the
   folder to `DirectoriesToAlwaysCook`; hand `MakeLabel` that font object, which is also the fix for
   §8's missing-glyph bug; rewrite `UFXR_PanelBuilder` to the eight boards at 900 x 870 with Rounded
   Box brushes and per-corner `FVector4` radii; and set `bDrawAtDesiredSize = false` with
   `DrawSize = (900, 870)` on the panel component (§7).
   Two things that will bite otherwise. The design's colours are **sRGB and `FLinearColor` is
   linear** - convert, or the panel comes out washed out and too bright
   (`FLinearColor::FromSRGBColor` does it in code). And Montserrat as it ships in the source design
   carries **only weights 500, 600 and 800** - there is no Regular, so any 450 or 650 in the spec
   resolves to 500 and 600.
2. **Exam mode auto-fails on a step timeout.** Decided 2026-10-09, not built. Today
   `FFXR_StepRunner::Tick` escalates a hint on each elapsed multiple of `Timeout Seconds` and logs a
   `Timeout` mistake - but `ShouldShowHints()` already returns false in Exam, so an Exam timeout
   stacks mistakes silently and shows nothing, which is the worst of both. In Exam the first timeout
   on an open step should end the run, failed, and show the report. Guided and Practice spend a
   timeout on help; Exam spends it on the verdict, which is what makes the mode mean anything.
   It needs one type change. `FFXR_SessionReport::bReachedEnd` conflates a failure with someone
   calling `StopSession`, so it becomes `EFXR_SessionOutcome { Completed, Failed, Stopped }` plus a
   `FailedStepId`, which is what lets the failed board name the step it ran out of time on.
   `OnSessionFinished` already carries the report, so this needs no new hook.
   **A step with `Timeout Seconds = 0` never times out, and so can never auto-fail.** Choosing Exam
   does not by itself make a graph time-limited - the steps that should be timed need timeouts set.
   Kept explicit rather than letting Exam impose a default nobody chose.
   A timeout is the only thing specified as fatal. Whether a **wrong action** should also end an Exam
   run is open: it was raised and not answered, and today it scores as a mistake and the run goes on.
3. **Author a graph and run it.** Nothing in §4 has executed. Tick Expose to Training and set an
   Interaction Id on a handful of interactables, make a `UFXR_StepGraph`, drop a session on a manager
   actor, and watch it in PIE before putting a headset on.
4. **Put the panel in the level.** The rebuilt `WBP_FXR_TrainingPanel` inside an `FXR_Panel` at 90 cm
   wide, and one `FXR_WidgetPointer` per hand on the pawn. This is the first real exercise of the
   kit: the ray path, the poke path, the beam over UI and the panel's own type sizes are all
   unverified (§8).
5. **The fire-safety demo.** Only after the above. Light that level for the device from the start
   rather than with the desktop template sky (§7).

Done: Phase 3 closed, merged and tagged `v0.4-ui`; the Quest smoke test (§6); the guidance arrow,
confirmed in-headset 2026-10-09; the SOP runtime, the spatial UI kit and the validation panel (§4);
the panel design, approved 2026-10-09.

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
