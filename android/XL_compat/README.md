# XL compatibility boundary

This directory owns D2X-XL format knowledge and compatibility policy. The stock
`d1/` and `d2/` engines do not depend on it. Do not add XL branches or relax
validation in their loaders to make a file load

The initial native library and command-line tool inspect exactly level version 15
with game-data version 33. They read ordinary DHF HOGs or individual RL2 files.
The inspection tool does not convert content, enable play or claim compatibility. Geometry, wall,
trigger and embedded color values are emitted for comparison against an independent
reference. Objects, header data, reactor targets, generators and delta lights are
also decoded. Unverified behavior and asset dependencies remain explicitly unknown

```sh
cmake -S android/XL_compat -B temp/xl-compat-build
cmake --build temp/xl-compat-build --config Release
ctest --test-dir temp/xl-compat-build -C Release --output-on-failure
# Executable is under Release/ with a multi-configuration generator
temp/xl-compat-build/Release/xl_inspect d2-xl.hog temp/xl-report.json
```

Exit 0 means every RL2 member was inspected, not that any level is playable.
Exit 1 means at least one level was rejected; the report retains its reason and
the other levels' results. Exit 2 means an input, invocation or output error.
Every report explicitly says `playable: false`. Reports are deterministic and
identical regeneration preserves the previous file's modification time

No game assets are committed here. Use the original local archive identified in
[the pilot plan](../ai%20tool%20plans/mission%20metadata/d2xxl_first_mission_pilot_20261023.md)
for real-content inspection. The CTest fixture is synthetic

Configure `-DXL_PILOT_HOG=/absolute/path/to/d2-xl.hog` to register the private
`xl_pilot_archive` integration test. It checks the source hash, all 30 levels,
pilot record counts, explicit non-playable status, repeatable output and source
preservation. This is a parser integration test, not a gameplay test

## Report values

Records preserve file order so array indices identify source records. Coordinates,
orientation, UVLs, light values, object physics and timers retain their serialized
integer units. Texture overlay values include orientation bits. Color components
are decoded to double precision from the version-specific integer/double encoding;
the original index byte is preserved. Colors are not clamped or discarded

Compact numeric rows use these field orders:

- `flicker_lights`: segment, side, mask, timer, delay
- `reactor_targets` and trigger targets: segment, side
- `matcens`: robot bitmask 0, robot bitmask 1, hitpoints, interval, segment, fuel center
- `light_indices`: segment, side, count, first delta index
- `delta_lights`: segment, side, padding byte, four vertex intensities
- Object `movement_data`, `control_data` and `render_data`: serialized field order
  for the tagged movement/control/render type, with vector/matrix components
  flattened. See the explicit decoder switches in `inventory.cpp`; these are raw
  values before either engine's object normalization

The old 700-entry object-trigger head table is retained when present without
object triggers. Only all-`-1` heads are neutral; dangling nonempty heads are
reported as unknown. Nonempty object-trigger payloads are unsupported

## Verified checkpoint

On 2026-10-23 the native CMake build passed with MSVC warnings treated as errors.
Both CTest tests passed, including the pinned original 30-level archive. An
Android x86_64 NDK build passed its synthetic test on emulator-5554 and inspected
the same archive. Windows and Android produced identical report SHA-256
`98b135993ce65c4599888762369b22652f4c076d8c66f2dd6c32bfb66c8a9cb7`

This is platform parity between two builds of this reader, not an independent XL
reference comparison. No level has been played or rendered by this module

The library uses only the C++ standard library, requires no renderer or engine
globals, and can be built separately on desktop or Android. Future admission,
translation and presentation support belong here too. A translated level may
use stock loaders only after every required XL feature has a verified equivalent;
dropped colors or substituted stock behavior must not be called faithful support

## Developer stock-loader probe

`xl_probe_fixture --incomplete-presentation-probe input.rl2 NEW-output-directory`
creates an explicitly unverified loose mission fixture. Pass the resulting root
as the stock headless engine's `-extra-dir`, with `-mission xlprobe`. The descriptor
is placed in `missions/`; the loose level is at the mounted root. Existing output
directories are refused. This tool is not connected to import or mission admission

