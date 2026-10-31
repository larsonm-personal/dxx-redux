# Authored particle effects: fire runtime and qualification

Current status: strict authored fire now uses normal source-owned admission;
its former diagnostic bypass is removed. Normal-admission native and Android
validation passes. Simulation, geometry, resources, saved activation,
source buffering and soft rendering have passing source/native/GPU evidence.
Original-Pumo controlled level-2 lifecycle and derived secret-save owners pass.
Other particle kinds, moving/controlled/multiplayer effects and triangular
emission faces remain rejected. Pumo's hub and campaign are NOT qualified playable.
Work is committed on `xl_more_86_particle_rendering` through `55c937273`, based
on current cmake, published as draft PR #238 after explicit user approval.
Checkout: C:/local/dxx-redux.
The sections below record successive checkpoints; later checkpoints supersede
earlier lists of unimplemented work. Current process state and remaining actions
are in `android/ai tool plans/mission metadata/xl_continuation_20261030.md`.

## Source and established behavior

Reference is `arbruijn/d2x-xl` revision
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The source adapter extracts and
executes `CObject::SetupSmoke`, `DoStaticParticles`, `MaxParticles`,
`ParticleSize`, the fixed multiplication macro and float rounding function.
Consumed files and hashes are pinned by `tests/reference_particles.cmake`.
Serialized particle constants come from the source header; emitter image IDs
are a different numbering system.

The existing effect-reader integration now covers 9,072 preparation cases and
18,144 emitter-definition cases, including:

- Six level versions, all seven authored subtypes, point and face emitters,
  zero/small/unit/large face sizes, legacy water-texture conversion, default
  parameters and authored parameters through 10.
- Both disperse profiles, the source density cap, fixed direction, color and
  default color, lifetime, brightness, fade type and bubble-sound request.
- Existing-owner reuse and disabled-owner destruction in the actual source.
- Explicit rejection of arithmetic overflow, undefined density/shift ranges,
  invalid byte values, invalid sides and unknown subtypes.

Face size, water classification, object storage, renderer creation and audio
requests are adapter inputs/captures. These comparisons do not yet qualify real
face geometry, emitted particle motion, random distributions, audio output or
pixels. Point-emitter jitter currently receives controlled zero random values;
it is not qualified by the emitter-definition comparison.

Source `DoStaticParticles` compares a remapped type against serialized type
constants for its density divisor. The implementation preserves the executed
behavior, including the resulting distinction between fire and waterfall.
Do not simplify those comparisons based on the names alone.

An initial fixed-vector adapter used arithmetic shifts. Auditing the pinned
math macro showed that XL actually uses double multiplication and truncation
toward zero. Replacing the adapter with the actual source macro made the old
production direction fail (`Actual source particle emitter definition differs`).
The corrected implementation passes the same comparison. The failed and passing
receipts are retained as `math-negative-linux.log` and
`math-corrected-linux.log` under `temp/xl-particles-20261030`.

Linux/GCC and Windows/x64 MSVC source comparisons pass. The first Windows/x86
standalone configuration stops on existing `/WX` narrowing warnings in unchanged
`audio_voice.cpp`; that attempt is retained in `build-windows.log`. It is not
counted as a successful x86 build. No unrelated audio code or warning policy
was changed. This checkpoint has no D1/D2 or Android application changes and
adds no CI workflow or duplicate integration owner.

## Original Pumo dependency

The existing original inventory identifies twelve particles in `mvlev1.rl2`,
objects 49-60. All have serialized subtype 3 (fire), use a face emitter and start
disabled. They require actual authored activation and visible fire behavior;
loading them as inert invisible objects would not qualify the level. The
thirteen diagnostics are twelve object records plus one category finding.

The downloaded `d2x-xl-win-1.18.77.exe` is a self-extracting archive. It was read
as an archive, not executed. Its original `textures/d2x-xl/smokingfire.tga` has
SHA-256 `b7bd821c02ba917a4cb0ff503f2e83d4e2007c19f0450728525df2f3af7f738f`.
Files are extracted only under the local artifact directory. No original art
has been added to Git or silently substituted with generated art. Resource
lookup, decoding, image ownership and distribution remain to be established.

## Face geometry and authored fire checkpoint

The existing effect-reader owner now also executes pinned `FaceSize`,
`TriangleSize`, `RandomPointOnTriangle`, `RandomPointOnQuad`, `SetupCorners`
and `GetCornerVertices`. Seventy-two face profiles exercise classic density
ordering separately from explicit emission ordering; 7,056 surface samples
check both triangles, reflected barycentric weights and individual fixed-point
rounding. A 20-by-20 world-unit quad has density area 1. Source triangular-face
sampling receives an explicitly initialized missing-corner sentinel: the pinned
`GetCornerVertices` second loop never writes one. No runtime triangle admission
is being widened on the strength of that controlled input.

The original Pumo archive passes the same geometry comparisons for 106 face
emitters across its fourteen levels, including all twelve opening-level fire
objects. Modern authored parameters are converted using those actual face
areas. These are source/asset comparisons, not an engine loading or playtest
claim. The receipt is `pumo-geometry-linux.log` in the artifact directory.

`particle_fire.h` implements authored fire creation and lifetime/motion updates.
The actual active `CParticle::Create`, `Update`, `UpdateDrift`, `Bounce`,
`ChangeDir`, `Drag`, `UpdateDecay`, `UpdateColor`, `SetupColor` and atlas-coordinate
consumers run in the source adapter. The comparisons cover 2,304 creations and
41,472 updates: point/face origins, zero and non-axis directions, random draw
order, both fire/smoke cells, life/size variation, decay, lookup timing, missing
segments, supplied wall normals, portal results, collision disabling and death.

Limits of this checkpoint:

- Random inputs are controlled, with matching call order and values. Engine FX
  stream isolation and pause behavior still require runtime checks.
- Segment lookup and wall collisions are supplied outcomes. Actual source
  side-plane classification, compiled mine geometry and exhaustive lookup are
  not yet qualified for this particle consumer.
- Orientation-matrix construction is a storage adapter because authored fire
  billboards do not consume it. Its two random draws are checked. The source's
  early `RenderType` call reads frame fields before initializing them; the
  comparison does not bless that pool-state-dependent render classification.
  Actual rendering still needs an explicit, verified policy.
- Fire color is white with additive-emission classification even for authored
  orange/transparent colors. Pixel output and blend state remain untested.
- Particle creation/lifecycle support currently implements fire. Other subtype
  definitions are decoded, but their simulation is not implemented by this work.

The inverse-cosine table, normalized angle and fixed dot are now shared by fire
and spatial audio in `fixed_angle.h`, instead of duplicating that implementation.
The existing actual-source audio owner still passes after the extraction.
No new CTest owner or workflow was added. This is reuse, not yet a measured CI
optimization. All new production changes remain inside `android/XL_compat`.

