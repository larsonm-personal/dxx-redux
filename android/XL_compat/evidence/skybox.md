# Skybox requirement audit

Status: normal skybox admission remains closed. Source-backed collision decisions
and the native collision hook are implemented; native integration passes. Investigation branch: xl_more_87_skybox,
stacked locally on published particle tip 55c937273b0633d2b5aa92e3f5fcd616400fa46c.
Branch 86 is based on cmake 0e4587e7dedbf0f21955c6fda351c923ff5e23c5.
Visibility/rendering and complete skybox gameplay are not yet qualified.

## Branch 88 textured rendering checkpoint

Work continues on xl_more_88_skybox_rendering above #239. Normal skybox
admission remains closed. Actual pinned FaceIsVisible, RenderSkyBox and
CWall::IsInvisible comparisons pass on Windows and Linux: 25725 face cases,
3944 second-normal distinctions, eight automap/depth gates and 18432 wall
inclusion cases. Split-13 uses expanded triangle vertex 3 (corner 1), not
corner 3. Source cloak threshold is 31, distinct from Redux's fade constant 34.

The runtime textured-face pass runs before transparency/particle depth capture,
uses compiled source normals and triangle order, forces full brightness, and
restores depth/window state. Native controls pass 474 loaded faces across mixed
and all-skybox bindings. Android first found ordinary OGL and visibility-only
paths still marking skyboxes explored. Those paths now exclude skybox segments;
the repeat passes all 31 existing secret-owner steps in 53.189 seconds, including
real-frame red/green texture pixels, static-light independence, automap isolation
and subsequent secret-save/GPU lifecycle checks. Protected file hashes match.

Receipts under temp/xl-skybox-20261031: visibility-source.log,
visibility-linux.log, native-runtime/controls.log, android-render-scene.log
(initial automap failure), android-render-automap-fixed.log (pass). Scoped
quality and native/Android builds pass. This is derived geometry evidence;
it is not original Pumo hub qualification. Distinguishing depth/occlusion,
separate skybox objects, colored boundaries, complete weapon entry and authored
skybox resource/save/context lifetime remain to be earned.

## Original Pumo hub

Read-only current inspector receipt:
`temp/xl-evening-20261030/pumo-admission-audit.json`. Original package hash remains
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.

`pm-hub.rl2` still has 298 blockers: 295 function-10 skybox segments, one
segment-properties record (segment 154, bits 60), and two reports for the same
unsupported smoke owner (object 1, segment 59, particle subtype 0). The skybox
contains no authored objects, has 12 directed links into non-skybox geometry,
534 external faces and 12 walls. Its segment functions are 135 ordinary, nine
function-9 and 295 function-10. Do not turn off all these gates as one feature.

The source constant for outdoors is property 16. Segment 154 combines blocked,
no-damage, outdoors and light fog. Outdoors affects lighting, so it must not be
accepted as an inert flag. Smoke is a separate simulation/image/shader profile.

Seven levels have zero format-level runtime blockers after strict-fire admission:
mvlev1, mvlev2, mvlev3, mvlev4, mvlev5, mvlev6 and mvend. This is inspection only;
resources, renderer and actual gameplay are not established by that result.

## Pinned source consumers

Reference revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.

- main/skybox.cpp: CountSegments and BuildSkyBoxSegList build an ascending
  function-10 list; Destroy clears bHaveSkyBox.
- render/rendermine.cpp: RenderSkyBox gates automap separately and enables depth
  writes. RenderSkyBoxObjects traverses objects in every skybox segment.
- render/fastrender.cpp: RenderSkyBoxFaces uses a separate pass, full brightness,
  FaceIsVisible and RenderMineFace, restoring prior brightness. Corona setup
  skips skybox and outdoors segments.
- render/renderframe.cpp: skybox pass follows RenderMine and precedes effects.
- main/findsegment.cpp: skybox and ordinary position searches are separate.
  Existing particle-world source comparisons already cover the particle subset;
  reuse them rather than repeat their investigation.
- physics/physics.cpp: HandleWallCollision allows weapons to leave skybox
  boundaries, retires qualifying weapons at external boundaries, and can move
  weapons through transparent pixels into a separately located skybox segment.
  The pinned default bNewPhysCode is 1, selecting DoPhysicsSim and its
  HandleWallCollision method. The legacy branch contains the same wall policy.
