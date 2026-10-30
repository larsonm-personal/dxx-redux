# Authored particle effects: preparation and fire simulation foundation

Status: preparation, emitter definitions, face sampling and authored fire CPU
creation/update comparisons pass. Engine binding, real-world collision/lookup,
resources, rendering and mission admission are not yet implemented by this work.
Pumo Mines is not qualified as playable. Work is on `xl_more_78_particles` in
the main checkout, `C:/local/dxx-redux`.

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