Linux/GCC and Windows/x64 MSVC both pass `xl_effect_decode_reference` and
`xl_named_audio_source` after formatting. The source comparisons use the existing
standalone XL CMake project with `XL_REFERENCE_GEOMETRY_ROOT` pointing to the
pinned source tree. Build targets are `xl_effect_reference`, `xl_inventory_test`
and `xl_named_audio_source`; run:

```sh
ctest --test-dir <build> -R '^(xl_effect_decode_reference|xl_named_audio_source)$' --output-on-failure
<build>/xl_effect_reference <original-pmines.hog>
```

Next: real world geometry, emitter scheduling, image/blend consumers, engine
activation/deletion and save behavior. Admission remains unchanged until those
consumers and platform controls pass.

## Loaded world geometry checkpoint

The foundation above is published in draft PR #229 at `3b7c6cbb6`. Continuing
work is in the main checkout on `xl_more_79_particle_world`.

`ParticleWorld` implements fixed side-plane containment, exhaustive lookup with
the source segment checked first, and particle wall/portal classification. Its
Linux and Windows source comparison executes actual `CSide::Masks`, `CSegment::Masks`,
`PointInSeg`, `FindSegByPosExhaustive` and `CParticle::CollideWithWall`, using the
already extracted vector functions. It passes 10,920 lookups and 5,460 collision
cases, including the different strictness at the negative plane tolerance,
two-face concavity, single-face sides, skybox filtering and portal transitions.

The native `compile_particle_world` reuses the existing XL classic-side compiler
and verifies that vertices and connectivity match the loaded mine. It snapshots
the particle-specific plane anchors, concavity, radii, skybox classification and
spatial grids without changing Redux geometry. Stale inputs with matching counts
reject. Native checks also preserve both simulation and effects RNG call counts.

The existing `xl_effect_inspection_gate` now exports raw vertices/connectivity,
compiled fields and actual native query results for stock D2 level one and
original Neptune. The stock control supplies loaded raw geometry because the
XL inspector deliberately excludes stock version 6; it does not supply Redux's
computed normals, centers or radii to the source compiler. The source adapter
executes pinned `SetupCorners`, `SetupAsTriangles`, `SetupVertexList`,
`SetupFaceVertIndex`, `Height`, `IsPlanar`, `Dist`, `ComputeCenter`, `ComputeRads`,
normal ordering and the actual integer normal math. Unconsumed float lighting
normals and per-side render bounds are storage adapters, not rendering evidence.

Windows/x86 native execution and Windows/x64 and Linux source comparisons pass
321 compiled segments and 6,099 native lookup/collision queries. Changing one
exported fixed normal caused the independent comparison to fail at the changed
segment and side. These loaded-world results cover stock and Neptune, not a
loaded original Pumo level. The original Pumo face/particle inventory comparison
still passes for all fourteen levels; it does not establish gameplay support.

The grid comparison additionally executes pinned `CSegmentGrid::Create` and
`GetSegList` from `main/gamedata.cpp` (SHA-256
`782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12`). It checks
40-unit ordinary and 80-unit skybox grids, fixed division followed by flooring,
flattened-index boundary behavior, and every candidate list in the loaded
receipts. Production stores occupied cells sparsely but preserves source
candidate ordering. Supplied-geometry cases exercise both grid and no-grid
branches. Particle frame performance and memory use in Pumo remain unmeasured.

Reproduce the native/source integration through the existing CTest owner:

```sh
ctest --test-dir <private-build> -R '^xl_effect_inspection_gate$' --output-on-failure
<reference-build>/xl_effect_reference --particle-world <private-build>/effect-inspection/particle-world.txt
```

The private build must configure `XL_NEPTUNE_HOG`, `XL_NEPTUNE_DESCRIPTOR`,
`XL_RETAIL_DATA`, `XL_PREVIEW_LIFECYCLE` and `XL_REFERENCE_GEOMETRY_ROOT`.
The required originals runner now requires the effect source oracle argument;
omitting the oracle cannot silently count as passing that required owner.

### Effect gate setup cost

The owner previously started three native processes, one for each format
23/25/27. It now initializes the engine once while retaining all six stock/XL
rejection checks, six successful initial loads, three supported recovery loads,
and explicit per-version witnesses. The new geometry receipt runs once.
There is no new CTest owner or workflow.

Three sequential local Windows measurements before/after consolidation, at the
same pre-grid geometry stage, were 6.826/6.310/6.361 seconds and
5.603/5.104/5.717 seconds. Median setup consolidation saved 0.758 seconds, about
12 percent for this owner. This is a local measurement, not a claim about total
CI duration. The later grid comparisons add coverage and have separate cost.

Emitter scheduling, image resources, rendering, activation/deletion and
save/load still need implementation and integration checks. No runtime particle
admission or drawing is enabled by this checkpoint. All production changes in
this layer remain inside `android/XL_compat`.

## Emitter scheduling and engine ownership

On `xl_more_80_emitters`, `FireEmitter` now owns particle storage, source emission
rate/tick accumulation, capacity, interpolation and ring retirement order. The
existing fire adapter executes actual `CParticleEmitter::Create`, `Update`,
`SetLife` and `IsAlive`, reusing the actual particle creation/update and vector
math already above. Pinned `effects/particle_emitter.cpp` has SHA-256
`0a92df8997332d46abc2132b3f91e5831958d0a63ac64a8cdf6594318d7ae3a5`.

Linux and Windows/x64 pass 2,880 frame comparisons, 24 drained rings and six
zero-capacity rejections. Coverage includes irregular and repeated timestamps,
capacities 1/2/7/64, face and point emitters, movement interpolation, collision,
failed segment lookup, disabling and re-enabling, and matching live particle
fields, random draw order and world call counts. This matrix supplies world
outcomes explicitly. A separate composed comparison now connects the particle
and emitter bodies to actual source geometry, as described below.

The source retires dead particles while shrinking the retirement loop bound;
some dead entries remain until another update. Preserve that behavior, including
its consequences for capacity and RNG order. A particle Update false return
alone does not retire a particle. Fresh zero-capacity CArray allocation returns
null; the storage adapter was corrected after auditing `include/carray.h`, pinned
at `10ea5f725a079e9be98f7871ffa6544d1de5c427db472b214b3f91188719019c`.
Owners must skip zero-capacity construction, as XL does when creation fails.

The emitter adapter supplies authored-effect class zero and fire brightness one.
Unconsumed orientation matrices remain storage adapters. These checks cover CPU
scheduling, not resource loading or image/render fidelity.
Source `KillObjectSmoke` disables the old emitter and removes the object's system
association; enabling again creates a new system while the old particles drain.
The engine owner preserves that distinction and object signature checks.

The source also executes `DoStaticParticles` twice per ordinary effects frame:
once through `ObjectParticleFrame`, then through `StaticParticlesFrame`. Face
emitters keep their position; point emitters consume both jitter visits. The
existing `DoStaticParticles` source adapter now compares 1,152 point positions
and random-call sequences, including the zero-spread fallback and face no-jitter
controls, on Linux and Windows/x64.

