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
