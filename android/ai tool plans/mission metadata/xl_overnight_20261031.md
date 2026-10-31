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