`runtime/particle_objects.cpp` binds immutable fire definitions and loaded world
geometry, checks native identity/signatures, preserves ascending authored visits
and newest-first system updates, and retains draining systems after disabling.
It matches the 10,000-system source pool limit and the 25-ms update gate that
drops elapsed residue. The source capacity constant is extracted and asserted.
The effects clock uses SDL milliseconds relative to level binding; pause skips
updates and resume includes elapsed wall time, without consuming simulation RNG.
The gate begins at the level epoch, rather than inheriting XL's global tick phase.
Full pause/menu behavior and save restoration still require platform integration.

One intentional ownership correction differs from pinned XL: destruction of an
old draining system only clears the object's association if that association
still points to the old system. Pinned `CParticleSystem::Destroy` clears it
unconditionally, which can orphan a replacement created after re-enabling.
The native control verifies that the replacement remains bound after drainage.

The existing native effect gate injects two authored fire objects into loaded
Neptune geometry and executes the production owner. It checks wall and automatic
object-trigger activation, initial disabled behavior, 25-ms cadence, draining and
fresh re-enable, signature reuse, pause, two point-jitter visits, failed binding
without partial replacement, explicit cleanup and unchanged simulation RNG.
Stock hooks reject activation and consume neither RNG stream. The original
six rejection controls and three supported recovery loads remain in the same
process. These are controlled injected effects, not an admitted original Pumo
level and not visible-rendering evidence.

The existing loaded-world receipt additionally drives the actual source emitter
and particle bodies against independently compiled source lookup/collision code.
Production uses the native-exported world. Each of stock and Neptune passes 720
frame comparisons and six drained rings, with 15-bit random inputs matching the
native FX range. Stock exercises 882 lookups/836 collisions; Neptune exercises
881/834. Linux and Windows/x64 agree. This composed matrix uses only capacity 7;
the supplied-world matrix already covers other capacities and zero allocation,
so those controls are not duplicated. No new native startup, CTest owner or
workflow was added.

Windows/x86 native build and the registered `xl_effect_inspection_gate` pass.
The production implementation stays in `android/XL_compat`, except for five added
lines in `d2/main/game.c`: one include and a guarded post-gameplay frame call.
The existing trigger-frame hook runs before object movement and would delay
movement-triggered activation until the next frame. D1 and Android application
sources are unchanged.

No runtime particle admission or drawing is enabled. Resource/image ownership,
visible output, saved authored state and Android lifecycle checks remain next.

## Particle image preparation work in progress

`xl_more_81_particle_images` adds a bounded raw 24/32-bit TGA decoder, reusing
the existing owned bitmap and XL RGBA alpha/key normalization. The pixel
normalizer now accepts width 4096, which the source TGA header permits; packed
POG headers still cannot encode that width. The reader preserves the source's
`yStart` row/channel convention, including its unused descriptor bits, rather
than silently substituting another decoder's orientation policy. The 24-bit
source path remains opaque and does not apply the RGBA transparency color key.

The existing POG source owner now also executes actual `CTGAHeader::Read` and
`ReadData`/`SetProperties` against particle images. Linux and Windows/x64 pass the
synthetic header/row/channel/alpha comparisons and fourteen malformed or
unsupported-input rejections; existing POG checks still pass. The user's original
128-by-128, 24-bit `smokingfire.tga` matches all 65,536 decoded RGBA bytes on both
platforms. The original art remains private and is not committed.

The existing POG source owner also executes `LoadAddonBitmap` and the particle
image manager's active quality-2 table and `Load` body. It verifies the atlas
name, two cells per axis, nonanimated policy, one-pixel borders, mod-before-base
lookup, failed present-mod rejection without fallback, and cached successful
loads. Bitmap allocation and filesystem reads are adapters; this is not GPU
upload evidence. These comparisons pass on Linux and Windows/x64.

Native preparation now owns decoded pixels in the prepared level resources.
It checks `mods/<selected-hog>/textures/d2x-xl/smokingfire.tga` before
`textures/d2x-xl/smokingfire.tga`. Mission images must belong to the selected
mission; the default may also come from the captured startup HAM installation.
Malformed present images reject instead of falling back. Preparation currently
qualifies square power-of-two fire atlases only; other layouts reject explicitly.
No particle objects means no image read or allocation.

The existing native effect gate passes mission/default precedence, both forms
of foreign shadow rejection, case-insensitive lookup, missing/malformed inputs,
retained pixels after file changes/removal, failure recovery, and unchanged
current prepared resources. Its final Windows/x86 run passed in 6.02 seconds,
including the previous CPU owner, rejection/recovery and loaded-world controls.
The separate startup-installation fallback and Android resource mounts still
need platform witnesses. No new CTest owner, workflow or core source edit was
added for this layer.

The original atlas requires distinct render behavior: source `RenderType`
selects fire versus smoke cells, and `CParticleBuffer::Flush` uses emissive
blending for the former and multiply for the latter. GPU upload, visible
rendering, saved authored state and Android lifecycle remain unqualified.
Particle admission stays blocked; resource preparation is not rendered fidelity.

## Fire billboard and GPU backend checkpoint

Image preparation is published in draft PR #233, commit `9c2d1bd67`, stacked on
#232. Rendering work continues in the visible `xl_more_82_particle_render` branch.

Pinned `TRANSFORM_PARTICLE_VERTICES` is zero: the active `CParticle::Setup`
constructs world-space corners from fixed camera axes. The new billboard
consumer passes 1,536 exact source comparisons on Windows/x64 and Linux for
corners, UV orientation and one-pixel borders, lifetime scale, supplied rotation
axes and policy, white color/alpha control, and fire versus smoke selection.
Camera matrix construction and projection are supplied inputs in this owner;
they still require native integration evidence.

The source emitter clears its particle storage. `Create` chooses `m_bRotate`
using the preceding slot's frame before assigning the new frame: fresh slots
rotate, while recycled slots depend on the old atlas cell. This is defined
reuse behavior, not uninitialized memory. Production now retains that rotation
choice and the existing creation/emitter comparisons verify it, including
drained/reused rings. Rotation advancement during rendering is not yet connected.

The XL-local drawing backend accepts ordered camera-space quads. Actual pinned
`CParticleBuffer::Flush` blend selection and `COGL::SetBlendMode` dispatch verify
that fire uses `ONE, ONE_MINUS_SRC_COLOR` (weak additive), while smoke uses
`DST_COLOR, ZERO` (multiply). The existing EGL/POG owner checks the production
RGBA upload and particle draw with complete synthetic-atlas pixel comparisons,
wall depth rejection, no scene-depth writes, and restoration of blend/depth/cull,
texture binding and alpha-reference state. Mixed fire/smoke draws also check
that the backend preserves the caller's ordering. These are controlled GPU
inputs, not original-mission scene captures or Android rendering evidence.

