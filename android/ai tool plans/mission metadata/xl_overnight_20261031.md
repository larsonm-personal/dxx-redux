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

Current AVD DxxOvernight_1 on port 5554 temporarily uses host GPU, launcher PID
37712. Original emulator processes 53120/43828 were verified terminal before
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
