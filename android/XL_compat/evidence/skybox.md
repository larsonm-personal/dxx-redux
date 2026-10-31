# Skybox requirement audit

Status: modern textured skyboxes have passed source, native and Android
resource/save/context qualification on branch 91. Admission requires an active
OpenGL renderer and retains explicit rejection of untextured colored boundaries.
Original Pumo hub remains rejected for smoke and outdoors behavior. This is not
campaign gameplay qualification. Earlier checkpoint sections below describe
the gates in effect at those checkpoints.

## Branch 91 admission and authored-resource checkpoint

Normal translation retains skybox function 10 in XL metadata and maps it to
ordinary type 0 in the classic station data. The authored test exposed the
missing translation through an invalid fuel-center type error; a regression now
checks actual translated station bytes. Shared face selection drives renderer
inclusion and the colored-only boundary gate. Independent property/effect
requirements remain enforced; the original hub rejection checks its outdoors
property rather than the superseded generic function gate.

The fixture preserves original Pumo archive members, geometry, textures and
object indices. Exactly two hub bytes change: outdoors property 16 is cleared
from segment 154 (60 -> 44), and smoke subtype 0 becomes fire subtype 3 at object

1. These substitutions isolate unqualified features and do not qualify them.
   The original archive remains unchanged. The descriptor identifies a derived
   mission, pskybox, and the protected owner stages original mod assets under that
   mission's separate directory.

Android passes 54/54 steps in 120.725 seconds, including all 295 skybox memberships,
original POG CPU/GPU pixels, distinguishable textured views, disk save/restore,
EGL context recreation, stock cleanup and restore from stock. Comparisons use
the same fixed pose; an earlier test failed after normal physics moved the
camera during its resume/context sequence. No manual owner rebind is used.
Protected file hashes match. Log: android-authored-fixed-view.log under
temp/xl-skybox-20261031; final receipts: temp/xl-skybox-authored-android.

Windows/Linux source and inventory suites, native integration, Android build,
scoped formatting and both automation catalogs pass. Existing source evidence
for collision, object traversal, visibility, wall inclusion and coronas is reused.

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

## Corona segment exclusion checkpoint (branch 89)

The actual texture-emitter API now excludes skybox and outdoors segments before
face classification, following pinned SetupCoronaFaces. Existing property
normalization distinguishes legacy outdoors function 16 from modern function 16
and modern outdoors property 16. Rejected calls clear their output.

The extracted, hash-pinned SetupCoronaFaces body passes 131072 combinations on
Windows/Linux, including visibility, doorway, light, camera and global-effect
gates, query counts and assignment order. Native and Android controls use a real
loaded emitter across 54 function/property/version combinations. Android passes
31/31 in 53.841 seconds with all 28 protected file hashes restored. Receipts:
corona-linux.log, visibility-source.log, native-runtime/controls.log and
android-corona.log under temp/xl-skybox-20261031. Scoped formatting passes;
existing automation catalogs remain valid with no new script registrations.

This fixes corona exclusion only. It does not qualify outdoors lighting or open
skybox admission. Colored mesh boundaries still need their draw-state behavior
established; the pinned RenderSkyBoxFace passes bTextured=1 to RenderFace even
when its mesh face has no base bitmap. Do not infer a new tint from mesh flags.

## Complete native physics entry control (branch 90)

The existing native/Android owner now drives do_physics_sim through an ordinary
wall into an enclosing skybox. The fixture copies an empty cell and uses eight
isolated temporary vertices; it restores segment data, linked actor ownership,
vertex bytes, count and highest index. It asserts destination membership,
retained lifetime, the source-prescribed wall-hit position, geometric containment,
and continued movement inside the skybox on the next physics frame.

This replaces an insufficient distant-cell control that checked only destination
ID and lifetime. Pinned ProcessOffset retains hitResult.vPoint, and FindSegment
returns a lookup result without relinking, so segment ID alone was not enough.
Native integration passes; Android passes all 31 protected steps in 54.820
seconds, with all 28 hashes restored. Receipt: android-full-entry.log under
temp/xl-skybox-20261031. Scoped formatting and native/Android builds pass.
No production physics semantics changed. Authored skybox resource/save lifetime
and normal admission remain unqualified; this is derived geometry evidence.

## Outdoors lighting source audit (branch 93)

Correction to the earlier source map: dynlight.cpp Contribute's explicit
outdoors destination-face test is inside disabled #if0 code. Active Add records
bSelf for outdoor face lights, and lightprecalc's light-to-segment visibility
uses it. However, method-0 GatherStaticVertexLights passes destination segment
-1 through SetNearestToVertex; Contribute returns after its range check before
consuming that visibility table. It must not be applied to the ambient vertex
visibility table as an additional filter.

Active Create sets bAmbient for outdoor base/overlay lights. The active
ogl_color path restricts these lights to source-face vertices and shades them
at zero distance. Existing ambient gathering implements that behavior; the
runtime material compiler now receives normalized outdoors flags instead of
constant false. Create also excludes skybox segments from geometry light sources;
the runtime compiler now does so as well. Baked registered method-0 colors remain
handled by the existing source-prescribed rebuild gate.

Validation so far: 98 pinned Create face-selection cases on Windows/Linux;
54 loaded legacy/modern flag cases; four loaded ordinary/outdoor material cases;
exactly four lit source vertices with all vertices marked visible; disabled
source clears all colors; skybox binding removes the source. Final native owner
passes. Source selection oracle records Add requests and models positive-light
creation/update ordering; it does not execute the complete Add implementation.
Android build/test is pending. Outdoors admission remains closed at this point.
Receipts: outdoors-material-windows.log, outdoors-material-linux.log and
native-runtime/controls.log under temp/xl-skybox-20261031.

## Original Pumo hub lifecycle qualification (branch 93)

Normal modern admission now recognizes outdoors property16. Unknown bit128
remains rejected, including the existing Neptune live-state preservation owner.
The original pmines HOG, descriptor and mod package pass the existing smoke/
skybox lifecycle with no changed mission bytes. Segment154 retains properties60
and its outdoors flag across restores. Android passed68/68 in121.407seconds:
live smoke and scene pixels,64atlas cells,295skyboxmemberships,POGresources,
disk restore,EGLcontext recreation,stock cleanup and return. Protected hashes
match. This qualifies the tested hub lifecycle, not campaign traversal.

Outdoors controls also passed Android31/31 in55.112seconds. Final Windows/Linux
inventory suites pass, including all256property combinations and source-owned
outdoors translation. Native integration passes with the fire-only owner
explicitly rejecting the missing smoke dependency; complete hub assets are
covered by Android. Updated Neptune negative/recovery owner passed65/65 in
79.044seconds with matching protected hashes. Its changed negative fixture uses
unknown128, not the now-supported outdoors16. Both automation catalogs passed.

Receipts: android-original-hub.log, android-outdoors-controls.log,
android-hub-neptune-regression.log, admission-test.log, hub-linux-inventory.log
and native-runtime/controls.log under temp/xl-skybox-20261031. The lifecycle's
final screenshot contains GPU diagnostic pixels and is not a natural gameplay
image; a separate fixed-view scene capture is being prepared for that purpose.

Clean fixed-view smoke scene passed17/17 in54.835seconds with matching protected
hashes. Inspected temp/xl-original-hub-scene/render.png: live textured hub view,
no GPU diagnostic grid and no pause overlay. This is a close fixed view of the
smoke area, not a campaign walkthrough or broad hub overview. Native/Android
builds and full68step lifecycle are reused; scene-only changes affect the owner.
