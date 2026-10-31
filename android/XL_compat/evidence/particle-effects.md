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