The fixture translates bounded geometry and rebases game-record offsets for the
unchanged stock loader. Nonneutral ownership, object triggers, unknown extra
semantics and unrepresentable references are rejected. Original color tables
remain in `xlprobe.source.json`; stock rendering does **not** apply these colors.
Asset resolution and gameplay semantics remain unknown. `UNVERIFIED.txt` travels
with the fixture and its mission display name includes `UNVERIFIED`

Optional CMake settings `XL_PILOT_LEVEL`, `XL_STOCK_METADATA` and `XL_RETAIL_DATA`
register `xl_stock_loader_probe`. Supply absolute paths to the pinned original
level, existing D2 headless metadata executable and private retail data. This
integration checks source preservation, refusal to overwrite, explicit nonplayable
status and the stock loader's geometry/object counts. It passed on Windows for
Ahayweh Gate. It does not exercise combat, doors, triggers, saves or rendering

A pinned D2X-XL reference executable also loaded the original Ahayweh Gate and
saved a starting-room screenshot. Optional XL enhancement assets were missing,
audio was disabled and no gameplay route was tested. This establishes reference
startup only; it is not an independent semantic dump or fidelity certification

## Stock field audit

For a separate D2 build tree, add
`-DCMAKE_PROJECT_INCLUDE=/absolute/path/android/XL_compat/stock_audit/configure.cmake`
to the usual D2 CMake configuration, then build `dxx-redux-d2-headless-metadata`.
That opt-in build produces `main/xl_stock_audit` instead of the metadata CLI.
Do not apply this setting to a normal game build tree. The audit reuses stock
initialization and loading; no stock source is changed

Run with `-xl-source original.rl2 -probe-level xlprobe.rl2 -hogdir RETAIL_DATA
-extra-dir PROBE_ROOT -mission xlprobe -userdir SCRATCH_USER`. It compares loaded
vertices, segment topology and attributes, side textures and UVLs, wall fields
and trigger fields/targets with the original XL inventory. It also compares object
identity, position, orientation, size/shields, contained items, physics/spin, AI,
pickup counts and polygon/animation records. Any difference makes
the command fail. Counts are checked before indexing. Wall controlling-trigger
values are excluded because the loader reconstructs them. Explosion/weapon/light
control payloads, runtime-only object fields and embedded XL colors are not covered
by this audit. It checks original robot, weapon, powerup and contained-item IDs
against loaded retail tables before comparison so loader substitutions cannot hide
missing definitions. These checks do not certify every transitive asset dependency

The pilot currently produces **29,120 comparisons, six mismatches and 225 direct
dependency checks**. Segments
51, 81, 82 and 111 have ambient flags 2 after loading, versus 0 in the source.
Stock `set_ambient_sound_flags` recomputes these flags. XL also recomputes them in
`main/findsegment.cpp`, but uses its own passability logic. Therefore the audit
remains failing until runtime equivalence is established; these differences are
not silently allowed. Player-start object 4 also has two orientation components
changed by one fixed-point unit; the stock loader orthogonalizes player matrices.
Reference parity for that normalization remains unverified. All other compared
fields agree. Negative probes detected a one-unit vertex change without a count
change and rejected original robot ID 255 instead of accepting substitution.
This is stronger than
count agreement but still does not establish gameplay compatibility

## Assisted route and checkpoint diagnostic

Set `XL_STOCK_ROUTE` to the stock headless route executable, alongside the stock
loader test settings, to enable `xl_route_checkpoint_probe`. It creates a fresh
unverified fixture, verifies two fresh route reports are byte-identical, saves a
checkpoint after obtaining the red key, and restores that save in a separate
process. The restored run must retain the red key and reach reactor/exit objectives

This passed with a freshly built Windows stock route executable: the fresh route
took 1,739 simulated frames; the red-key checkpoint took 713; the restored route
took 1,005. These timings describe the diagnostic actor, not normal player travel