The Windows/x86 native engine builds with the backend; the existing effect gate
passes in 5.37 seconds. No core D1/D2/application source or new test startup was
added. Live image/GL lifetime ownership, camera construction/projection,
visibility and transparent ordering, render animation, save restoration and
Android/original-Pumo integration remain to be done. Particle admission remains
blocked throughout this checkpoint.

## Scene integration checkpoint

`xl_more_83_particle_scene` connects the prepared image, live camera and authored
particle owner through the existing XL emitter render hook. This is still a
development path behind particle admission rejection. It does not admit Pumo.

The existing actual-source effect owner now covers these previously supplied
consumers on Windows/x64 and Linux:

- 1,152 camera rotation cases execute `FixSinCos`, fixed matrix construction and
  multiplication, and `CParticle::InitRotation`/`SetupRotation`
- 840 render/lifetime transitions and 24 animation-clock cases execute
  `CParticle::Render` and `CParticleManager::BeginRender`. Source queues first,
  then advances rotation; the active delayed `Setup` reads the live rotation
  member, so the produced quad uses that advanced value. Life zero still draws;
  negative life does not
- Eight depth-limit/viewer profiles compare 2,119 ordered entries against actual
  transparency initialization, insertion and bucket traversal. Distance is fixed
  Euclidean distance, biased by ten units only when it is at least ten units;
  equal-bucket insertion reverses input order. This is the single-thread source
  profile, not a claim about every source batching/thread configuration
- 4,608 emitter and 5,120 particle selections execute original visibility
  functions, including scratch-state construction/zero initialization, flag
  rollover, transparent/blocked links, adjacent visible segments, failed and
  successful relocation, and lookup call counts. The pinned manager uses
  `visibility[nThread + 2]` for the emitter query; its local visible flags stay
  clear, independently of the global visible segment list. Production preserves
  that default scratch policy. Particle adjacency itself does not test walls
- 255 visible-scene depth limits execute `CVisibilityData::InitZRef`, including
  its float center/viewer distance and fixed outer radius. Empty production
  scenes have no candidates; invalid indices and depth overflow reject

The native scene owner obtains the visible segment list and current camera from
the engine, collects source-visible particles, orders them, advances render
animation at the source 33-ms cadence, and constructs the quads. Its existing
injected-Neptune integration control verifies visible/empty scenes, the 32/33-ms
boundary and unchanged simulation/effects RNG. Stock calls return without
allocating a scene or consuming RNG. The native Windows/x86 build and registered
effect gate pass; the final gate took 6.23 seconds. These controls still use
injected effects, not original Pumo gameplay or a rendered native capture.

Prepared images now own their GPU texture as well as decoded pixels. Upload is
lazy; empty draws allocate nothing. Six added lines in `d2/arch/ogl/ogl.c` notify
this owner before texture-pool initialization and normal/lost-context cleanup.
Without that notification an image can retain a pointer to a pool slot later
reused by an unrelated bitmap. The implementation and tests otherwise stay in
XL_compat; D1 and Android application sources are unchanged in this layer.

The existing EGL owner passes 26 atlas/blend/depth/order cases, including equal
depth under the source `LEQUAL` policy, regular release/reload and destruction.
Twelve further cases independently compare CPU world-to-camera conversion with
OpenGL matrix-stack projection. A real EGL context is then destroyed and
recreated while decoded particle pixels remain owned; an unrelated bitmap
occupies the reused pool before particle reload. Pixel output, separate texture
ownership and destruction all pass. This tests desktop GL lifecycle, not an
Android EGL-loss event. The final GPU owner took 0.49 seconds and retains its
existing indexed/POG controls; no new CTest owner, workflow or engine startup
was added.

Remaining rendering qualification is significant: the current hook draws after
the engine's other transparent content. Particle-to-particle ordering is checked,
but this does not establish correct interleaving with transparent walls, sprites
or other objects. Source buffer batching and complete original-scene output are
also unqualified. Saved authored enable state, startup-installation image fallback,
Android resource/graphics lifecycle and the original Pumo package still need
integration evidence. These requirements continue to block admission.

## Saved authored particle state

`xl_more_84_particle_saves` adds particle owner rows to development trailer schema 26. Each row carries the authored object index, native signature, enabled flag
and live/deleted binding state. The common authored-source identity now includes
particle definition bytes, position and orientation. Missing, extra, reordered,
ill-typed, out-of-range or mismatched rows reject during the trailer's validation
phase, before any owned state is applied. Schema 25 is replaced, consistent with
the pre-release XL save policy; this is not an XL-native save importer.

Pinned `CObject::SaveState` writes only the particle enabled byte in its render
payload. `LoadState` reads it from version 49 onward and defaults it for earlier
versions. These exact branches now execute in the existing source effect owner;
both Boolean values and six version/state restores pass on Windows/x64 and Linux.
The source restore path calls `PrepareLevel`; its `CleanupBeforeGame` destroys
game data and shuts down the particle manager. That lifecycle connection is a
source audit, not execution of the whole XL save manager. Production restores
the authored state and starts fresh transient particle systems and visual clocks.

The existing native effect owner now round-trips the complete XL trailer with a
live particle cloud and mixed enabled/disabled authored objects. Twelve corrupted
trailers test rejection without changing recaptured trailer bytes or the live
cloud. Successful restoration discards old clouds, creates only the enabled
owner, resumes emission and preserves both RNG streams during restoration;
subsequent emission uses only effects RNG. Deleted owners remain dead, and a
recycled native identity cannot inherit a dead binding. Native Windows/x86 build
and effect integration pass (6.92 seconds). No core or Android application code
changes or new test owner were needed.

The existing comprehensive `xl_named_audio_runtime` owner also passes with
schema 26 (112.18 seconds), including ambient lifetime/trailer rejection and
seismic disk-save-through-stock controls. It was run because the common trailer
changed, rather than repeating the unrelated GPU or classic replay suites.

This is native trailer/lifecycle evidence with injected effects. A complete
original-Pumo save/load, Android save restoration and cross-level/secret-return
qualification remain outstanding, along with the rendering/resource gates above.

## Shared scene transparency

`xl_more_85_transparency` replaces the after-scene particle pass with a shared
XL frame queue. Small hooks in `render.c` and `object.c` retain native face and
object rendering; collection, copied face inputs, owner signatures, view bounds,
dispatch and cleanup live in XL_compat. Classic levels return before collecting.
Transparent water portals join the same queue. Deferred objects execute the
inner renderer once, without replaying outer visibility/AI bookkeeping.
Particle preparation retains original insertion order; only adjacent particles
batch together, so batching cannot cross a wall/object boundary.

