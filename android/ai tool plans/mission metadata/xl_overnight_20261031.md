# XL overnight continuation

Authorized from October 30 evening through October 31 noon Pacific local time
(19:00 UTC; daylight time remains in effect). Reserve 11:30 onward for final
validation, emulator restoration and handoff. Continue bounded 1-2 hour chunks
on stacked branches, publishing validated work to start CI. No merges requested.

## Starting checkpoint

Branch 88, `xl_more_88_skybox_rendering`, starts at branch 87 commit
3249cb52f1dae4785f51c82d96878c043547cc10. PRs #238 and #239 are open;
#238 passed all 14 checks. At 22:29 Pacific #239 has passed 12 checks with
native/headed replay pending/running. Reuse earned source/native/Android tests.
The local evening handoff edits and unrelated manifest/Shearbase/W5AM work
remain preserved. No subagents or autonomous navigation. Logs and reads bounded;
process checks 30-60 seconds apart, never restart on observation timeout.

## First bounded chunk

Implement separate skybox face rendering and qualify it against pinned XL
2576a98b5de310e3ac7c9777df2677d1627b8bae. Begin with actual source contracts for
visibility, list order, automap gating, full brightness and depth writes. Reuse
the existing source comparison and headed rendering owners. Integrate before
effects depth capture; verify ordinary geometry occludes skybox, brightness
restores, and the separate pass does not mark the mine explored. Use derived
geometry for controlled tests without enabling normal skybox admission.

Earn complete weapon-entry physics, skybox object rendering/coronas and original
authored resource/save/context lifetime before admitting skyboxes normally.
Smoke and outdoors properties remain separate Pumo hub blockers. Pumo is not
qualified playable. Advance those requirements as bounded chunks finish, not
by bypassing admission or replacing authored behavior with a screenshot.

## Active implementation checkpoint

Branch 88 exists. New skybox_render.h provides source face-facing, automap and
wall-mesh inclusion policies. Existing xl_effect_reference extracts actual
FaceIsVisible, RenderSkyBox and CWall::IsInvisible bodies with hashes. Windows
and Linux pass 25725 visibility cases, 3944 second-normal distinctions, eight
automap/depth gates and 18432 wall inclusion cases. Source FADE_LEVELS is 31,
not Redux's 34. The initial extraction matched a forward declaration; fixed.

Runtime textured-face drawing is connected before xl_transparency_flush. It
compiles source-ordered geometry, forces full brightness, and restores depth and
window state. Normal admission remains closed. Native integration passes with
474 loaded faces across mixed/all-skybox bindings. Scoped formatting passes.
First Android run failed the new automap isolation assertion: ordinary OGL
passes and visibility-only update paths still wrote Automap_visited. Those are
now guarded too. Protected restoration matched all 28 files. Native recompiles
and Android rebuild passes. Android retry passed 31/31 in 53.189 seconds;
log android-render-automap-fixed.log. All local build/test sessions are terminal.
Reuse this validation unless subsequent changes justify another run.

The Android control uses a fixed pose, temporary red/green RGBA pixels, altered
native static light and automap isolation, then the existing secret-save/GPU
owner. No authored bytes are changed. Rendering depth/occlusion, object passes,
colored boundaries, full weapon entry and original skybox lifecycle remain gates.

Next: finish the headed controls, including distinguishing depth/occlusion.
Source gameio/createmesh.cpp also includes colored-segment boundaries; these
are not yet connected to the new textured skybox face pass.
Source split-13 visibility anchor is expanded m_vertices[3], which maps to
corner 1; split-02 uses corner 0, quads use SideCenter. Compile using existing
source-backed geometry, implement the pass before xl_transparency_flush, and
add headed integration controls for brightness/depth/state/automap isolation.

## Emulator and receipts

Current AVD DxxOvernight_1 on port 5554 temporarily uses host GPU, launcher PID 37712. Original emulator processes 53120/43828 were verified terminal before
restart. Restore swiftshader_indirect at closeout (original args saved in
temp/xl-evening-20261030/emulator-original.txt). Use Hidden windows and verify
termination before switching. Retain new receipts under
temp/xl-skybox-20261031; reuse existing builds and fixtures where valid.

## 23:49 validation checkpoint

Source object traversal now passes 18432 lists and 55296 ordered calls on both
Windows and Linux. Android controls earned 46 native object visits, 20 outside
the ordinary list, and exact gameplay visibility order including a temporary
robot outside that list. Actual depth rejection, depth writes and restoration
pass. The mixed foreground test exposed a diagnostic-state bug: the depth
witness disabled shim texturing without restoring the native enable cache.
An extra ordinary frame distinguished it (white first frame, green second;
mixed frame exactly matched green). That diagnostic run passed 31/31 in
54.635 seconds, but the extra frame and temporary logs have now been removed.
The depth control restores texturing consistently. Scoped formatting passed;
final build session 13340 is running, log mixed-state-build.log. After terminal
success, install and run the existing protected secret owner again, recording
android-mixed-state.log. Do not publish until that passes. Source/native/catalog
passes remain reusable. All prior build/test handles are terminal.

Final mixed-state build and Android owner passed: 31/31 in 54.035 seconds.
All 28 protected hashes match. Actual mixed foreground and discovery preserved;
46 object visits, 20 outside ordinary list; both visibility orders include the
separate robot. All local handles terminal. Next: update skybox evidence, review
scoped diff, recheck parent PR 239, commit/push branch 88 and open draft PR.

## Published branch 88 and next bounded chunk

Branch 88 is published at 59d14782bcb58687cd888ff0ca0892f57d800901 as draft
PR https://github.com/larsonm-personal/dxx-redux/pull/241 above open PR 239.
Parent 239 was fully green at publication. No merges performed. Current branch
is xl_more_89_skybox_boundaries, initially identical to published 88.

Next bounded chunk: source-backed colored boundary selection and corona
exclusions, extending existing owners. In pinned gameio/createmesh.cpp lines
1622-1634, mesh inclusion uses differing IsColoredSeg values OR wall type;
InitColoredFace precedes InitTexturedFace when both apply. IsColoredSeg in
render/renderlib.cpp is controlled by mission colored-segment configuration and
FogType (singleplayer); skybox membership alone does not imply a colored face.
Investigate existing fog/water consumers before adding duplicate drawing.
Retain normal admission gates while qualifying authored behavior.

## Branch 89 corona checkpoint, 00:02 Pacific

Implemented SetupCoronaFaces segment exclusions in the actual emitter API:
skybox function 10 and decoded outdoors properties return a cleared emitter.
Legacy function 16 remains distinct from modern function 16; modern property
bits are decoded through the existing inventory policy. No admission changed.

Hash-pinned actual SetupCoronaFaces extraction passes 131072 combinations on
Windows and Linux, covering face conditions, light queries and ordered corona
assignments. Native and Android loaded controls pass 54 combinations using a
real eligible emitter and verify rejected output is cleared. Android existing
owner passes 31/31 in 53.841 seconds; all 28 protected hashes match. Scoped
formatting passes. Logs: corona-quality.log, corona-runtime-quality.log,
corona-linux.log, native-runtime/controls.log, android-corona.log. Windows source
receipt remains visibility-source.log. All current build/test handles terminal.

This chunk is not committed yet. Continue colored boundary investigation before
publication, or publish the corona correction as its own bounded chunk if the
boundary work requires a separate design. Reuse validation unless affected code
changes. Both catalogs were already passed for existing owners; no new script.

Pinned RenderSkyBoxFace explicitly calls LoadFaceBitmaps then RenderFace even
for untextured colored faces. LoadFaceBitmaps returns for negative nBaseTex or
!bTextured; RenderFace clears bmBot for !bTextured. Thus mesh-only inclusion is
not enough to settle drawing behavior. Next inspect its untextured draw setup
and existing fog compositing to determine the precise missing consumer. Do not
add a speculative tint or silently omit an authored boundary.

## Corona publication and next bounded chunk

Branch 89 published at a948fb156406b4920fbacf8ae63e88f14bf3f937 as draft PR
https://github.com/larsonm-personal/dxx-redux/pull/242 above open PR 241. No merges.
Branch 90, xl_more_90_skybox_lifecycle, starts at that tip. Next establish complete
weapon entry through actual physics and loaded skybox save/resource lifetime,
reusing existing controls. Colored-boundary draw behavior remains an explicit
unresolved admission gate; do not infer colors from mesh inclusion flags.

Source follow-up: ogl_fastrender.cpp SetRenderStates uses cached bmBot and may
skip SetupTMU for a null bitmap when history is null. SetupTMU itself calls
pBm->Bind; CBitmap::Bind is not a null-safe no-op. InitFace zeroes the structure,
InitColoredFace sets nBaseTex=-1/bTransparent=1/bSegColor, and InitTexturedFace
later overrides texture/transparent fields when a wall exists. BeginRenderFaces
resets bitmap history. Therefore colored-only drawing needs a source state/order
control, not a simple assumed constant tint. Preserve this finding for the
remaining boundary chunk while progressing independently testable lifetime work.

## Branch 90 full-entry control in progress

New skybox_collision_test.h control drives do_physics_sim from ordinary segment
1 toward skybox 15 after native FVI selects the intervening wall. Scoped format,
native build and existing native integration owner pass; log controls.log has
XL skybox full entry. Current assertions establish destination segment and retained
lifetime only. Do not call the gate complete yet: next compare post-entry position
and source ProcessOffset/FixPosition behavior. Native physics assigns hit_pnt
and can return early after find_object_seg without relinking; merely observing
shot.segnum is insufficient. Pinned new physics likewise preserves wall hit vPoint
before ProcessOffset, so inspect that consumer before changing position semantics.
No Android build/run for this latest test yet. All local handles terminal.

## Enclosing skybox entry checkpoint

Replaced the weak distant-cell fixture with a temporary enclosing cell using
eight isolated vertices, preserving/restoring native segment data, vertex slots,
Num_vertices and Highest_vertex_index. Source ProcessOffset preserves the wall
hit point; FindSegment does not relink on success. The new native control proves
actual entry 1 -> 2, exact wall-hit position, geometric containment, retained
lifetime, and continued movement inside the skybox on a second physics frame.
The full native integration owner passes. Initial fixture rejection exposed a
missing Highest_vertex_index update; corrected before this passing run.

Scoped formatting and native build passed. Android build is live session 78362,
log entry-android-build.log. After terminal success install and run the existing
protected secret owner, console log android-full-entry.log. Do not rerun source
or catalog checks for this runtime-test-only change. No production physics code
changed. Original authored skybox save/resource lifetime remains a separate gate.

Final enclosing-entry Android build and owner passed: 31/31 in 54.820 seconds,
log android-full-entry.log. Full entry 1 -> 2 receipt present, including exact
hit position, containment and second-frame movement assertions. All 28 protected
hashes match. All process handles terminal. This validates derived geometry;
next qualify original authored skybox resource/save lifetime, without bypassing
normal admission. Branch 90 is not yet committed or pushed.

## Original authored topology audit, 00:22 Pacific

Bounded projection of pumo-admission-audit.json identifies all skybox levels:
pm-hub 295 (properties 0 and 60), tulev2 one (0), pmlev1 one (4),
pmlev2 one (0), pmlev3 one (4). Only the hub has differing fog-type boundaries:
all six are segment 154, function 10, properties 60. Each side has child=-1 and
wall=-1, with textures 19,31,29,33,15,30. Therefore these six faces take the source
InitTexturedFace path after InitColoredFace; their colored-only flags are
superseded. Do not treat unknown colored-only drawing as proof this actual hub
uses that path. Conversely do not treat this audit as qualifying outdoors/fog.