The route harness removes ordinary robots, grants player invulnerability, boosts
projectile damage, pauses the reactor countdown and commits the actor's position
to the player for checkpoint saving. It does not prove combat, normal countdown
survival, every trigger/branch, native level transition or XL reference parity.
Each run writes `coverage.json` with those limitations and `playable: false`.
All original acceptance gates remain in effect

## Vertex lighting calculation

### Separate rendering preview build

`runtime/configure.cmake` is an opt-in `CMAKE_PROJECT_INCLUDE` for a dedicated D2
build tree. It produces `xl-render-preview`, leaving normal builds and the base
source files untouched. The build generates a render source overlay with a
single checked hook and a loader wrapper; all policy and state live in
`runtime/preview.cpp`. The hook generation fails if its stock anchor is absent
or no longer unique

Use the developer `xlprobe` fixture and supply both `-xl-static-light-preview`
and `-xl-source /path/to/original/d2leva-1.rl2`. The loader checks the complete
fixture bytes against a fresh translation of the supplied source, then checks
loaded geometry before retaining source colors. Each level load first clears
that state, so unrelated levels cannot inherit the pilot's colors

The Windows x86 preview executable builds and has rendered the pilot's starting
room. For reproducible startup, place the private translated `xlprobe.rl2` beside
the executable and its descriptor in `missions/xlprobe.mn2`, then pass
`-xl-preview-start -xl-static-light-preview -xl-source <original.rl2>` together
with `-hogdir <retail-data> -window`. Startup skips the briefing but applies no
route assists. After 30 swaps with shaded vertices, it requests a screenshot in
the executable's `screenshots` directory. The isolated graphics overlay captures
the back buffer before swap; the initial post-swap front-buffer attempt produced
a black image. This proves a drawing path, not reference fidelity or playability
The companion `xl-preview-lifecycle` uses the real loader and tests color
activation, clearing on an unrelated level, reloading, rejection of a one-bit
coordinate change with identical counts/file size, and recovery after restoration.
Configure `XL_PREVIEW_LIFECYCLE` in the standalone test build to enable the private
`xl_preview_lifecycle` CTest with the existing pilot/retail-data settings. It passes
and verifies the original source remains unchanged

This integration exposed the stock MSVC headers' persistent one-byte packing.
The XL C++ headers now explicitly establish and restore their own eight-byte
packing, preventing color-record layout changes across the stock/XL boundary.
The analytical, pinned-source lighting comparison and inventory tests also pass
This is explicitly a static-light integration experiment: it replaces face RGB
with the embedded ambient calculation and omits dynamic light, flash scaling and
special face effects. It must not be used to certify gameplay or full rendering
support. Reference image comparisons and normal gameplay remain required

`lighting.cpp` implements the XL vertex-color calculation as a standalone module:
face/ambient blending, scalar and colored dynamic-light mixing, reactor/seismic
flash input, brightness, scalar saturation and final light scaling. It is connected
only to the opt-in static preview described above. Light-source generation, face setup, lightmaps and
runtime state transfer remain unimplemented; this does not change playability

`vertex_brightness` reconstructs the initial per-vertex maximum of decoded face
UVL brightness, matching `CSide::Read` for the audited 15/33 geometry. Open portals
without walls contribute nothing; portal walls do contribute. This value is
required for XL's ambient/dynamic-color mixing and is not the ambient RGB maximum

The analytical test covers cases that distinguish XL's maximum-channel cap from
simple RGB multiplication. For the optional source comparison, configure
`XL_REFERENCE_RENDERLIB` with the audited local XL `render/renderlib.cpp`.
`tests/reference_lighting.cmake` checks its exact SHA-256 and extracts the original
`ScaleColor`, `SetVertexColor`, `SetVertexLight` and `AdjustVertexColor` functions
into the build directory. A minimal data harness compiles those unchanged
functions and compares 8,192 deterministic option/input combinations against the
new module. Windows x64 and Android x86_64 tests pass with RGB tolerance 0.000001
and exact scalar equality

The harness substitutes XL's vector/data containers and fixed-point helper; it is
an arithmetic comparison, not an independent runtime render. Debug-only and
disabled lightmap-adjustment code are excluded. The full executable, light
generation, face setup, assets and visual checkpoints still need verification