Actual pinned `AddObject`, `AddSprite`, `AddParticle` and `AddPoly` bodies now
execute with the existing insertion/bucket traversal oracle. Adding polygons
caught a missing distinction: faces clamp to the maximum depth, whereas other
items drop. Particle bias remains ten units, other items zero, and equal buckets
use reverse insertion order. Sixteen profiles compare 5,733 ordered entries,
including faces outside the depth bound. Actual weapon table assignments and the energy
predicate match all 256 IDs. The actual `FaceIsTransparent` body matches 972
bitmap/forced/additive/alpha cases. Native POG installation now retains the
source see-through bit; existing original-pixel and collision/destructible
texture controls also cover that retained flag.

The existing native effect owner verifies classic isolation, mixed tie/bias
dispatch, duplicate geometry-pass suppression, copied vertex/UV/light data,
restored render context, recycled object identities, replacement views and
cleanup after interrupted dispatch. These controls observe production queue
dispatch without a graphics context; they do not prove a fully rendered level.
Source render animation advances only after distance admission. A source-call
audit disproved the earlier suspicion that rejected particles should advance;
the native control now explicitly checks that they do not.

The existing EGL owner adds 24 mixed scenes using the actual merged-wall draw
and particle renderer. Full color and depth comparisons cover both particle
blend modes, front/back overlap, biased ties, super-key holes and opaque
see-through cutouts. The final source plus GPU checks passed in 3.46 seconds on
Linux. Windows native effect and original POG integration passed in 7.57 and
10.15 seconds respectively. No new
CTest owner, engine startup or graphics context was introduced.

The normal Android debug build succeeds. Original Neptune passes all 65 checks,
including POG GPU residency after restore, the stock Counterstrike transition,
and actionable admission errors. Its negative fixture had become stale: blocked
segments are now supported. It now uses unsupported outdoor segments and checks
that rejection while generating the fixture, so future support changes fail with
an explicit fixture-maintenance message.

The cloak Android owner exposed another stale observation: its semantic audio
aliases still read Redux counters after XL moved to its own mixer. XL now records
per-sample starts, completion, stops and failures in the actual voice consumer;
the bank preallocates receipt storage outside the callback. Android aliases select
the active mixer and its physical-sample mapping. The existing PCM owner checks
completion versus stop, replacement, looping, mute and failed allocation; the
native audio integration checks mapping through the shared bank and passes in
127.75 seconds. These are diagnostics, not saved gameplay state. Android water
passes 51 steps; cloak passes 81 including actual draw alpha, cue completion,
disk/rewind and the stock transition. A separate intermittent checkpoint failure
also occurs in Arena. Failure-only buffer capture and checksum-validated trailer
comparison identify exactly two changing audio playheads (+512 samples), with
all other state and bytes outside the XL trailer identical. The test stopped
simulation time but did not freeze the asynchronous mixer between its detached
capture and synchronous reference. A scoped XL audio pause now brackets that
comparison and preserves the caller's pause/pending-resume state, without
excluding any saved fields. Native real-callback integration passes in 114.76
seconds, covering unpaused, paused, pending-resume and nested capture scopes.
The corrected Android build passes Arena's 58 steps (original GPU pixels,
disk/rewind, context loss and stock transition) and the complete cloak owner
again (81 steps). Temporary buffer dumping/delay instrumentation
has been removed; the forensic receipt is `checkpoint-difference.json` in the
tranche artifact directory.

This closes the separate-pass architecture gap and qualifies the existing
Android wall/water/POG regression scenes. Original Pumo particle scene output,
Android particle image lifecycle/save restoration and startup-installation image
fallback still need verification.
Source advanced buffer reordering, wider particle types and full campaign
behavior remain outside this fire checkpoint. Particle admission is unchanged.

## Distinct startup installation image fallback

The existing native effect owner now initializes from disposable copies of the
retail base files, separate from its mission directory. The image control verifies
that the fallback origin is trusted by startup HAM ownership and is not owned by
the selected mission. It checks decoded installation pixels, mission default/mod
precedence, malformed mod rejection without fallback, foreign default shadows,
recovery, malformed/missing installation files and retained decoded pixels after
file removal. Original retail files are not modified. No production behavior or
admission policy changed.

Windows/x86 native build and the existing xl_effect_inspection_gate pass (6.09
seconds), retaining its loaded-world, rejection/recovery and lifecycle controls.
Scoped formatting and lint pass. Receipts are under temp/xl-evening-20261030;
the detailed owner log remains in contracts-windows64/effect-inspection under
temp/xl-particles-20261030. This closes the startup-installation test gap above;
Android particle rendering/resource lifetime and original-Pumo save qualification
remain open.

## Android retained particle image and draw checkpoint

The existing Arena POG Android owner now also stages the unchanged original XL
fire atlas, checks its pinned SHA-256, and prepares it through production particle
resource lookup. A renderer-only probe compares eight GPU samples per invocation
against decoded atlas pixels and both source blend equations. It repeats after
checkpoint restoration, disk restoration, actual EGL context recreation and a
disk return through stock D2. The complete owner passes 65 steps (89.838 seconds)
and restores its backed-up app state. Android debug build, scoped quality checks
and both automation catalogs pass. Receipts: temp/xl-pog-android and
temp/xl-evening-20261030/android-pog-particle.log.

The probe deliberately retains its image across these transitions and releases
it at completion. It does not claim that Arena contains authored particles or
that loaded-level particle ownership/save state is qualified on Android. Constant
pixel-center UVs isolate Android upload/blending; billboard geometry and atlas
coordinate generation retain their earlier independent source/desktop evidence.
Normal particle admission remains rejected. Original-Pumo loaded ownership,
authored activation and save restoration are the next gate.

## Fire object translation qualification

The source-to-native translator previously stripped owned ambient-sound payloads
but not particle payloads. An explicit diagnostic fire translation entry point
now retains authored particle data in its source inventory and emits the audited
native object header with normalized ID and RT_NONE. Normal admission remains
unchanged. Strict shape, parameter, segment and quadrilateral-face validation is
shared with native particle binding; other subtypes, movement/controllers,
multiplayer objects and unqualified triangular emission remain rejected.

The existing effect-layout owner passes 125 profiles and 3,360 truncations, now
including modern fire header preservation and negative translation controls.
The native effect owner passes after sharing the validator (5.76 seconds).
The unchanged original Pumo archive (SHA-256
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043) passes
diagnostic translation of mvlev1.rl2 with its twelve disabled authored fire
objects. The test indexes the archive and reads only that bounded level payload.
Receipts: temp/xl-evening-20261030/translation.log, pumo-translation.log and
effect-shared-validation.log. This is not yet native original-level loading,
authored activation, disk persistence or gameplay qualification.

## Original package admission findings

The original descriptor starts at pm-hub.rl2. The twelve-fire scene mvlev1.rl2
is ordinal 2, not the campaign startup level; earlier references to its
"opening level" describe the first particle target and must not imply campaign
startup qualification. The unchanged hub is independently rejected for modern
segment-function 10 and other unsupported requirements (298 reported blockers
in the bounded admission message). No hub exception has been added.