Next authored-lifetime control can use a clearly labelled derived copy retaining
original skybox geometry/textures while isolating unsupported smoke/properties,
then test actual resource/save restoration. It must preserve original package
bytes and normal hub rejection, and must not claim original hub qualification.
The final original hub still needs smoke and segment-154 property behavior.
Source-only colored-boundary support remains a separate general feature gap.

## Published physics checkpoint and admission/resource sequence

Branch 90 published at 8bb0ab18b01205ef8bb537b68334d50d58494c37 as draft PR
https://github.com/larsonm-personal/dxx-redux/pull/243 above open PR 242. Current
branch is xl_more_91_skybox_resources. No production admission edits yet.

The next resource/save proof must reload authored skybox metadata through normal
admission. Manually rebinding a stripped fixture after restore would not prove
automatic owner restoration. Prepare candidate admission for supported textured
skyboxes together with the derived authored-data fixture and save/GPU owner;
do not publish admission until those checks pass. Keep explicit rejection for
unsupported colored-only skybox boundaries and independent smoke/properties.
Normal original hub rejection remains required. Do not claim campaign admission.

Portable admission is android/XL_compat/admission.cpp runtime_blockers. Source
skybox membership is currently rejected as segment_function_v21=10. Both original
Pumo rejection owners currently assert that feature string; any future admission
change must instead verify the actual remaining authored smoke/property blockers,
not loosen the checks to an unspecified failure. Existing geometry renderer binds
and clears at preview load/clear; source metadata must survive actual disk restore.

## Branch 91 candidate and authored fixture (unpublished)

Implemented shared skybox_side_textured selection in portable skybox_render.h;
runtime binder now reuses it. Source IsTriggerTarget checks GEOTRIGGERS only,
matching level.triggers. Extended actual-source wall comparisons exercise loaded
side selection, colored-only connected boundaries and external textured faces.
Windows source comparisons pass. Added skybox_colored_only_boundary.

Local admission candidate admits modern function 10 but explicitly rejects
colored-only fog boundaries; runtime graphics validation requires gl_initialized
through xl_skybox_renderer_ready. Original Pumo rejection controls now require
segment_properties (its actual remaining outdoors gate), not the obsolete generic
function gate. Windows inventory/translation/admission suite passes. These
changes are NOT committed/published and must not ship before real resource/save
qualification. Linux and Android validation remain outstanding.

New target xl_skybox_fixture creates pskybox.hog/mn2 from original Pumo. Exactly
two hub bytes change: segment154 properties60 ->44 (outdoors cleared), object1
particle subtype0 ->3 (explicit fire substitute). Geometry, walls, object indices
and all other member bytes retained. Builder verifies offsets against decoded
metadata, exact two-byte diff, no remaining portable blockers. Build/generation
passed, original archive hash unchanged. Output temp/xl-skybox-20261031/authored-fixture:
HOG 69f56e52a45fb16f061290c115709086c416b88cecd1037e26d81c95ad8e89b7
MN2 e2562e9ddd913ca7eeae81f0e9bd9323fb58380950c6411a60b1b462c73babf5
Logs authored-fixture-build.log / authored-fixture.log; admission-test.log;
face-selection-quality.log / admission-candidate-quality.log / authored-fixture-quality.log.

Next extend existing test_xl_pog_runtime owner with a SkyboxFixture mode and
explicit protected pskybox mission/mod paths. Reuse original mods/pmines assets
under mods/pskybox, not an unprotected write to mods/pmines. Add owned JSON and
probe operations: normal start1, paused renderer/membership/resource capture,
real disk save/restore, context recreation, stock transition/clear. Do not use
manual rebind after restore. Re-run both automation catalogs after script changes.
Current native build launched this turn; inspect its live handle before rerun.

Native candidate build session 24128 completed successfully; all local handles terminal.

## Authored Android lifecycle owner and first failures (unpublished)

Added SkyboxFixture mode to test_xl_pog_runtime.ps1, protected pskybox paths,
owned 52-step test_xl_skybox_authored.jsonc, and skybox_lifecycle_probe.h. Uses
normal mission start, POG byte/GPU verification, fixed view154/0, real disk save,
disturbed pose/energy, restore, EGL recreation, stock transition and restore.
Membership verification requires295 authored skyboxes; no manual rebind.
Original mod assets now tar from their own root into mods/$mission, avoiding any
unprotected mods/pmines write during the derived test. This affects existing
Pumo mode too but keeps its contents/paths equivalent.

Formatting, both catalogs (491 support scripts), Linux source/inventory, native
candidate integration and initial Android build passed. First Android attempt
failed selection: fixture title exceeded mission's25-character bound. Shortened
title to Derived Pumo skybox. NEW descriptor SHA256:
fe8d90d8efd9efe2d270fdf6f90b1854171c15b8701ff6c9435df7bc94f45c7c
HOG unchanged69f56e52a45fb16f061290c115709086c416b88cecd1037e26d81c95ad8e89b7.

Retry selected mission but synchronous StartNewGame reached Error: Invalid station
type10 in fuelcen.c (live log00:46:12). It is still running session77304 under the
480-second owner timeout; do NOT install/restart before it terminates/restores.
Console log android-authored-title-fixed.log, protected output
temp/xl-skybox-authored-android. First failure restored files through finally.

Fixed probe.cpp classic shadow mapping: modern skybox10 -> ordinary0, retained
XL source stays10. Added regression inspecting translated station byte (previous
metadata-only assertion missed this bug). Both scoped formatters terminal0.
Inventory test session24524 is running; rebuild APK session from latest tool
is running, log shadow-fixed-android-build.log. Poll these handles at30-60s.
Do not publish admission yet. Need complete Android pass, meaningful authored
pixel-difference witness (current capture/restore comparison alone may be weak),
updated Linux/native validation for probe.cpp, final scoped format/diff check.

## Branch 91 final lifecycle pass, 01:05 Pacific

All local handles terminal. Corrected shadow mapping passed Windows/Linux
inventory contracts and rebuilt native integration. Native integration passed.
Android shadow-fixed run loaded and passed disk restore, then failed context
pixel comparison. Introspection proved resumed physics moved camera from
(-432.33,0,432.33) to(-431.7167,0.3570,431.7155). Corrected control uses distance8,
reestablishes identical fixed pose after EGL recreation, and requires a different
textured-side view to differ from capture. No runtime renderer patch for this.

Final APK build fixed-view-android-build.log and protected owner
android-authored-fixed-view.log pass: 54/54,120.725seconds. Six original POG
bitmaps,25165824 pixel bytes,all six rendered;295skybox memberships; distinct-view
witness; disk restore; EGL recreation; stock cleanup and restore from stock.
All28protected file hashes match. No manual skybox rebind after restore.

Both catalogs passed with491support scripts before the final same-owner step
changes; no new registration change. Scoped formatting passed including final
fixed-view edits. Native/Windows/Linux translation/source evidence is current.
PR241 and242 fully green;243 had only two replay jobs running at01:00, no failures.
Next: final scoped diff review/check, parent243state check, commit/push branch91
and draftPR if acceptable. Preserve unrelated files and the evening handoff.
Candidate admits textured modern skyboxes; original hub remains blocked by smoke
and outdoors, colored-only boundaries explicitly rejected. Original campaign
playability remains unqualified. Then address smoke/outdoors as bounded chunks.

## Branch 91 publication and smoke scope, 01:13 Pacific

Published df7f76a7612555a374946f4347e6f156a0b0bc32 as draft PR244:
https://github.com/larsonm-personal/dxx-redux/pull/244
Actual base is xl_more_90_skybox_lifecycle (PR243), still open; two replay
checks in progress at08:09UTC, all other required completed checks successful.
The first PR-create attempt used an incorrect base name and created nothing;
corrected attempt succeeded. No merge performed. Unrelated manifest hash intact.

Current branch xl_more_92_smoke_particles. Next bounded chunk: qualify the
stationary authored smoke subtype0 using the existing source particle owner,
then extend runtime/resources and Android lifecycle only once comparisons pass.
Do not infer smoke support from the fire atlas's smoke cells: authored smoke
uses image1/smoke.tga (8-cell dimension), while fire uses image5/smokingfire.tga.
Pinned effects/particles.cpp has distinct smoke Create, UpdateColor, decay,
growth and SetupColor branches. Existing preparation tests cover smoke emitter
definitions, but FireParticle creation explicitly rejects image !=5.
Original smoke.tga already exists under temp/xl-particles-20261030/original-assets/
textures/d2x-xl, so reuse it rather than downloading or substituting artwork.
All local processes remain terminal; no new build launched. Next inspect active
source Create and existing extraction adapter, then implement smoke comparisons.

## Branch92 smoke color checkpoint

Added particle_smoke.h with authored fade1 setup and linear lifetime decay.
Existing source owner now compares actual pinned CParticle::UpdateDecay and
SetupColor across lifetime thresholds, alpha thresholds, brightness, bright
particles and RNG seeds. Windows source owner passed, including all existing
fire/geometry/emitter/world/skybox comparisons. Scoped formatting passed after
sandbox denied installed clang-format and authorized escalation succeeded.
Receipts: temp/xl-skybox-20261031/smoke-color-{quality,build,source}.log.
No admission/runtime wiring changed yet. Next extend creation and update using
actual source adapter, then atlas/resource and normal lifecycle consumers.
PR244 Windows/Linux XL contracts passed at latest check; remaining CI pending,
no observed failures. All local handles terminal. Changes uncommitted on92.

## Branch92 smoke creation checkpoint, 01:19 Pacific

Windows actual-source comparisons pass: 7680 color/decay cases,2640 sequential
color transitions,2304 complete creation cases (point/face, blow-up on/off,
authored/default color, three brightness inputs, seeds, parameter values).
Existing fire/source suite remains passing. Creation compares RNG consumption,
position/drift/direction, lifetime, radius/size, atlas indices and color states.
Source adapter now supplies the pinned smoke 8-frame image alongside fire2.

A real mismatch exposed source Create mutating fBrightness to1-fBrightness for
nonbright expanding smoke, then retaining that argument for final SetupColor.
Corrected implementation passes exact comparisons; no tolerance added.
SMOKE_LIGHTING is0 in pinned include/particles.h. Creation scope uses quality2,
default alpha setting, authored nonpositive alpha. Admission remains unchanged.
New files: particle_smoke.h, tests/reference_smoke_creation.h. Existing source
owner files reference_particle_geometry.cpp/reference_effects.cpp extended.
All changes uncommitted on92. Formatter terminal0; all local handles terminal.
Next: qualify smoke movement/update and atlas geometry using actual source,
then wire resource/emitter/runtime and Android lifecycle before admission.
Receipts smoke-color-source.log/build.log and smoke-creation-quality.log under
temp/xl-skybox-20261031. Preserve other user files and original emulator restore
requirement at tranche closeout.

## Branch92 smoke movement checkpoint

Windows AND Linux actual-source owner passes all smoke cases:7680color,
2640transitions,2304creation,22464updates. Existing fire2304creation/41472update
and emitter/world/skybox controls pass after sharing common motion lookup and
collision logic through advance_particle_motion in particle_fire.h. Fire keeps
constant direction drag; smoke supplies source decay-based fixed drag. Smoke
updates run color first, honor negative-life return semantics, then movement,
lookup/bounce, lifetime and source radius/alpha growth. No admission change.

