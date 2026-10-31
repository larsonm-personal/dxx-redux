# Skybox requirement audit

Status: normal skybox admission remains closed. Source-backed collision decisions
and the native collision hook are implemented; native integration passes. Investigation branch: xl_more_87_skybox,
stacked locally on published particle tip 55c937273b0633d2b5aa92e3f5fcd616400fa46c.
Branch 86 is based on cmake 0e4587e7dedbf0f21955c6fda351c923ff5e23c5.
Visibility/rendering and complete skybox gameplay are not yet qualified.

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