An explicit fire qualification profile now connects strict diagnostic translation
to the real loader's ordinary resource and dependency checks. It is disabled by
default and cannot be changed with a loaded source or running game window.
The headless original-package test reaches level 2's visibility gate and correctly
rejects it because XL fog requires an active OpenGL renderer. Native loading or
disk-save success is not claimed. Original-Pumo loading and persistence must be
qualified on Android with its real renderer; the headless owner retains both
hub and fog rejection controls. The original HOG and descriptor are hash-checked
before and after the test, and mounted read-only; the atlas is copied only into
the disposable installation fixture.

The final native build and extended admission owner pass, including both specific
rejection witnesses and unchanged original hashes. Scoped formatting/lint pass.
Receipt: temp/xl-evening-20261030/effect-original/controls.log.

## Original level-2 Android ownership and persistence

The existing POG owner accepts a PumoMods mode and owns the support script
test_xl_pumo_particles.jsonc. It stages the original hash-pinned archive,
descriptor and fire atlas plus the package's mod directory, and restores its
protected app state after execution. No new top-level integration owner was added.
An explicit diagnostic start selects original ordinal 2 and keeps the unsupported
startup hub rejected. It uses the existing one-shot intro-skip API to avoid
reentering automation through briefing UI.

On the host GPU emulator backend the complete original-level owner passes all
60 steps in 197.724 seconds. It verifies twelve initially disabled/alive source
owners, actual prepared atlas GPU samples, original authored enable-trigger
dispatch, observed live emission, rewind after activation disturbance, disk save
and restore, real EGL context recreation, empty particle ownership after stock
transition, disk restoration from stock, fresh transient clouds and resumed
emission. Restored decoded atlas pixels equal the captured original-level pixels.
Protected app-state hashes match after cleanup. The APK build, scoped formatting
and both catalog checks pass. Receipts: temp/xl-pumo-particles-android and
temp/xl-evening-20261030/android-pumo-live.log.

The default legacy SwiftShader emulator still correctly rejects the known broken
fog depth transfer (observed depth 0 instead of 0.625). The host backend passes
the production depth check; no renderer gate was bypassed. GPU atlas sampling is
not a full original-scene visual comparison. Controlled original-scene particle
drawing was the next evidence gap at this checkpoint. Normal fire admission remains closed;
the unsupported hub and complete campaign are not qualified playable.

## Controlled original-scene contribution

The original inventory identifies fire object 49 at segment 409, side 2. The
existing face_view action positions the test eight units from that face, without
autonomous navigation. An explicitly armed Android diagnostic reads framebuffer
RGB immediately before and after actual particle batches in the transparency
queue. It preserves read/draw targets, packing and scissor state, bounds the
viewport and stops after success or sixteen batches. It does not compare HUD,
walls or an offscreen atlas target as a proxy for particle drawing.

The complete owner passes 64 steps in 223.039 seconds. Its scene control records
168,063 changed RGB pixels over two batches (577 quads, zero multisamples).
This establishes visible particle contribution at the controlled pose, not a
pixel-identical comparison against the original XL engine. Protected app-state
hashes match. Android build, scoped quality and both catalogs pass. Receipt:
temp/xl-evening-20261030/android-pumo-scene.log; selected state is retained as
scene-initial-state.json alongside it. Fresh scene assertions after EGL recreation
and disk return through stock exposed an intermittent shared GLES shim overread
before the post-context assertion ran. The crash's inlined source location is
gles3_shim.c:1094, the color-array copy. Copying count times stride from an interior
interleaved attribute pointer reads past the final vertex. This is not evidence
that the original scene's lifecycle was reliably safe, despite the earlier pass.

The Android shim now copies only through the final attribute value. The existing
GLES VBO owner adds client arrays ending at a protected page, tests first=0 and
first=3 in both shader paths, and requires the expected green framebuffer pixel.
It passes for D1 and D2 (10 steps each), and the app sandbox is restored. Android
build and scoped quality pass with no compiler warning/error matches. Receipts:
pumo-scene-context-crash.log, shim-span-d1.log, shim-span-d2.log and
android-shim-span-build.log in the evening artifact directory.

The fixed original owner passes 72/72 steps in 193.873 seconds. Fresh scene
requests pass before save (6,234 changed RGB pixels), after EGL recreation
(1,382,400), and after disk return through stock (338,492). Counts reflect the
current simulated clouds rather than an exact visual oracle. The final state has
twelve owners/systems and 1,219 live particles; protected app-state hashes match.
Receipt: temp/xl-evening-20261030/android-pumo-scene-fixed.log. All build/test
processes completed and git diff --check passes. Normal particle admission is
still closed; the hub and full campaign remain unqualified.

## Original reusable fire actions

The bounded original inventory identifies wall actions 19 and 20, type 32,
targeting ten and two fire owners. Both have flag 1 (TF_NO_MESSAGE), not
TF_ONE_SHOT. There is no original fire-linked disable action in this level.
The existing owner now requires exactly two actions and twelve enabled owners,
retains their original flags through rewind/disk restoration, and exercises the
same reusable authored actions after diagnostic disabling at the post-context
and disk-return boundaries. The diagnostic disable is not claimed as an authored
mission action. Full owner passes 74/74 in 194.417 seconds; protected hashes match.
Native effect layout controls, scoped quality and Android build pass. Receipt:
temp/xl-evening-20261030/android-pumo-reactivation.log.

The final fixed-view screenshot contains visible fire and smoke, but the
eight-unit pose is inside the effect and fills the windshield. It does not permit
a useful judgment of the effect's boundary against surrounding level geometry.

## Soft-intersection fidelity gap and source contract

A geometry-derived adjacent pose (segment 410, side 4) passes the scene-only
prefix in 20 steps, 31.673 seconds, while exposing abrupt rectangular intersections.
SceneOnly mode reuses the existing original owner's startup and RGB assertion,
captures the view and restores protected app state without repeating lifecycle
checks. Receipt: temp/xl-evening-20261030/android-pumo-scene-410.log.

The original atlas is RGB with neutral black fire and white smoke corners; adding
alpha handling would not follow this source path. Pinned XL quality 3 enables
soft particles. Fire and its smoke variant bypass particleFS and use glareFS with
a five-unit depth interval, disabled depth testing, weak-additive fire and
multiplicative smoke. The current runtime only applies hard GL_LEQUAL and has not
yet earned that higher-quality source behavior.

The existing EGL owner now extracts glareFS/glareVS from hash-pinned glare.cpp
(b7bac14bf8bd92b2f0e088955daf09ad58bd02b85bceb7e04dc054ded074a2cb). A portable
shader using the same equations passes 86 full-frame source GPU comparisons across
near/far depths, foreground/equal/behind geometry and both blends. Twenty-four
cases distinguish soft blending from hard depth rejection. Existing mixed queue,
particle camera/context and RGBA/indexed controls also pass. Scoped quality passes.
Receipts: soft-source-build.log and soft-source.log under the evening artifacts.
This qualifies the fragment equations only; production scene-depth capture,
shader integration, GL restoration and Android lifecycle remain outstanding.