Scoped formatter passed; gitdiffcheck passed. All processes terminal, including
WSL session11263. Linux receipts smoke-linux-build.log/smoke-linux-source.log;
Windows smoke-color-build.log/smoke-color-source.log; smoke-motion-quality.log.
All under temp/xl-skybox-20261031. Branch92 changes still uncommitted.

Next concrete task: smoke quad and animation in existing particle_render owner.
Active source Setup (non-TRANSFORM_PARTICLE_VERTICES) uses smoke scale1/sqrt(decay)
when blowup, otherwise1; for decay>.9 both multiply sqrt((1-decay)/.1). Uses
rendered RGBA and8x8atlas, normal smoke blending (must qualify buffer/backend,
not reuse fire's multiply smoke variant). Continue normal resource/emitter/runtime
and Android lifecycle qualification before allowing original hub smoke.

## Branch92 smoke geometry and animation checkpoint

Windows actual-source owner passes1440smoke billboards and1680animation
transitions, along with all existing fire checks. Added shared billboard helper
with scale/atlas/color inputs; fire wrapper preserves previous parameters.
Smoke uses8x8atlas, authored RGBA, source expansion scale and positive lifetime.
Smoke animation advances atlas every animation tick and rotation every second
tick, matching actual CParticle::Render and newly extracted UpdateTexCoord.
Render adapter supplies smoke image and source bWobbleBubbles option.

ParticleBlend::alpha added for future smoke wiring; no runtime caller emits it
yet, and current backend still needs explicit alpha handling. Source buffer
selects m_bEmissive (0 for authored smoke) as blend mode, uses soft shader mode0;
fire smoke variant instead uses multiply. Do not allow subtype0 until resource,
buffer/backend, emitter and Android lifecycle are qualified. Next extend those
existing owners. Updated Windows receipts smoke-color-source/build.log and
smoke-render-quality.log; Linux simulation evidence predates renderer edits and
needs refresh after renderer/backend work. All local handles terminal, changes
uncommitted. No APK build/install since branch91.

## Branch92 smoke alpha backend checkpoint

Added actual source alpha branch to particle_soft_shader.h, alpha GL factors in
fixed and soft ParticleTexture backends, and blendMode0 dispatch. Existing GPU
owners now include authored RGBA tint/alpha, source-derived blend factors and
source glare shader comparison. No alternate renderer or integration owner.
Linux EGL GPU owner PASS:129soft depth/blend comparisons,38hard atlas/blend/depth
cases, existing buffer/mixed transparency and real context recreation controls.
All114RGBA cases/2120414pixels passed with max analytical error1. Linux source
owner also refreshed and passed simulation, smoke billboards/animation and fire.
Scoped mixed formatter passed; gitdiffcheck passed. All processes terminal.
Receipts smoke-gpu-build.log/smoke-gpu.log/smoke-linux-source.log and
smoke-blend-quality.log under temp/xl-skybox-20261031.

Next: resource representation currently holds one fire image; prepare_particle_image
rejects other subtypes and uses smokingfire2x2. Need owned smoke8x8 alongside fire,
with same mission/base provenance, lookup precedence and retained pixels. Preserve
mixed fire/smoke scenes and save/context lifetimes. Then integrate smoke emitter
and extend original-Pumo/derived owner. Admission remains fire-only,92uncommitted.

## Branch92 smoke resource ownership checkpoint

prepare_particle_image now selects authored subtype3fire or0smoke, retains exact
mission/base lookup ownership policy, and validates the respective2x2/8x8atlas.
Prepared level resources own separate shared fire/smoke images; getter defaults
to fire for existing callers and accepts0for future smoke runtime consumers.
Existing resource owner runs full precedence/malformed/missing/case/shadow/base
installation/retained-pixel checks for both subtypes and mixed-image separation.

Native engine rebuilt and integration passed (session72903terminal0). Scoped
formatter and diffcheck passed. All local handles terminal. Receipts same native
runtime owner under temp/xl-skybox-20261031/native-runtime and native-runtime.log;
smoke-resource-quality.log. No admission change, no Android build yet on92.
PR244 latest checks no failures; contracts/builds/tooling/LAN green, ASan/replays
pending. Next implement smoke emitter using source ring scheduling, then wire
runtime scene/resource selection, snapshots and original hub smoke fixture.

## Branch92 smoke emitter checkpoint

Shared AuthoredParticleEmitter<Smoke> ring now aliases FireEmitter/SmokeEmitter.
Same scheduling and retirement order; subtype-specific creation/update and smoke
brightness from authored definition/100. Existing source emitter owner extended
for smoke, compares every live particle including color/target/rendered state.
Windows source owner PASS:2880smoke frames,24drained rings,6zero-capacity
rejections,5532lookup/collision calls; existing fire2880/9172also passes.
Includes irregular timing, moving origins, disable/re-enable, full RNG state.
Brightness adapter supplies authored-effect brightness; creation/update bodies
remain actual pinned source. Linux emitter refresh still required.
Scoped formatter/diffcheck passed. All local handles terminal.92uncommitted.

Next runtime integration: ParticleSystem currently contains FireEmitter directly;
convert to variant or equivalent typed ownership without allocating dummy fire
rings for smoke. Preserve systems insertion order and pointer stability. Existing
native tests reference .emitter fields and must be updated to inspect the proper
variant. Scene items need image subtype for transparency submission; rendered
resources already support xl_prepared_particle_image(0). Admission stillfire-only.

## Branch92 runtime mixed-particle checkpoint

ParticleSystem now owns variant<FireEmitter,SmokeEmitter>, preserving list order
and stable system pointers. Binding accepts smoke internally via shared authored
preparation; normal admission remains fire-only. Frame/update/stop/jitter visitors
use the actual emitter type. Scene items carry subtype; production chooses each
prepared texture and correct8x8vs2x2dimensions. Saved state still authored enable
bytes only, reconstructing fresh transient clouds. Existing fire probes use
explicit fire accessor; generic counts visit either type.

Native owner extended with mixed smoke/fire emission and alpha scene, then
activation restore with empty clouds and resumed emission. Full native integration
passed after rebuild. MSVC nested visitor lambda required a typed mutable motion
accessor; fixed without changing semantics. All local handles terminal.

Mixed source buffer matrix initially failed: actual source bCompatible[1] permits
smoke1compatibility even across emissive flag changes. Shared buffer now preserves
this for admitted types. Source comparisons pass264profiles/156090inputs including
smoke/fire/fire-smoke variants. Current normal smoke emissive remainsfalse.
Native compiled final buffer fix and allintegration passed. Scoped formatting and
gitdiffcheck passed.92stilluncommitted. Receipts smoke-runtime-quality.log,
smoke-runtime-fix-quality.log,smoke-color-source.log,render-native-build.log and
native-runtime/controls.log. Linux runtime/source refresh still needed.

Next original smoke.tga source pixel/atlas qualification, Android mixed/actual
hub smoke lifecycle. Review source smoke border/filter/setup image semantics
before claiming image path qualified. Keep outdoors isolated and original mission
unmodified. No APK build or normal smoke admission yet.

## Branch92 original smoke image checkpoint

Source Load investigation found smoke has ZERO atlas border, while onlyfire
gets one-pixel inset. Corrected shared billboard helper to take inset policy,
smoke passesfalse. Earlier supplied-border smoke geometry comparison was not
proof of the image-manager policy. Extended existing actual Load/GetType/table
owner to smoke andfire; source exact zero-border smoke controls nowpass.
Active Load's smoke premultiply/RGB conversion is#if0. Original square image
has one bitmap frame; CParticle atlas animation is the qualified64cellpath.

Original smoke.tga512x51232bit, SHA256
6fe8b321ff2cf6e3bc6edd1c1cf8d692b2ba7796325bdb64254a2556e4d1e6ac.
Existing CTGA owner matched all1048576decodedRGBAbytes. Linux source simulation,
smoke emitter/buffer/billboard/animation and EGLGPU suites refreshed andpass.
Image source owner also passed481header/pixel cases,14malformed controls and
15760POGcontrols. Receipt smoke-original-image.log; smoke-image-source.log;
smoke-image-linux-build.log; smoke-linux-source.log; smoke-gpu.log. Formatting
anddiffcheckpassed. Session65904terminal0; all localhandles terminal.

Next Android normal-admission candidate: derive hub with onlyoutdoors cleared
(exactonebyte), keep originalsmokeobject1segment59. Extend existing skybox
fixture/POGlifecycleowner with smoke mode and protectedpaths, originalsmokeimage
hash, GPUalpha64cells, realdisk/context/stockreturn and observable activecloud.
Do not erase originalfire-substitute control or claim originalhub playable.
Candidate normal smoke admission may be widened onlywith validation inprogress,
not published until complete. Current admission remainsfire-only;92uncommitted.

## Branch92 normal-admission candidate and smoke fixture

UNPUBLISHED candidate now admits strict stationary/uncontrolled single-player
smoke subtype0 alongside fire3. Generic authored shape no longer copies objects.
Runtime serializer preserves originalsubtype and strips particlepayload from
classicshadow; serialization-only probe rejectsbothkinds. Linux inventory suite
passed, including positive smoke translation equality withfire'sclassicheader
and retainedsource subtype; previous subtype0negative control nowunsupported1.

Skybox fixture owner accepts optional--smoke, writespsmokeHOG/descriptor and
verifiesexactONEchangedhubbyte(outdoors60->44). Originalobject1smoke retained;
olddefaulttwo-bytepskyboxcontrolpreserved. Windowsfixturebuild/pass:
psmoke.hog9584308a740140b81950812e00779b5c5e92c35dac4b431852bf7e4ba811c326
psmoke.mn2d7a34f614d094e5f288f3b648b8da30aab6847fb87af47a1a11c50e2b5d77eea
under temp/xl-skybox-20261031/smoke-fixture.
All handles terminal; scopedformatterpass. Receipts smoke-fixture*.log and
smoke-admission-linux*.log. NoAndroidbuild/testyet; doNOTpublishcandidate.
Next extend test_xl_pog_runtime SmokeFixture mode, protectedpsmokepaths,
originalsmokeimagehash and ownedscript; probe actualsmokeGPU64cellalpha,
activation/cloud/scene witness, diskrestore/contextrecreation/stockreturn.
Need bothautomationcatalogs beforecommit. Originalunchangedhubstilloutdoorsblocked.

## Branch92 Android owner and first runs, 02:02 Pacific

Added SmokeFixture/SmokeImage mode to existing test_xl_pog_runtime, protectspsmoke
mission/modpaths and hashpinsoriginalsmoke. Owned68step test_xl_smoke_authored
uses normalhubstart, originalpointsmoke59fixedview, livecloud/scene pixelwitness,
64cellalphaGPU, skyboxfixedview, POGGPU, diskload, EGLrecreation, stockcleanup/load.
Added smoke_lifecycle_probe.h and generalized existing particle_gpu_probe.
Actualsmokepayload[4,10,3,4,8,10,203,78,78,0,0,0,1], position[-13689968,0,52625],
segment59, enabledtrue. Keepsoriginalobject; noactivationbypass.

BothcatalogsPASS:99JSON/492support/314PS,427top-level. MixedformatterPASS.
FirstAndroidcompilefailed missingxl_compatnamespace innewprobes; corrected;
fixedbuildterminal0. Firstowner usedoldinstalledAPK (ownerdoesNOTinstall):
android-smoke-lifecycle.log failsstep4Unknownskyboxoperation, hashesrestored.
Explicitadbinstall-r newAPK succeeded. Secondowner android-smoke-installed.log
loadsnormalhub, failsstep11waitingmissingxl_pog.live_particles. Runtime diagnostic
onlyreturnedfieldsforolderoriginalfire/secretowners, reportwasnull. Corrected
pog_probe toinitializereportatstart_smoke and exposefieldsforpsmoke. Secondrun
terminal1, allbefore/afterhashesmatch; noemulatorprocessleftbytest.

Formatterforreportfixterminal0. New Gradle build started this turn, see livehandle
inlatesttoolresult; logsmoke-report-android-build.log. DoNOTstartanotherbuilduntil
terminal. Then EXPLICITLYinstall android/app/build/outputs/apk/debug/app-debug.apk
withadb-r before rerunning sameSmokeFixtureowner. Needfullpass;92unpublished.

## Branch92 Android smoke pass and final contract correction

Android SmokeFixture passed all 68 steps in 139.346 seconds with the explicitly
installed report-fixed APK. All protected before/after hashes match. The owner
and both final Windows builds exited successfully. Manifest hash remains
DD62EE6B6B61DD65F46F785C6F35248D91A881789E2644EA87632D6F9C8CDCE7.
Receipt: android-smoke-report-fixed.log and temp/xl-smoke-authored-android.

Final Windows inventory passed, but this default invocation does not execute
--effects. Native integration's --effect-fixtures invocation caught the new
smoke contract incorrectly comparing entire stock_level byte vectors. Probe
intentionally retains unused original source records; subtype therefore differs
outside the active translated object table. Corrected the assertion to resolve
the smoke object table and compare its complete 79-byte classic header against
the audited expected header, separately checking retained source subtype zero.
No runtime change. Prior claim of full smoke translation contract qualification
was too broad. Rebuild and run --effects explicitly on Windows/Linux, then rerun
native integration before publication. Scoped formatting is currently running.

Parent PR244 remains OPEN at df7f76a7612555a374946f4347e6f156a0b0bc32;
all checks passed except headed replay parity still in progress at this check.
Branch92 remains unpublished. Next bounded chunk after smoke publication is
source-owned outdoors lighting, required before unchanged original hub admission.

## Branch92 final validation checkpoint

Corrected --effects passes on Windows and Linux:125profiles/3360truncations.
Final native integration terminal0, including mixed smoke/fire activation/scene,
resource controls and existing automatic secret lifecycle. Android68/68 and
matching protected hashes remain valid; subsequent edit was host assertion only.
All process handles terminal. Evidence appended to particle-effects.md.
Before publication finish scoped documentation quality, diff review and commit
only branch92 files; preserve continuation_20261030.md and unrelated game data.

## Branch93 outdoors lighting plan, 02:20 Pacific

Branch92 published as82d5e112728e2eb911f761f99cc4eef833ac70a3, draftPR245
above91. Initial CI running with no failures observed. No local processes live.
Branch93 starts from92, no change to admission until qualification completes.

Bounded chunk: implement the active pinned method-0 outdoors lighting semantics,
qualify on source/native/Android, then test unchanged original hub admission.
Reuse the existing light material, geometry, ambient and POG lifecycle owners.

Source audit corrected earlier evidence: dynlight.cpp Contribute's destination
face outdoors restriction is inside the disabled #else of #if1. Do not port it.
Active Add assigns bSelf from outdoors for face lights. ComputeLightVisibility
in lightprecalc.cpp uses bSelf to reject other destination segments. Separately,
Create assigns bAmbient for outdoor base/overlay lights. ogl_color.cpp's active
bAmbient path only shades the source face's vertices and uses zero distance.
Existing ClassicAmbientLighting already implements that bAmbient vertex gate,
but compile_classic_geometry_lights always passes false to materials.face.
Create also skips skybox segments; verify this exclusion in the ambient source
compiler. GatherStaticLights skips rebuilding registered baked method-0 colors.
Trace the bSelf visibility table consumers before adding any restriction to the
vertex visibility table: these are distinct source tables, not interchangeable.

Required evidence: source comparisons for active material creation/update and
outdoors flags, loaded runtime flags with ordinary/legacy/modern and stock
isolation, native ambient behavior on source/non-source vertices, unchanged Pumo
hub resource/save/context/stock lifecycle. Preserve existing normal smoke/fire
and skybox evidence. Do not claim campaign qualification from hub startup.

Branch93 initial implementation wires normalized source outdoors bits into the
existing material compiler and excludes skybox source lights. Added 54-case
loaded flag assertions to the existing corona owner plus companion material
outdoors/ordinary controls. Not yet built or qualified; admission unchanged.
Further source trace: GatherStaticVertexLights calls SetNearestToVertex with
nSegment=-1, and Contribute returns after range checking for nDestSeg<0, before
LightVis consumption. Do not impose bSelf light-to-segment restrictions on the
method-0 ambient vertex table. Its active face-local behavior comes from
bAmbient in ogl_color. Need extracted source creation/update comparisons and
native ambient behavior controls before claiming this chunk qualified.

## Branch93 source/material validation checkpoint

Native initial build and integration passed, including54loaded normalized flag
assertions in the existing corona owner. Added pinned dynlight.cpp Create
face-selection extraction to the existing companion-lighting executable:
98base/overlay/outdoors cases pass on Windows/Linux. Source Add call recorder
compares presence, created/final texture, brightness and first creation ambient
flag; it does not claim to execute the full Add implementation. Hash-pinned
source revision retained. Receipts outdoors-material-{windows,linux}.log and
corresponding build logs. Existing material ordinary/outdoor controls also pass.

Added four loaded base-only material compiler assertions (legacy20/modern27,
ordinary/outdoors) to the same native/Android corona owner. Saves/restores native
overlay and source segment/version. This newest control still needs build/run;
formatter running in latest tool handle. No admission change and no publication.
Next qualify actual ambient source/non-source vertex colors and skybox source
exclusion, then Android runtime/lifecycle before unchanged hub admission.

Latest four loaded material cases compiled and native integration passed
terminal0 (session57732). Scoped runtime formatting and diffcheck passed.
All local process handles terminal at this checkpoint. No Android build yet.

## Branch93 ambient integration passed

Final native ambient controls passed: both legacy/modern outdoor base lights
illuminate exactly their four source face vertices even with all vertices marked
visible; disabling the source leaves all colors zero. Rebinding a derived skybox
segment removes its ambient light sources. Native integration terminal0, session 67996. Prior source98cases remain valid. Receipt native-runtime/controls.log;
formatter outdoors-ambient-quality.log. Android build started, see current
session handle and outdoors-android-build.log. Must wait terminal then explicitly
install APK before existing SecretFixture owner, whose skybox operation invokes
these same controls. Test assets native-runtime/missions/pfsecret.{hog,mn2}.
Admission still unchanged; next candidate unchanged hub admission must include
full original resources and preserve existing lifecycle controls.

Android build terminal0 in2m54s, explicitly installed with adb-r success.
Existing SecretFixture owner running session86249; console receipt
android-outdoors-controls.log. Do not start another emulator test until terminal
and protected hashes compared. Evidence formatter terminal0. No other handles.

Android outdoors controls PASS31/31 in55.112s, session86249terminal0.
Protected before/after hashes match; logcat confirms face-local ambient/disabled
source/skybox exclusion assertions ran on Android. Receipt
android-outdoors-controls.log and temp/xl-particle-secret-android. All handles
terminal. InstalledAPK includes final93ambient controls. Admission stillclosed.
Next change normal modern outdoors admission and existing negative controls
(current rejection fixture uses outdoors16; replace with truly unsupported128),
then extend existing smoke/skybox lifecycle to ORIGINAL pmines hub without any
changed mission bytes. Update original fire/native hub assertions to appropriate
renderer/resource requirements. Need full inventories, relevant existing rejection
owner/catalogs and original hub Android lifecycle before publication.

## Branch93 original hub admission candidate

UNPUBLISHED candidate now recognizes modern outdoors property16. Positive raw
23/40 translation retains source property and unchanged classic shadow; all256
property combinations updated, unknown128 still rejects. Windows/Linux full
inventories pass. Negative Neptune fixture/script now use unknown128 instead
of supported16; rebuild/run that Android rejection owner before publication.
Both automation catalogs pass (99/492/314;427top-level).

Existing POG owner has OriginalHub mode: hashes original pmines HOG/descriptor,
stages original mods+smoke/fire images, reuses68step smoke script with only
startup operation changed to skybox_start_hub. Runtime verifies hub154properties60
and normalized outdoors flag after restore/context/stock return, versus44for
derived controls. Original fire level2 owner no longer asserts unrelated hub
outdoors rejection. Native fire-only owner correctly rejects missing smoke atlas;
its initial expected skybox-renderer rejection was earlier than the actual
resource validation order, diagnosed and corrected. Needs final native rebuild.

First formatting caught a malformed PowerShell single-pair replacement in
Neptune JSON. Restored that file from HEAD and changed ONLY16->128; formatter
passed. Subsequent Windows failures were stale property16 negative expectations,
now fixed with positive translation and exhaustivemask. No weakened gate.
Current formatter45385running, hub-final-contract-quality.log. All otherhandles
terminal. Next final native integration, Android build/install originalhub test,
Neptune rejection owner and final docs/CI before publishing93.

Final candidate native build and integration PASS, including explicit missing
smoke dependency for fire-only hub fixture and level2 headless fog rejection.
Android candidate build terminal0 in1m52s; explicit adbinstall-r succeeded.
OriginalHub owner nowrunning session92261, receipt android-original-hub.log.
This uses unchanged originalpmines HOG/descriptor/mods plus originalXL smoke/fire
atlases, never modified mission bytes. Wait terminal/hashrestore before anynext
emulator test. Updated Windows rejectionfixture built terminal0 for next Neptune
regression. Both catalogs alreadypass. Only comments changed after APKbuild.

## Original unchanged Pumo hub PASS, 02:46 Pacific

OriginalHub Android owner passed68/68 in121.407s, session92261terminal0.
All protected before/after hashes match. Original HOG/descriptor/mods and smoke
atlas used with outdoors154properties60 intact. Live smoke/scene witness,
64cellGPU,295skyboxmembership,POG resources,diskrestore,EGLcontext,stockreturn
passed. Receipt android-original-hub.log/temp/xl-original-hub-android.
Final render.png was visually inspected: it shows a paused GPU diagnostic color
pattern, not a useful natural hub demo view. Do not present it as a gameplay
screenshot. Need a clean real-scene capture after diagnostics before claiming a
user-viewable demo artifact; completed lifecycle evidence remains valid.

Neptune rejection/recovery owner nowrunning session10258 using rebuiltWindows
xl_rejection_fixture and unknownproperty128. Logandroid-hub-neptune-regression.log.
Waitterminal+hashrestore before further emulator actions. No otherprocesshandles.

Neptune regression passed65/65 in79.044s; protectedhashesmatch, session10258
terminal0. OriginalHub+SceneOnly now reuses initialsmoke witness and resumes
normalrendering before screenshot, separate outputtemp/xl-original-hub-scene.
Formatterpassed; currentlyrunning session46886, android-original-hub-scene.log.
Parent245OPEN at82d5e1127; allCIgreen except two replayjobsstillrunning, nofailures.
Need finalbothcatalogs afterSceneOnlyedit, evidenceformat, review, commit/push93.

OriginalHub+SceneOnly passed17/17 in54.835s, matching protectedhashes. Screenshot
visually inspected: normal live textured close-view, no diagnosticgrid/pause.
Do not oversell it as walkthrough/overview. Session46886needs finalhandlepoll;
owner log PASS and afterhashes complete. Final catalogue97223terminal0; owner
catalogue48311stilllive atlastpoll. Next finish polls/formatdocs, diffreview and
publish93above92. No runtimechanges after finalAPK; onlycomment/owner/docs.

## Branch94 mission exit audit, 02:56 Pacific

Branch93 published7cb0eff377208a252bfb05e6647e9f2f09c68eac asdraftPR246
above92; initialCIrunning, nofailures. Newbranchxl_more_94_mission_exits.
Current original Pumo inventory refreshed with rebuiltLinuxxl_inspect into
pumo-current-inventory.json; boundedprojection pumo-bounded-requirements.json.
Eight of14levels have no level-format blockers (hub,mvlev1/2/3/4/6,mvend,mvlev5).
Others retain wallflags/particles/lightning/objecttrigger/additivewall requirements.
These are inventory results, not package/gameplay qualification.

IMPORTANT newly traced mission-flow gap: hub exits type3 encode2,11,29. Current
Redux stock check_trigger_sub ignores authoredvalue and AdvanceLevel always
selects current+1. Hub rendering/lifecycle qualification does NOT qualify these
routes. Next bounded chunk must implement source exit destinations/progression,
not simply test sequential exit2 and claimallhubbranches.

Pinned CTrigger::DoExit: localplayer/disabled gates; stopaudio/speedboost;
CMissionManager::AdvanceLevel(X2I(value)); for modern positivelevels reject
previouslydestroyed destination; disable exit unlesspermanent; startendsequence.
D1 and negativelevel paths differ. AdvanceLevel uses authoredpositive destinations
only for version>20 positiveD2levels; legacy advancessequentially; D1secret uses
entry+1; D2secret retains priornext. GetLevelState out-of-range returns0.
Source loadgame.cpp end-of-level: next>last with no returnlevel and rotateoff
endscampaign, explaining hub29 in14levelpackage. ReenterLevel saves non-destroyed
currentnormallevel and restores cached destination state onrevisit. State flags
0unvisited/1visited/-1destroyed persist via mission and fullsave owners.

Added puremission_exit_destination helper and actualhashpinned
CMissionManager::AdvanceLevel extraction in existingxl_trigger_flags_reference.
Not connected to runtimeyet. Formatter73406running, mission-flow-quality.log.
Next buildsourceowner Windows/Linux, then qualifyDoExit sideeffectordering and
mission state save/revisit path before corehooks/Android controlled originalexit.
No autonomousnavigation: controlled authoredwall dispatch permitted, do notcall
it earnedroute. Do notexpandcampaignclaim from existinghub68step rendering pass.
All emulatortests/builds from93terminal and originalconfigrestore stilldue11:30.

Pure destination source comparisons PASS10080cases on Windows/Linux; existing
trigger/countdown/disarm matrices also pass. Formatterterminal0. No livehandles.
Receipts mission-flow-{windows,linux}.log. Runtime stillunchanged, nextDoExit and
state/cache semantics. This milestone does not fix progression by itself.

## Branch94 DoExit source-order checkpoint

Added operate_mission_exit callback policy matching actualCTrigger::DoExit.
ActualhashpinnedDoExit extraction added beside AdvanceLevel in existing source
owner. Windows/Linux pass331776activation/flags/sideeffectcases plus10080
selectioncases. Includeslocal/nonlocal,disabled/permanent/all256flags,D1/D2,
legacy/modern,positive/zero/secretlevels,dead/shieldstates,destroyed/visited/new
states andhub2/29targets. Existingtrigger/countdown/disarm matricesremainpass.
Formatterterminal0; no liveprocesshandles. Stillnotwiredtoproduction.

Further cache trace: sourceReenterLevel calls Save(0,-1,0,levelcache) whenleaving
an intactnormalD2level, thenLoad(1,-1,0,destcache) whenvisited. The negative mode
selects cache storage; it is NOT secret_restore==1 player-carry semantics.
Source LoadState bSecret<0 reloads current mission level-state table from cache,
where ordinarysave restore reads storedlevelstates. Therefore revisit must not
roll back mission visited/destroyed flags from an olderlevelsnapshot. Preserve
selected primary/secondary across sourcecacheload and reset playerposition via
InitPlayerPosition(1). Audit player fields ratherthanassuming secret-savecarry.
Relevantlines: loadgame.cpp1409ReenterLevel,1582AdvanceLevel; savegame.cpp2391
LoadState and2423levelstate branch; mission.cpp1104LevelStateName/1117save/1135load.
Next wire runtime missionstate owner and checkpoint integration with explicit
cache-vs-ordinary restore semantics, then controlledoriginalhub exits. No
publication94untilactual progression/persistence controls pass.

## Branch94 runtime mission progress owner started

Added runtime/mission_progress.{h,cpp}, configure source, new-game reset hook in
D2 gameseq and bind afterXLsource load. State is missionidentity + visited/
destroyed vector + pendingNext_level_num. Existing XLsave trailer now schema27
with validated mission_progress field; validate-before-apply, bounds/identity
checks, cache-restore mode preserves currentcampaign timeline. No oldschema
migration (pre-release saveformats disposable perrepo instructions).
Existing native particle/event owner runs roundtrip,cachepreservation and three
malformed-state controls; allstate restored afterprobe. Exitselection policies
stillnotwiredtostockexit/AdvanceLevel; actuallevelcache diskrestore notyetbuilt.
Formatter36039terminal0. Nativebuildrunning56922, render-native-build.log.
Do notstartanotherbuilduntilterminal. InstalledAndroidAPKstillbranch93/schema26;
futureAndroidbuildmustexplicitinstall and protectedtests regenerate theirsaves.

## Branch94 exit dispatch checkpoint, 03:18 Pacific

Recovered prior native owner: no matching build/test process remained and the
native-runtime.log ended with the effect inspection gate passing, including real
base/secret saves. The temporary inventory command matched only its own shell.
PR246 remains OPEN at7cb0eff; CI had no failures, three long jobs still running.

Runtime trigger.cpp now handles TT_EXIT with the source-compared DoExit policy:
authored Next_level_num, local/disabled/destroyed gates, sound/boost stopping,
permanent flags, direct secret return and successful chain termination. Flags
are published before synchronous transitions can invalidate the trigger. The
stock AdvanceLevel still overwrites Next_level_num: progression is NOT finished.
Native build and existing effect lifecycle integration both terminal0 after this
change (session12102); this is regression evidence, not a controlled original
hub exit/revisit qualification. Scoped formatter terminal0 (53360).

Added source-extracted FindNextLevel comparisons and mission_death_destination
policy. Windows and Linux both pass10368 mixed exit/state/ordinal cases, alongside
10080 destination and331776 DoExit comparisons. Existing trigger/countdown/disarm
controls also pass. Linux session26753 terminal0; formatter29761 terminal0.
Receipts mission-flow-{windows,linux}.log, exit-dispatch-quality.log,
death-flow-quality.log, render-native-build.log and native-runtime.log.
No live build/test/formatter handles remain. No Android rebuild since branch93.

Next connect advancement/cache persistence and reactor-death policy, then test
actual controlled transitions. Cache implementation must use full-world restore
without secret_restore==1 carry semantics; existing state_save_all ignores the
filename override outside secret modes, so use a deliberate owned path/helper.
state_save_all_sub consumes the caller's stop_time and rejects active flyouts or
death. Do not cache an invalid flyout pose; inspect end-sequence cleanup before
choosing the cache checkpoint. No branch94 publication until progression and
persistence controls pass. Goal remains active through noon Pacific; restore
original emulator configuration starting11:30 as previously recorded.

## Branch94 real directed transitions and disk revisits, 03:37 Pacific

Production advancement now preserves authored destinations for modern single-
player D2 XL levels, ends the campaign for destinations beyond Last_level, marks
destroyed departing levels, caches intact worlds after scoring, and restores
visited worlds with current mission flags and selected weapons. Cache files use
encoded pilot/mission names and an ordinal, under Players when configured;
otherwise the current PHYSFS write root. New-game state ignores previous caches.
DoPlayerDead invokes the source-compared FindNextLevel policy before advancement.
Core hooks remain in D2 gameseq; implementation is in mission_progress.cpp.

Revisit restoration uses state_restore_all_sub(path,0) under the cache-restore
scope, then InitPlayerPosition(1) and fly_init. It restores the saved player/world
and preserves selected weapons, not secret_restore==1 carry-player semantics.
state_save_all_sub receives a stopped timer and balances it itself. Active
flyout/player-death saves remain rejected. Actual rendered-flyout revisit paths
still need qualification: current native fixture exits do not animate a flyout.

The new integration initially caught visited-state binding using the previous
Current_level_num. LoadLevel sets that global only after the XL loader returns.
Fixed bind to consume the loader's explicit ordinal; ordinary direct loads retain
the current-level fallback. Removed C linkage from C++-only bind/mark methods to
avoid MSVC exception warnings; weapon selection lives in Players in Redux.

Existing particle_secret_test.h now also creates pfrevis (eight-character save
identity) from its derived Neptune/particle archive and runs actual trigger exits
1->3->1->3. It verifies two disk caches, distinct particle/player state, controls,
visited/unvisited flags, full checkpoint restoration after new-game reset,
destroyed-destination rejection and runtime death-route selection. The latter
temporarily supplies three trigger slots, restoring the original table/count.
This is controlled dispatch, not earned navigation or original Pumo qualification.

Final native build29504 terminal0; native-runtime93804 terminal0. Controls receipt
reports all revisit checks plus existing real secret save/return and admission
regressions passed. Failed87717 assumed Neptune supplied three trigger slots;
final93804 supersedes it after correcting the test setup.
Formatter23421 terminal0. Both catalogs passed (32261/31838):99standaloneJSON,
492support,314PowerShell;427top-level entries. No live handles remain.
Receipts revisit-controls-quality.log, revisit-quality.log,
revisit-catalog.log, revisit-owner-catalog.log, render-native-build.log,
native-runtime.log and native-runtime/controls.log. Source policy comparisons
from the prior checkpoint remain valid and were not repeated.
Unrelated manifest SHA256 stillDD62EE6B6B61DD65F46F785C6F35248D91A881789E2644EA87632D6F9C8CDCE7.

Next: Android build/install (installed APK is still93/schema26), extend existing
owner/probe for controlled original hub exit2 and revisits. Ensure owner protection
or cleanup includes the new pilot/mission cache files. Qualify actual campaign-end
destination29 and reactor-death transition, plus flyout handling if reachable;
runtime selection alone does not prove those terminal transitions. Hub exit11
targets a level with still-unsupported features; preserve truthful rejection.
Review cache/player pose and stale camera effects for rendered flyouts before
publication. Branch94 remains uncommitted/unpublished; goal active until noon,
emulator original-configuration restoration still due beginning11:30.

## Branch94 original Android directed-route pass, 03:54 Pacific

Added mission_flow_probe.h through existing xl_pog_probe, test_xl_pumo_flow.jsonc,
and -MissionFlow on test_xl_pog_runtime.ps1 (requires -OriginalHub). The unchanged
original Pumo authored route is1->2->3->4->14, followed by ordinary hub checkpoint
restore and original POG GPU readback. This does NOT manufacture a return exit:
mvlev1 exits3, mvlev2 exits4, mvlev3 exits5 or14, mvlev5 exits5. No original route
back to the hub was found in normal exit records. Derived disk revisits remain
native-qualified; Android revisit-cache restoration still needs its own control.

Probe requires SysUsePlayersDir before any transition, so existing owner backup
and restore of Players includes every generated revisit cache. Original HOG,
descriptor, mod resources and source trigger values remain unchanged. Controlled
dispatch from the current player does not prove traversal or combat completion.

First Android attempt failed from reentrant automation in the destination briefing
(not an engine pause defect). Added input_demo_set_skip_level_intro(1) per exit,
beside temporary SysInputDemoNoRender for synchronous score presentation. This
matches other probes and leaves production presentation unchanged. Preserve this
distinction: headed destination rendering resumes after every transition.

Final build16276 terminal0 (1m52s), explicit adb install success. Android owner
96530 terminal0:32/32 steps in131.441sec, four authored exits including4->14,
hub mission/player/particle checkpoint restoration and POG GPU checks passed.
Before/after protected state hashes match. Receipts:
temp/xl-original-flow-android/{result.json,steps.jsonl,state.json,logcat.txt,
before.sha256,after.sha256}; owner log android-original-flow.log under the shared
xl-skybox-20261031 receipt directory. Installed APK now branch94/schema27.
Formatter55428 terminal0. Both catalogs passed after the new script (74762/80266):
99standaloneJSON,493support,314PowerShell;427top-level. No live process handles.

Next qualify Android derived cache revisits, actual campaign-end29 and reactor-
death transitions; inspect rendered-flyout cache pose/state as previously noted.
Do not repeat completed original route or native source matrices unless changes
affect them. Branch94 still uncommitted/unpublished; goal and11:30 emulator
restoration deadline unchanged.

## Branch94 original Android campaign-end pass, 04:03 Pacific

Extended existing trigger_levelcomplete automation with value trigger:N. Its
shared fire_level_trigger helper validates the index before consuming the action,
then dispatches through check_trigger_sub; advancing before dispatch allows the
following script actions to operate synchronous score/ending screens. Existing
set_debug fire_trigger behavior is unchanged. This follows the existing direct
completion/secret action's advance-before-dispatch pattern.

Added -CampaignEnd to the same protected POG owner and test_xl_pumo_end.jsonc.
The flow_end_ready probe confirms unchanged original hub trigger5 is an available
TT_EXIT with authored value29 and descriptor Last_level14. No presentation bypass
is used in this test. Normal initial score, original ending briefing, final score
and return to New game menu passed16/16 steps in40.968sec on Android (30698 exit0).
Final state in_game=false, first menu item New game. Protected before/after hashes
match. This is controlled dispatch, not earned completion of Pumo's campaign.

Android build24981 terminal0, explicit install succeeded. Formatter49779 exit0.
Both catalogs5649/2673 passed:99standaloneJSON,494support,314PowerShell,
427top-level. No live handles remain. Receipts android-end-build.log,
android-original-end.log, end-quality.log, end-catalog.log, end-owner-catalog.log
under temp/xl-skybox-20261031; device artifacts temp/xl-original-end-android.
Installed APK includes ending controls and all current branch94 production code.
Next remains Android derived cache restoration, actual reactor-death transition,
and rendered-flyout cache validation. No need to repeat the original route/end
controls unless affected code changes. Branch94 not published yet.

## Branch94 Android cache revisits pass, 04:11 Pacific

Added mission_revisit_probe.h via the existing POG probe, -RevisitFixture on the
same protected owner, and test_xl_mission_revisit.jsonc. It consumes native-owner
generated pfrevis.hog/mn2 (under native-runtime/missions), using explicit runtime
trigger edits only in this derived fixture. Original mission data is not edited.
HOG SHA256b0e11a4dbcc8d94261bfb2b6e77a036e8b4d69384bdb5aaee4b8a9ed6d235111;
descriptor SHA256b6636bd55f6db72df985d4490842651d1b89d52378b23dfeb6fb599eb08c6c78.

Android owner77433 exit0:27/27 steps in56.856sec. Real1->3->1->3 disk revisits
restore two distinct particle/player worlds, preserve current mission visited
flags and selected primary weapon, and retain flight controls. Original fire
atlas GPU checks run after each transition. Then ordinary quick-save, deliberate
timeline/player disturbance, full restore and GPU checks pass. Requires protected
Players directory; before/after owner hashes match. No earned-navigation claim.

Build72317 exit0, explicit install success. Formatter45572 exit0. Both catalogs
41573/11562 pass:99standaloneJSON,495support,314PowerShell;427top-level. No live
handles remain. Receipts android-revisit-build.log, android-revisit.log,
revisit-android-quality.log, revisit-final-catalog.log,
revisit-final-owner-catalog.log under temp/xl-skybox-20261031;
device artifacts temp/xl-revisit-android. Installed APK includes all current94
production and probes. Production code unchanged since original route/end passes.

Remaining94 gates: actual reactor-death transition (not merely selector policy),
rendered-flyout cache pose/control/camera state, focused final review and stack
publication. For flyout owners see android/tests/test_flyout_safety.ps1 and
test_mission_metadata_flyouts.ps1; no autonomous navigation is authorized. Preserve
source cache player semantics when checking death: source ReenterLevel restores
the cached player and preserves only selected weapons explicitly, so audit lives
against source rather than assuming secret_restore==1 carry-player semantics.
Goal remains active through noon; begin emulator original config restore11:30.

## Branch94 actual Android reactor-death transition pass, 04:20 Pacific

Extended the existing revisit probe/script with a controlled DoPlayerDead call
while Control_center_destroyed=1 in level3, using its explicit exit back to cached
level1. This is the production terminal entry used by reactor countdown, not just
the destination-selection helper; it does not exercise the full countdown timer.
Before death, the probe confirms the exact cached level3 file exists for pilot
player/mission pfrevis, then verifies that destruction removes it. Restore of the
older level1 world preserves level3 state=-1 and level1 state=1. Particle world,
headed GPU resources, player flight controls and post-death ship reset all pass.

Source audit: active savegame.cpp1733 m_bSecret!=1 loads the complete cached
player (including lives); ReenterLevel preserves only selected weapons explicitly.
loadgame.cpp2440+ decrements the departing life, advances, then ResetShipData.
The control deliberately changes departing lives and expects cached lives after
return, followed by100energy/100shields/primary0. This source behavior must not be
silently replaced with secret_restore==1 player carry semantics.

Android owner49101 exit0:31/31 steps in64.001sec, protected before/after hashes
match. Build2388 exit0 (1m42s), explicit install succeeded. Formatter35049 exit0.
Catalog93723/55714 both pass (99JSON,495support,314PowerShell;427top-level).
No live handles remain. Receipts android-death-build.log,
android-revisit-death.log, death-android-quality.log, death-catalog.log,
death-owner-catalog.log under temp/xl-skybox-20261031. Device artifacts are now
the latest temp/xl-revisit-android generation. Installed APK includes death probe.

Remaining investigation: rendered-flyout cache state. Both pinned source
StopEndLevelSequence (main/endlevel.cpp399) and Redux stop_endlevel_sequence clear
the animation flag then call normal completion; neither visibly deletes its
lookback camera there. Redux cinematic leaves player MT_NONE/CT_NONE and can
leave Viewer on the camera. Reentry already calls InitPlayerPosition and fly_init,
but cache writing after a completed intact cinematic needs focused verification.
Destroyed exits skip cache writing, so ordinary destroyed-mine flyout tests alone
cannot prove the intact-cache case. Existing full animation controls are in
android/tests/test_upstream_compat.cpp write_endlevel_lifetime_trace (~9830),
invoked by test_flyout_safety.ps1 with retail D1 and Grand Finale assets. Their
existing runs set Control_center_destroyed=1. RuntimeLayoutFixture only converts
Neptune23/40 to25/27, not arbitrary classic D1 geometry. Do not assume a classic
rendered-flight pass qualifies modern intact cache creation. No autonomous 3D
navigation; controlled poses/engine cinematics are permitted test mechanisms.

## Intact rendered flyout exposed preflight ordering, 04:39 Pacific

Initial flyout build and both automation catalogs passed (99 standalone JSON,
496 support scripts, 314 PowerShell; 427 top-level). Installed APK successfully.
Android owner25222 terminated exit1: derived original level2 tunnel reached
exterior cinematic phase and normal score screen, then next-level admission
rejected mvlev2.rl2 with model definition registry changed since mission
initialization. Failure step16/19, not a cinematic routing failure. Before/after
protected asset hashes match, including temporarily supplied retail D1 PIG.
Artifacts temp/xl-flyout-cache-android; owner android-flyout-cache.log.

Root cause: load_exit_models replaces extra mission models with exit scenery and
sets Robot_replacements_loaded. Normal level loading resets those definitions,
but the XL AdvanceLevel preflight runs before that reset. Added existing
reset_level_robots_file call for committed XL progression after departing cache
save and before destination preflight. No relaxation of registry validation.
Shared hook formatted/scoped quality exit0; git diff --check passes. Native
build terminal exit0. Android rebuild39217 and native runtime43834 are live
as of this checkpoint; poll at 30-60 seconds, do not restart on timeout.
Logs android-flyout-reset-build.log, flyout-reset-native-runtime.log under
temp/xl-skybox-20261031. Rerun focused flyout only after Android build and install.
Production changed: prior route/death passes do not cover this new reload order;
existing native lifecycle is running to check cache/secret resource regressions.

## Intact flyout and cached return passed, 04:41 Pacific

Android rebuild39217 exit0 (1m51s), explicit install success. Focused owner36093
exit0:19/19 steps in85.259sec. Exterior cinematic phase reached; normal score
screen advanced to original destination3; derived return trigger restored cached
level2 with energy83, particle world/resource, current visited timeline, interior
spawn, CT_FLYING/MT_PHYSICS and Viewer==ConsoleObject. GPU control passed after
return. Protected before/after hashes match. Production reset ordering fix solved
the observed registry rejection without changing resource admission checks.

Native runtime43834 exit0; controls.log confirms disk revisit/full checkpoint/
new-game isolation/destroyed rejection/death routing and automatic secret save
lifecycle. Native build also exit0. No live test/build handles remain. Installed
APK includes the reset fix. Reuse completed policy and catalog validation.

Parent PR246 rechecked at04:38: OPEN, exact7cb0eff377208a252bfb05e6647e9f2f09c68eac;
all executed checks SUCCESS including ASan, native save/replay, headed parity,
LAN and both asset-free XL platforms. One optional timing export SKIPPED.

Next: focused branch94 final diff review, evidence document and selected-file
commit/push/draft PR above93. All94 changes remain uncommitted. Preserve unrelated
manifest/shearbase/w5am files and old continuation/handoff edits. Continue toward
noon; restore emulator original SwiftShader launch configuration from11:30.

## Branch94 published and next bounded chunk, 04:48 Pacific

Commit118f28044548163739c0d26a9f2c4620a4833a4d pushed to origin
xl_more_94_mission_exits. Draft PR247:
https://github.com/larsonm-personal/dxx-redux/pull/247
Verified OPEN/draft, base xl_more_93_outdoors_lighting, one commit, exact head.
CI queued/in progress at publication; parent246 all executed checks successful.
No live local build/test processes. Unrelated working files remain unstaged.

Next bounded chunk: wide XL wall flags / automatic-door marker obstruction.
Current inventory shows original tulev1.rl2 has exactly one portable blocker,
wall181 flags65432 (0xff98). Pinned wall.h defines WALL_IGNORE_MARKER=256;
upper reserved bits also set, so do not normalize by merely narrowing to byte.
Pinned CWall::Read reads UShort for modern game version; runtime consumers use
IgnoreMarker in main/segment.cpp828 and main/wall.cpp510,841. DoorIsBlocked
in main/segment.cpp625 skips markers conditionally. Preserve low classic flags,
carry marker policy in XL-owned metadata, audit reserved-bit consumers against
source, extract reference behavior, then qualify actual door obstruction and
original tulev1 admission. Do not admit by simply deleting the wall_flags gate.

Other five blocked levels combine lightning/particle subtypes, object triggers,
additive walls and these flags. This chunk directly extends original sequential
route beyond mvlev6 while retaining the independent gates. Inventory receipts:
temp/xl-skybox-20261031/pumo-bounded-requirements.json and
pumo-current-inventory.json (large: parse/project fields, never dump full).

## Branch95 door-marker implementation in progress, 04:52 Pacific

Created xl_more_95_door_markers from published94 at118f28044548163739c0d26a9f2c4620a4833a4d.
Uncommitted new door_obstruction.h matches pinned object exclusions and front-list/
both-side traversal. Existing reference_close_door.cpp now compares production
policy against extracted DoorIsBlocked (existing84 cases); needs expansion for
asymmetric geometry/list membership, plus live modern integration controls.

Important source/runtime distinction: existing runtime/doors.cpp blocked() has an
intentional two-list geometry correction for previously qualified legacy XL.
Modern layout now routes through xl_door_is_free in preview.cpp, which owns full
source wall flags and uses bit256. Legacy path remains unchanged. Added API in
preview.h and one call in doors.cpp. Initially considered core is_door_free hook,
then removed it after locating the existing XL door owner. d2/main/wall.c should
have no remaining diff. Do not duplicate obstruction handling in core.

Admission/probe translation gates remain unchanged: wide wall flags still reject.
Before admission: audit reserved flag consumers, add modern integration fixtures
with marker flag both states, compare actual runtime geometry/lifecycle and disk
restore, test original tulev1. Existing source policy walks only front object's
linked list and tests both side planes; do not accidentally replace this with
legacy two-list behavior. Source PokesThrough ignores zero-size objects.

Formatter initial50371 failed solely installed clang-format access denied;
escalated57813 terminal0 all scoped checks pass. Native build57163 LIVE; wait
30-60seconds then poll, do not restart on timeout. Command render-native-build.cmd
writes temp/xl-skybox-20261031/render-native-build.log. No Android build/test yet.
Reference owner already exists: tests/reference_close_door.cmake extracts pinned
DoorIsBlocked. Test xl_close_doors via tests/close_door.cmake produces native
close-door-samples.txt/door-animation-samples.txt then calls reference executable.
Existing legacy controls runtime/door_obstruction_control.h expect two-sided
actor blocking and should remain valid for legacy fixtures.

Native57163 terminated1: find_connect_side requires non-const segment pointers.
Corrected local front reference to auto& (no behavioral change). Rebuild42357
is now live; same render-native-build.log. All other handles terminal. Admission
still blocked and no claim of runtime validation yet.

## Branch95 source geometry mismatch isolated, 04:56 Pacific

Build42357 terminal0. Expanded extracted DoorIsBlocked policy comparisons from84
cases to336 (21types x2marker modes x4overlap patterns x2list memberships) in
reference_close_door.cpp; formatter81026 terminal0. Reference build succeeded.
First xl_close_doors failed Two-sided actor obstruction differs, as expected:
existing fixture IS modern23/40, not legacy. Corrected connected-only actor
expectations in close_door_test.h and door_obstruction_control.h to source front
list behavior. Formatter22054 terminal0, nativebuild77840 terminal0.

Second xl_close_doors terminates1 at Full-pool reversal allocated or failed.
Root cause now proven by source: check_poke uses Redux get_seg_masks without
bCheckPoke, so checking the back segment marks objects wholly past its plane as
intersecting. Source CSegment::PokesThrough calls SideMasks(...,true), which
calls CSide::Masks with bCheckPoke. main/side.cpp493: each face requires BOTH
xDist-xRad < -PLANE_DIST_TOLERANCE AND xDist+xRad >= -PLANE_DIST_TOLERANCE;
planar two-face sides require both counts, otherwise at least one. Zero radius
is excluded. Do NOT weaken the full-pool lifecycle assertion.

Next fix: use existing compiled XL side geometry, not core check_poke, for modern
sphere/door intersection. runtime/particle_world.cpp already compiles exact side
anchors/normals/IsPlanar equivalent from compile_classic_lighting_side:
triangles from compiled classic side; anchor=min(triangles0,triangles2),
opposite=min(triangles1,triangles4), pokes_out=distance(opposite, chosenface)>250.
ParticleSide in ../particle_world.h has anchor/normals/faces/pokes_out; its
particle_plane_distance uses fixed_vector_dot. Reuse or factor the side compiler
rather than approximating geometry. Add source Masks/PokesThrough comparison
with boundary distances and planar/nonplanar cases, then rerun native owner.

All process handles terminal. Current tests fail (known geometry bug), admission
still blocked; no Android build/test for95. Parent247 checked04:53 exact118f2804:
no failures; Windows/Linux asset-free checks succeeded, remaining runtime/jobs
still in progress. Source test runner temp/xl-skybox-20261031/door-reference-windows.cmd
builds reference then ctest -R ^xl_close_doors$; logs door-reference-{build,test}.log.
Native samples/artifacts under temp/xl-particles-20261030/contracts-windows64/close_door.

## Branch95 exact geometry and native door pass, 04:59 Pacific

Factored existing compiled-side algorithm into runtime/particle_world.cpp
compile_particle_side(segment,side,ClassicLightingSide), declared in its header.
Particle world uses the same helper without algorithm changes. Modern door
adapter now compiles the two XL side geometries and applies door_sphere_pokes:
zero radius false; face interval d-r<-250 and d+r>=-250; both faces required for
planar sides, either for pokes-out sides. Removed core check_poke dependency.

New tests/reference_door_masks.cpp executes extracted hash-pinned CSide::Masks
(main/side.cpp SHA5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989),
registered on existing xl_close_door_reference. 1620 boundary cases pass.
Reference DoorIsBlocked comparison now336 cases including asymmetric overlap and
front/back list membership. Existing3072 transition/allocation and660 animation
cases pass. Native xl_close_doors passes14.58sec including previously failing
full-pool lifecycle, saves and stock isolation. Build66990exit0; reference/ctest
owner1420exit0. Scoped format95646/26909 both terminal0.

No admission changes yet. Need high-flag fixture with real marker blocking vs
ignore marker, current source flags surviving saves/restores, reserved-bit source
audit, Linux reference, Android existing door owner, original tulev1 route.
Modern source front-list behavior now explicit; prior two-list expectations were
corrected only after source extraction exposed the difference.

Native particle/skybox/save lifecycle now running after shared geometry extraction:
see next session ID/tool receipt, output door-geometry-native-runtime.log under
temp/xl-skybox-20261031. Poll30-60sec and wait terminal before reusing assets.
Door test receipts reference.log and controls.log under
temp/xl-particles-20261030/contracts-windows64/close_door. No new catalogs needed
yet: no scripts/owner registration changed on95. Original unrelated files remain.
Live native lifecycle handle: 52209. No other live handles.

## Branch95 live high-flag/save proof, 05:05 Pacific

Native particle/skybox/save regression52209 exit0 after shared side compiler
extraction. Added modern wall_flags admission exception and probe low-byte
translation with full16bit source ownership. Inventory still records wide flags;
independent additive rendering remains blocked. Source only defines bits0..8,
IgnoreMarker is only high-bit consumer found; reserved high bits retained in source.

close_door_fixture now optionalflags/mission parameters (default original fixture
unchanged). New mpblock flags0xfe10 and mpignore0xff10 differ solelybit256;
both preserve reserved upper bits and normal low flags. Native owner runs shared
actor/marker controls before and after disk restore through stock for each:
192 cases pass. Nativebuild5821 exit0, ctest8782 exit0 (15.52sec), source-reference
matrices stillpass. Scopedquality76226 exit0. Receipts existing close_door dirs.

Android build91917 launched android-door-marker-build.log; inspect current tool
receipt for terminal/live state. No install or device test yet. Portable inventory
owner terminated1 on a stale wide-flag negative assertion in wall_key_contract.
Replaced with full-source/low-byte ownership checks for256,512,32768,65040,65296,
65432 across modern23/25/27; flags260 stillrejects independent additive rendering.
Formatter56032 currentlylive for inventory_test.cpp. After terminal, rerun
admission-test.cmd. This test change is not yet compiled. No live native owners.

Next: finish portable contract, Linux oracle, install Android successful build;
existing test_xl_close_door_runtime.ps1 pins original close fixture hash and should
remain unchanged. Extend owner/probe for high-flag fixtures (don't claim existing
48case test proves marker=true). Also qualify original tulev1 route with current
Pumo owner after checking gates. No publication95 until these checks pass.
Latest handles: formatter56032 terminal0. Android91917 still live; portable admission32714 now live. Poll30-60sec; do not restart.

## Branch95 Android baseline failure and marker owner prepared, 05:12 Pacific

Portable admission32714 exit0 after wide-flag assertion update; all inventory
contracts pass. Android91917 eventually terminal0 (3m21s), install succeeded.
No restart was performed while its live process remained confirmed.

Current native-generated close.hog hash34182874939bc4c0058984680bf87c248e523f0af9c1f84584ca2f724eda1660;
owner previously pinned3c8a1f... Changed expected hash to current generated fixture,
which passed native controls; descriptor remains6194d572... Need document/audit
fixture provenance/difference before publication rather than assume bytes same.

Android baseline owner29573 terminal1 atstep20/80: expected motion_audio.attempts1,
got2 after face_view21/4 distance4 and fire_trigger1. Doorstate2 and position21
assertions passed; source front-list check now allows closing from connected-only
list, resumed physics/frame handling makes this old assertion nondeterministic.
Do not simply change count to2 to hide behavior. Use controlled probe for list/
geometry semantics and make normal presentation test consistent with source.
Protected before/after hashes match. Artifacts temp/xl-close_door-android,
ownerlog android-door-runtime.log. InstalledAPK includes production95 highflags,
but NOT latest marker probe operation below.

Prepared not-yet-built Android marker coverage:

- door_probe.cpp accepts marker_obstruction and calls sharedrun(require,true).
- existingtest_xl_close_door_runtime.ps1 adds -MarkerFixture, pins mpignore.hog
  639787b543aa43323a9da4e532fb0d697f6202d88b67581e6255bcbd2ba8a393,
  uses existingclose descriptor (same bytes), stagesprotected close missionpaths,
  output temp/xl-marker-door-android, selects test_xl_door_markers.jsonc.
- New owned script startsmission, runs48marker/actor cases, setsenergy73/saves,
  disturbsenergy1/asserts, loads/asserts73, repeats48cases. No autonomous navigation.
- Formatter currentlylive (see toolhandle); wait terminal before rebuild/edits.
  Bothcatalogs required after owner/scriptchanges; notrunyet.

Next: resolve baseline script's obsolete connected-side expectation with direct
source-backed controlledcoverage; finish formatter/catalogs; buildnewAPK andrun
MarkerFixture serially. Then Linuxreference, originaltulev1, evidence/publication.
All otherhandles terminal. Goal activeuntilnoon, originalemulatorrestore11:30.
Only live handle: scoped formatter44615, log door-android-owner-quality.log. All build/test handles terminal.

## Branch95 Android marker pass and Linux oracle, 05:18 Pacific

Build5287 terminal0 (2m6s), installed successfully after baseline owner ended.
Catalog76212/36086 terminal0:99standaloneJSON,497support,314PS;427top-level.
Formatter8623 terminal0. Linuxoracle17200 exit0:1620sphere/plane,3072transitions,
336obstruction,660animation comparisons allpass using completed native receipts.
Log door-linux-reference.log.

Baseline script now pauses before connected segment21 pose/trigger, asserts
closing3 and motionaudioattempts2, dispatchesopen0, asserts BOTHfacesopening1 and
noaudiofailure, resumes and waitsforwaiting2. Owner51297 initiallyfailed added
assumption attempts3 on reopen (observed2); that new assertion was replaced with
actual reopening/pairedstate and failurecheck. Existing audio assertions remain.
Need final pass; don't report baseline successful yet.

Marker owner56095 initiallyfailed only scalar numeric assertion1 vs reported1.0.
Changed energy checks to range[1,1] and[73,73]. Rerun80523 terminal0:
17/17steps in28.994sec. Real marker/actor obstruction before and after diskrestore,
energy73->save->1->load->73 verified. Protected hashesmatch. Artifacts
temp/xl-marker-door-android; log android-marker-numeric-runtime.log.
Owner success text currently overstates stock/rewind for MarkerFixture mode;
fix conditional message after active owner terminal, before final publication.

mpignore.hog and currentclose.hog are exactly93509bytes and differ only at46421
and46446, source highflag bytes0->255. Currentdefaultfixturehash was stale inowner:
ownerlastchangedPR170 whereas keyed-wall generator changedPR193 (nonstandard key
bytes). This establishes generator evolution, but no exact oldfixturebyte diff
available. Currentgeneratedfixture is pinned and passednative; don't call bytes
unchanged versus oldowner. Descriptorhash unchanged6194d572...

Only livehandle38496: full86step Android baseline owner, log
android-door-final-runtime.log. Wait30-60sec then poll; no reinstall/test inparallel.
Remaining: baselinepass, finalformat/catalogafterlastscriptassertion changes,
originaltulev1 route qualification, evidence and branch95publication. Goalactive.

## Branch95 baseline source alignment and original-route build, 05:23 Pacific

Owner38496 terminal1: reopen trigger0 did not reverse closing door. Source audit
confirmed CSegment::ToggleWall (segment.cpp1013) onlyopens WALL_DOOR_CLOSED,
matching Redux wall_toggle. Revised script now moves player clear whilepaused,
resumes/waitsclosed, then dispatchesopen0 and waitsnormalwaiting. No production
change. Owner77700 advancedpastthat and failed atstep32/87 on obsolete closed
visibility expectationplayer_flags2 vsactual6 (bothfacesstate0,noactiveentry).

Current XL check_transparency delegates xl_side_texture_transparency, which
honors base/overlay transparent/supertransparent bitmap flags. WID6 is transparent
but nonflyable (WID_FLY_FLAG1 absent); script's oldopaque2 assumption predates
currentpixelvisibility work. Updated all six closed player_flags assertions to6;
actual crossing/state/audio checks remain. Do not claim opacity. Fullowner15038
nowlive, log android-door-current-visibility.log. Protectedhashes from earlier
runs restored; verify latest uponterminal.

Original Pumo probe extends route14->5->6->7 with original triggers, endingcheck
flow_file tulev1.rl2 andexitcount7, thenexistinghubcheckpointrestore/GPU. No
modifiedgeometry/triggers, stillcontrolleddispatch notearnedtraversal. Format
76568 terminal0 (before last visibility assertion edits). Build31949 live, output
android-pumo-tulev1-build.log. Do NOT install while15038deviceownerlive.

Marker owner successmessage now conditional and accurate (nofalse stock/rewind
claim for17step marker run). Lastmarker80523pass unchangedproductionproof.
Parent247 at05:20: allcompletejobsSUCCESS; native/headedreplay stillIN_PROGRESS.
Next: finishbaseline/build, installandrunOriginalHub MissionFlow extendedroute,
finalscopedformat/catalogs, evidence/doc andpublication95 afterallgates.

## Branch95 remaining Android full-owner timing issue, 05:30 Pacific

Android routebuild31949 terminal0 (2m5s), installed afterdeviceownerterminal.
Fullbaseline15038 passedactualcrossing butfailedstep50/87: close-aftercrossing
foundnoactiveentry becauseautomaticclosealreadycompleted. Added open0/waitwaiting
precondition aftercrossing. Owner62211 failedstep52/89: immediateforcedclose can
finishbeforeobservation because sourcetimerreversal playtime-waittime. Added1s
waiting interval. Owner80268 stillfailsstep53/90 onactive_entry0vs-1. Do notkeep
addingtimingassumptions: nextdiagnosis should pause beforepostcrossingfire1,
inspectwaitingstate/timer, assertdispatch state3/entry whilepaused, thenresumeand
waitactualcompletion. Potential stalegeometry/actorpose or ephemeraltransition;
notyetprovenexactcause. No production edits since native/markerpassed.

Allbaseline owners terminal/restored protectedstate. Original Pumoextendedroute
nowrunning (see nexttoolhandle), android-original-tulev1-flow.log. Do not run
anotherdeviceowner/installuntilterminal. InstalledAPK includes all95production,
markerprobe and extendedrouteprobe. No otherlivehandles.

Added evidence/door-markers.md draft with actual source/native/marker proof,
explicitlypending fullAndroidbaseline andoriginalroute. No publicationyet.
Need finalformat/catalogafterlatestscriptedits. Baselinescript currently90steps,
sourcepausedconnectedcase andpostcrossingopen/wait1s. NativeandLinuxsourceproof
alreadycomplete; reuse. Parent247lastcheckallcompletedSUCCESS withtworeplayjobs
pending. Restoreoriginalemulatorconfig11:30, goalthroughnoonunchanged.
Only live owner34003 (original Pumo route). Postcrossing baseline script now pauses beforefire1, requires waiting2, assertsclosing/entry underpause, then resumes forcompletion. Notformatted/rerunyet.

## Original level 6 placed-mine preflight correction, 05:40 Pacific

Owner31019 terminal1 at paused step55: closing state and active entry passed;
only motion_audio.attempts>=2 failed (actual1). Pinned wall.cpp537 creates close
audio only at zero door time, matching doors.cpp. Removed only that extra paused
count assertion; previous zero-time audio checks and failure checks remain.
Full owner57690 is live, android-door-paused-source-audio.log. Do not install or
run another device owner until terminal.

Original route34003 terminal1 reached level5, rejected mvlev6 object4 for saved
robot09.pof. This is OBJ_WEAPON51 (placed mine). Pinned loadobjects.cpp158-165
replaces its model from weapon51; Redux gamesave.c338-347 already does the same.
loadobjects.cpp SHA256 01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c.
Preflight now resolves exactly OBJ_WEAPON/PMINE_ID through Weapon_info, after the
existing weapon dependency walk; other object model-name checks remain. Android
flow probe verifies original level6 mines' actual model, size, mass, drag and
free-spinning flag after load. No external robot model substitution.

Formatter78063 terminal0. Native runtime rebuild terminal0 (preview.cpp plus
xl-preview-lifecycle.exe). Android build60189 live, android-placed-mine-build.log.
After both deviceowner/build terminal, install then retry extended original route.
Parent PR247 checked05:39: all required jobs passed, optional timing export skipped.
Branch95 still uncommitted/unpublished; preserve unrelated files as before.

## Placed-mine fresh history and remaining full-owner gate, 05:52 Pacific

Original route18118 terminal1: level6 load and mine model/physics assertions pass;
leaving6 fails save capture, not level7 admission. Native fresh objects have
last_hitobj0, but XL zero is a hit COUNT whereas Redux zero is victim INDEX0.
Pinned ReadObjectInfo clears objects; CObject::Read weapon payload is parent-only.
Modern fresh XL load now sets CT_WEAPON/OBJ_WEAPON last_hitobj=-1. Saved objects
and earned XL trailers are restored afterward; no save guard bypass.

Native rebuilt. xl_projectiles CTest wrapper exit8 only because reference binary
was absent; controls.log proves native damage/history/save/stock/corruption cases
passed. Built reference and ran completed samples:1920 impacts plus1650 history
controls pass. Do not rerun native gameplay solely for missing wrapper binary.
Android build91234 terminal0 (1m56s), installed. Original route retry pending.

Door owner26017 terminal1 reached step78/99. Paused checkpoint dispatch state,
active entry and completed closure passed; post-restore audio counter is1, not2.
Source starts close sound only at timer0; restored counters restart. Changed
only restored closure minimum to1; earlier close audio2 and failure checks remain.
Formatter3751 terminal0. Full door owner10056 live, android-door-restored-audio.log.
No concurrent device owner or install until it exits.

Added required-model negative guard for placed mines (model-1 must not fall back
to saved-name lookup). Route probe temporarily removes weapon51 model, requires
specific rejection, restores definition, verifies admission recovery. This is
not yet in installed APK. Formatter currently running; next build after terminal.
Both automation catalogs66090/74438 terminal0 before latest script adjustment;
rerun final catalogs before publication. Branch95 still unpublished.

## Final original-route retry and remaining door tail, 05:57 Pacific

Build1488 and formatter97945 terminal0; final APK installed after door10056
terminal1. Door10056 reached98/99: final post-stock-restore crossing allowed an
automatic close before scripted close. Same fix as earlier crossing: explicit
open/waitwaiting, pause, assert pairedclosing/entry, resume/closed. No engine
change. Formatter70540 terminal0. Final script104steps, not yet rerun.
Protected before/after hashes match for last owner.

Only device owner70400 live: original Pumo full41step route, log
android-original-mine-history-flow.log. Installed APK includes mine model guard,
fresh-history sentinel and negative/recovery assertion. Do not install/run any
other device owner until terminal. Native final guard rebuild terminal0; reuse
completed native projectile controls and source oracle. Final catalogs running.
Next: inspect route result, run final door104step owner, update evidence/current
feature docs truthfully, then commit/push draft95 above94 after all gates.

## Branch95 validation complete, 06:03 Pacific

Original Pumo70400 terminal0:41/41steps202.834s. Original route
1->2->3->4->14->5->6->7(tulev1.rl2), level6 real mine model/physics/empty-history
assertions, required-model rejection/recovery, departure save, hub checkpoint
recovery and GPU allpass. Protectedhashesmatch. Log android-original-mine-history-flow.log.
Full door31710 terminal0:104/104steps99.361s, including final stockrestore/crossing,
pairedtransition andcompletedclosure. Protectedhashesmatch. Log
android-door-final-stock-restore.log. Marker17step proof remains applicable.
Final catalogs78227/42690 terminal0,99standaloneJSON/497support/314PS/427top-level.
No live device/build/test handles. Native finalbuildpass, source/reference/native
proofs reused. Evidence docs updated, final doc formatting and publication next.
Publish95 above94; parent247 allCIgreen. Do not merge. Preserve unrelated files.