- lighting/dynlight.cpp: outdoors face lights are self-only and restrict
  destination face selection. This is separate from skybox full brightness.

## Next bounded chunk

Qualify skybox visibility and weapon-boundary decisions against actual pinned
source, then connect the matching runtime consumers behind existing admission.
Keep normal skybox admission blocked until rendering, lookup, weapon transitions,
resource/save lifetime and classic isolation have evidence. Native render_segment
marks automap visibility; a separate skybox pass must not accidentally reveal the
mine. Reuse existing render, source and geometry owners. No autonomous navigation.

Stop starting features at 21:30 Pacific for final checks and restoration of the
original SwiftShader emulator. Particle branch 86 is published as draft PR #238 against cmake after explicit
user approval. CI was running at the latest check.

## Collision checkpoint

`SkyboxCollisionWorld` compares against the extracted, hash-pinned actual
CObject::HandleWallCollision body in the existing effect source owner. Windows
and Linux pass 221184 cases, including 1728 entries and 2304 retirements. Checks
cover actor types, skybox membership, side/face selection, separate side segment,
pixel and destination query arguments/order, no-hit lookup, connected/external
walls and lifetime immediately below/at/above one second. The external geometry
and pixel query outcomes are supplied; this source oracle alone does not prove
the native consumers.

Redux FVI did not retain its selected triangle. A DXX_XL_RUNTIME-only result
field now carries it without changing collision decisions; real loaded Neptune
rays passed 293 wall hits (290 face zero, three face one). The runtime skybox
collision owner compiles its lookup world from loaded geometry, uses the real
transparent-pixel consumer, and is cleared/rebound on level transitions. It has
no state when no skybox segments are present. The native physics loop calls it
only for wall hits. Full-physics retirement, indexed/RGBA transparent-pixel entry, compiled
geometry lookup and ordinary-source reset controls pass. No skybox admission bypass or new public acceptance added. The shared native
control is now invoked by one extra step in the existing Android derived-secret
owner. Android passes all 31 steps in 50.737 seconds, including the collision
receipt and subsequent real secret-save/return and GPU checks. Protected file
hashes match. Both automation catalogs pass.

Receipts: `buffer-windows.log`, `skybox-linux.log`, `skybox-quality.log`,
`skybox-face-quality.log`, `skybox-native-quality.log` under the evening root.

The shared diagnostic uses CT_WEAPON for its allocated weapon. Android debug
assertions exposed the original CT_NONE fixture setup; this was corrected and
both native and Android owners rerun successfully. All local build/test handles
are terminal. Additional receipts: `android-skybox-collision-fixed.log`,
`skybox-constructor-quality.log`, `catalog-skybox.log` and
`catalog-owner-skybox.log`. The failed diagnostic run also restored protected
files with matching hashes before retry.

## Separate textured rendering checkpoint (branch 88)

The loaded skybox owner compiles ordered textured faces and draws them at full
brightness before queued transparency, with depth writes and caller depth state
restoration. Skybox objects use a separate source-ordered pass before ordinary
geometry. Ordinary geometry and automap discovery exclude skybox segments.
Gameplay visibility retains the same robot/player order as the renderer,
including objects outside the ordinary visible segment list.

Windows and Linux actual-source comparisons pass 25725 visibility cases, 3944
second-normal distinctions, eight automap/depth gates, 18432 wall inclusion
cases and 18432 object lists with 55296 ordered calls. Native loaded binding
controls pass for 474 faces across mixed/all-skybox fixtures. Both automation
catalog checks pass; existing owners are reused.

Android passes all 31 protected secret-save/GPU steps in 54.035 seconds. Fixed
camera controls verify 1024 red and 1024 green pixels, static-light independence,
actual depth rejection/writes and state restoration, 46 native object visits
(20 outside the ordinary segment list), and exact visibility order including a
separate-segment robot. Mixed ordinary/skybox geometry preserves foreground
pixels and ordinary discovery while excluding skybox discovery. All 28 protected
file hashes match. Receipts are in temp/xl-skybox-20261031, final owner log
android-mixed-state.log. A depth-witness texture-enable cleanup bug was diagnosed
and fixed; the final pass uses no extra warm-up frame or mismatch diagnostics.

Normal skybox admission remains closed. Colored boundaries, remaining corona
interactions, full weapon entry and original authored skybox resource/save/context
lifetime still need qualification. These derived fixtures do not qualify the
original Pumo hub or campaign as playable.