## Production soft-particle integration

The XL-local backend now captures opaque scene depth once per transparency pass,
uses the verified source shader in original queue order, and restores the caller's
program, VAO/buffer, texture, blend, depth and cull state. Particle-owned levels use
the source near/far projection (1/5000); stock levels retain their original near
plane. Shader, vertex and depth resources follow the existing lost-context hook.

The snapshot owner passes eight depth format/MSAA cases, including 16/24/32-bit,
packed stencil, offset viewports and preserved read/draw/unpack/scissor state.
The complete production backend, not just the shader text, passes all 86 pinned
source GPU comparisons and preserves scene depth. Existing mixed queue, particle
camera/context and indexed/RGBA controls pass. A once-per-context preparation
probe samples a known copied depth for 16/24-bit targets, rejecting silent broken
depth transfer rather than trusting the blit return status.

The first Android attempt failed at startup because the emulator rejects deleting
program zero; the leaked GL error was reported by later fog allocation. Empty
shader/program deletion is now guarded. That failed owner restored its protected
state. Its runner also recovered a launcher timeout by restarting the same AVD
with the host backend; this was not a particle draw crash.

The corrected fixed view passes 20/20 in 35.867 seconds. The full original owner
then passes 74/74 in 220.530 seconds, including reactivation, rewind/disk restore,
real context recreation, stock isolation and fresh scene contribution afterward.
Protected hashes match. Native Windows build/controls, scoped quality, both
catalogs and git diff --check pass. Android compilation has one existing constant
conversion warning in unchanged d2/main/game.c, none in the new renderer files.
Receipts under temp/xl-evening-20261030: particle-depth.log, soft-backend.log,
soft-capability.log, android-pumo-soft-scene-fixed.log,
android-pumo-soft-lifecycle.log and native-soft-capability-controls.log.
The fixed-view image is pumo-soft-scene.png. These are controlled original-level
checks, not a campaign playthrough or a pixel-identical original-engine capture.
Normal particle admission remains closed pending a final requirement audit.

## Admission audit: source particle buffering

The resource, authored-state and Android lifecycle gates above pass, but the
source batching requirement is still open. The pinned `include/particles.h`
selects three buffers, each holding 25,000 entries. Actual
`CParticleManager::Add` and `Flush` bodies choose compatible buffers, flush
overlapping existing buffers and drain buffers by greatest radial distance.
Allocating a fresh buffer skips the overlap pass. Actual buffer flush admission
discards singleton buffers. A manager flush also does not select buffers whose
maximum distance is zero. These are observable source decisions, not equivalent
to drawing every adjacent queued quad.

The existing effect-reference owner now executes these pinned manager and buffer
Add/Compatible bodies, the actual compatibility table and `CEffectArea::Add`.
Its GPU submission is replaced with an ordered particle-ID receipt, after the
actual extracted flush admission checks. Seven profiles exercise 25,016 inputs:
singletons, two equal-kind particles, overlapping and disjoint fire/smoke
variants, a non-particle boundary, zero radial distance and capacity rollover.
An initial expected overlap receipt was wrong: fresh allocation skips overlap
checks, so alternating colocated fire/smoke draws fire IDs 0 and 2 while dropping
the isolated smoke entries. Corrected expectations follow that executed source.

This qualification does not change the production renderer. Normal admission
stays closed. Next bounded chunk: implement the strict fire buffering contract
locally, compare its receipts against these source bodies over broader generated
profiles, then integrate it into the existing transparency pass. Preserve mixed
wall/object boundaries and qualify resulting pixels before repeating the
original Android lifecycle owner. Existing resource/save evidence is reusable.

Final source-reference owners pass on Linux and Windows, including all 125
existing payload fixtures. Scoped quality and diff whitespace checks pass.
Receipts are `buffer-source.log`, `buffer-windows.log` and their build logs in
`temp/xl-evening-20261030`. The Windows build caught a missing explicit string
header, now corrected; the final Windows build has no warnings.

`particle_buffer.h` now implements the strict-fire decisions without changing
the runtime call path yet. The existing reference owner compares every
intermediate and final dispatch over 263 profiles and 156,088 submitted entries,
including generated 3D positions/radii, viewer offsets, all type/emissive pairs,
third-buffer replacement and interior non-particle flush boundaries. Windows
and Linux pass; Linux also passes after scoped formatting. Receipts are
`buffer-windows.log`, `buffer-portable.log` and `buffer-portable-quality.log`.
The next integration must carry original world position and source Rad into
the transparency queue: Rad is hypot of unscaled width/height, not the billboard's
decayed visible extent. Retain existing source ordering before buffer dispatch.

The runtime now carries world position and the unscaled source radius into its
shared queue and runs qualified buffering during bucket traversal. Final flush
occurs before queue storage is cleared; exceptions retain the existing cleanup.
A production native dispatch control verifies a pair before an object boundary
and suppression of the following singleton. Six complete-frame GPU comparisons
cover singleton suppression, colocated variants and disjoint reordering in both
hard and soft paths; all differ from the former per-quad output and preserve
scene depth. Existing GPU controls, native build/effect owner and Android build
pass. Android original lifecycle validation is pending at this checkpoint.
Receipts: `buffer-gpu.log`, `buffer-gpu-build.log`, `build-native.log`,
`pumo-native.log`, `android-buffer-build.log`. Runtime admission is unchanged.

Android initially exposed a diagnostic sampling problem: after source buffering,
the first 16 tiny batches contained 70 quads and no changed pixels despite 3390
live particles. A saved pre-assertion introspection snapshot confirmed no GPU
error. The bounded readback now rejects batches wholly beyond a common camera
clip plane before spending the unchanged 16-batch budget. It never changes the
renderer or relaxes the RGB assertion. At the same 409/2 pose the original scene
passes 21 steps in 36.567 seconds, observing 424,928 changed pixels across two
batches/nine quads. Protected hashes match after restoration. Both automation
catalogs pass. The full lifecycle run is pending. Failed and successful receipts
are retained as `android-pumo-buffer-scene-diagnostic.log`,
`buffer-scene-zero-state.json` and `android-pumo-buffer-scene-frustum.log`.

The complete buffered original owner passes 78/78 in 238.059 seconds. It now
explicitly opens and verifies Game Menu before forced context loss instead of
relying on backgrounding to open one: an earlier run recreated the context but
timed out waiting for that incidental menu. All original post-resume assertions
remain, including resource pixels, authored state and fresh scene RGB after
context recreation and stock return. Protected file hashes match after restore.
Receipt: `android-pumo-buffer-lifecycle-menu.log`. This closes the source buffer
ordering gap for this strict-fire checkpoint; normal admission remains closed.

## Bounded GPU submission

