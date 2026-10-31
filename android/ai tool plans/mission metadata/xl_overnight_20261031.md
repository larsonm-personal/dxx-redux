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