The soft renderer now converts each contiguous same-blend run to ordered fan
triangles and submits one upload/draw per run, capped at 25,000 quads. It retains
source buffer order and blend boundaries. GPU reference comparisons submit the
expected source-qualified sequence one quad at a time, independently of the new
coalescing. All six complete-frame comparisons and existing soft shader controls
pass. A 25,001-quad same-blend case uses neutral quads followed by one colored
quad and proves the upload split retains the final quad. Windows native build
and scoped quality pass. The initial build hit the engine's legacy min macro;
parenthesized std::min calls resolve it without changing shared headers.
Receipts: `buffer-submit-gpu.log`, `buffer-submit-gpu-build.log`,
`buffer-submit-quality.log`. Android build/validation is pending; no measured
frame-rate claim is made from reduced submission counts alone.

Android batched submission qualification passes: 21-step original 410/4 scene
in 40.523 seconds, then all 78 lifecycle steps in 229.370 seconds with matching
protected hashes. Fresh RGB checks pass before saves, after real context loss
and after return from stock. Receipts `android-pumo-submit-scene.log` and
`android-pumo-submit-lifecycle.log`; inspected image
`pumo-buffered-soft-scene.png`. These timings are test durations, not a frame-rate
benchmark. Normal admission remains closed pending the requirement audit.

## Automatic secret-save lifecycle

The final requirement audit identified a missing named gate: ordinary disk and
stock-return tests do not execute automatic secret saves. The existing native
effect owner now generates a pinned derived mission using unchanged Neptune
geometry with two appended authored fire objects (point and face), distinct
normal/secret entries and preserved companion resources. It executes real
EnterSecretLevel/ExitSecretLevel twice, verifying first-secret defaults, distinct
base/secret enable states, automatic save files, exact prepared image pixels,
fresh transient clouds and resumed emission. The native owner passes.

The existing Android POG runner has a separate SecretFixture mode pinned to that
derived HOG/descriptor. It preserves player/root secret-save files and runs a
30-step support script. Synchronous transition presentation is suppressed only
within diagnostic entry/return calls; actual automatic save/load runs, followed
by headed GPU pixel checks and emission after every transition. It passes all
30 steps in 67.100 seconds, with matching protected hashes. This proves derived
secret persistence/resource behavior, not an earned original Pumo secret exit.
Both catalogs and scoped quality pass. Receipts: `effect-original/controls.log`,
`android-particle-secret.log`, `catalog-secret.log`, `catalog-owner-secret.log`.

## Normal strict-fire admission

The completed requirement audit permits only the validated stationary,
uncontrolled, single-player authored fire profile, with point or quad-face
emission and bounded decoded arithmetic. Normal admission now invokes that
validator; the separate diagnostic fire profile and translator are removed.
Other particle kinds and malformed/unsupported shapes retain specific blockers.
The stock-only translator rejects fire because its state belongs to the XL
runtime. Metadata reports partial effect support and never infers playability.
The pinned DoStaticParticles source creates an implicit sound only for bubbles;
fire has no missing implicit audio requirement.

Validation: full inventory contracts; 125 effect profiles and 3360 truncations;
native missing-atlas rejection and recovery, secret saves and original Pumo hub/
headless-renderer rejection. Android original Pumo passes 78/78 in 249.514 seconds;
derived secret owner passes 30/30 in 55.777 seconds. Both protected file hashes
match. Receipts: `admission-inventory.log`, `translation.log`,
`effect-original/controls.log`, `android-pumo-normal-admission.log`,
`android-secret-normal-admission.log`, `admission-quality.log` and
`admission-resource-quality.log`. Earlier source/GPU renderer evidence is reused;
this final chunk changes admission/routing rather than rendering algorithms.

## Pre-context cleanup regression

PR #238 initially exposed a public headless/startup crash. The existing GPU
owner reproduced SIGSEGV before eglInitialize: ParticleDepth::discard called a
null extension function for framebuffer zero. Empty texture/framebuffer/VAO/VBO
handles are now skipped. The regression calls normal/abandoned/normal particle
cleanup before any GL context or GLEW initialization. It failed before the fix
and passes afterward together with the complete GPU owner (114 upload/draw cases,
2120414 pixels, maximum analytical error one, retained-pixel/context controls).

Fix commit 55c937273 is published on branch 86. Receipts: empty-cleanup-before.log,
empty-cleanup-backtrace.log, empty-cleanup-after.log and empty-cleanup-quality.log.
New CI is running. The earlier LAN failure was separate: its runner had only
414652 KiB free where provisioning required 5242880 KiB; gameplay did not start.

## Authored smoke qualification (branch 92)

The candidate extends stationary, uncontrolled single-player particle admission
from fire to authored smoke (subtype 0). It shares emitter scheduling, world
collision and transparency ordering while retaining smoke-specific color,
remaining-life decay, growth, drag, 8-by-8 atlas animation and source-alpha blend.
Mission/base image provenance and retained-pixel context recovery now cover both
images independently. Unsupported particle kinds remain rejected.

Pinned source comparisons cover 7680 color/decay cases, 2640 sequential color
transitions, 2304 creation cases, 22464 updates, 2880 emitter frames, 1440
billboards, 1680 animation cases and 264 mixed buffer profiles. Existing fire
comparisons remain passing. Actual source image loading matches all 1048576
RGBA bytes of the original 512-by-512 smoke.tga. Smoke uses zero atlas inset;
fire retains its one-pixel inset. Linux EGL checks include alpha soft-depth and
hard-atlas blending, mixed transparency and context recreation.

Android's existing POG owner now has a SmokeFixture mode. Its derived Pumo hub
changes exactly one byte: clears outdoors on segment 154 while retaining the
original smoke object, geometry and companion resources. All 68 steps passed in
139.346 seconds: live emission, changed scene pixels, all 64 atlas cells, disk
restore, context recreation, stock cleanup and return. Protected hashes match.
This qualifies the isolated smoke lifecycle, not unchanged Pumo hub gameplay.

Final explicit Windows/Linux --effects runs pass 125 profiles and 3360 payload
truncations. The initial new assertion incorrectly compared whole shadow files;
those intentionally retain unused source records. The corrected assertion checks
the active 79-byte classic object header and separately preserves smoke subtype.
Default inventory success alone did not execute these effect contracts.

Receipts are under temp/xl-skybox-20261031: smoke-linux-source.log,
smoke-color-source.log, smoke-original-image.log, smoke-image-source.log,
smoke-gpu.log, smoke-effects-windows.log, smoke-effects-linux.log and
android-smoke-report-fixed.log. Final native lifecycle refresh remains pending
at this checkpoint. Branch 92 is not yet published.

Final native refresh passed after the corrected effect fixture assertion. The
owner exercised mixed smoke/fire resources, scene submission and activation
restore, existing automatic secret-save transitions, stock isolation and
original hub rejection. Receipt: native-runtime/controls.log. Windows inventory
and explicit Windows/Linux effects controls pass. Android evidence is reused:
only a host assertion changed after its completed run.
