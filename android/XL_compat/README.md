# XL compatibility boundary

This directory owns D2X-XL format knowledge and compatibility policy. Ordinary
engine builds keep XL support disabled. The dedicated D2 pilot uses guarded
`DXX_XL_PILOT` hooks; do not relax general loader validation to admit XL files

The `XL compatibility contracts` workflow runs on pull requests and manual
dispatch, building the standalone inventory/lighting checks on Linux and Windows
and auditing the preserved survey ZIP. It needs no mission binaries. These
checks establish parser/arithmetic contracts and evidence integrity, not pilot
playability; private gameplay and reference-engine tests remain separate

Current user scope: fully playable pilot with faithful XL engine extensions,
using Redux behavior for classic Descent features. Pixel parity and XL tweaks to
features already present in 1996 are not required. The historical lighting
experiments below are optional research, not prerequisites for support. Authored
XL colors/opacity and gameplay-affecting extension data must still work correctly

The dedicated runtime build now offers `-xl-pilot` for the original
`d2leva-1.rl2`, including when it resolves from the original mission HOG. Admission
requires the exact audited source size and SHA-256, then the bounded translator's
feature checks. A private `xl_preview_cache/pilot.rl2` supplies decoded stock
records through an isolated file-open hook; the engine retains the original
level filename for mission/save logic. Embedded extension state is activated only
after the stock loader succeeds and loaded geometry agrees. Other levels still
follow stock validation; this is not general version-15 admission

With the original mission installed in the dedicated build's mission directory,
`-xl-preview-start -xl-pilot` starts D2-XL level 1 without a pretranslated fixture
or `-xl-source`. It remains an unverified developer pilot, not production support.
The lifecycle test covers direct and original-HOG admission, original filename
preservation, one-bit source rejection, recovery and subsequent stock isolation

Set `XL_PILOT_DESCRIPTOR` to the original outer `d2-xl.mn2` to enable the
original-mission save/restore slice of that test. It starts level 1 normally,
checks all 38 robots remain and invulnerability is absent, then saves controlled
wall-opacity and switched-light changes. Restore must retain mission/level
identity, player state, robots, UVLs, live opacity and XL color activation.
The test selects its private write directory before console/log initialization
and verifies the save exists there. Direct state changes make this persistence
coverage, not evidence of normal progression or a completed playthrough

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

### Normal-input red-key integration test

`xl_normal_reactor` extends this test by restoring the earned key checkpoint,
collecting supplies through ordinary pickups, following authored door-trigger
dependencies and destroying the reactor with normal fire. Two runs agree on
red-key pickup at frame 4887 and reactor destruction another 6819 frames later,
with about 50.9 shields and the normal 45-second countdown. The earned reactor
save is `preview-user/xl-normal-reactor.sav`. This supersedes the older red-key
timing below; timed exit and full branch coverage are still unverified

With the original pilot descriptor/HOG and runtime executable configured, CTest
registers `xl_normal_red_key`. It drives the real player through ordinary input
and frame processing at difficulty 2 with all 38 initial robots and seed 1.
It requires a living red-key pickup and writes the actual player checkpoint to
the fixture's `preview-user/xl-normal-key.sav`. Two consecutive runs reach the
key at frame 4356 after nine kills, with 12 shields remaining. This is separate
from the assisted route diagnostic; reactor, exit and full branch coverage remain
unverified. Run it with `ctest --test-dir <compat-build> -R xl_normal_red_key
--output-on-failure`

### Separate rendering preview build

Configure a dedicated D2 build with `-DDXX_XL_PILOT=ON`. Ordinary guarded hooks
in the engine call `runtime/preview.cpp`, which owns the extension policy and
state. `runtime/configure.cmake` only attaches sources, include paths and compile
definitions; it does not rewrite or substitute engine sources. The option defaults
to OFF. When converting an older preview build, remove its project-include cache
entry with `-UCMAKE_PROJECT_INCLUDE`. The outputs remain `xl-render-preview` and
`xl-preview-lifecycle`

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
the executable's `screenshots` directory. The guarded graphics hook captures
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
This is explicitly a rendering integration experiment: it replaces face RGB
with the embedded ambient calculation and the partial dynamic path below.
Special face effects and remaining emitters are incomplete. It must not certify
gameplay or full rendering support. Reference images and normal play remain required

`lighting.cpp` implements the XL vertex-color calculation as a standalone module:
face/ambient blending, scalar and colored dynamic-light mixing, reactor/seismic
flash input, brightness, scalar saturation and final light scaling. It is connected
only to the opt-in preview described above. Light-source generation and runtime
state transfer are partial; full face setup and lightmaps remain unimplemented.
This does not change playability

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

The isolated preview also applies the pilot's authored textured-wall alpha and
render-through classification. XL uses a fade range of 31: cloak 21 becomes
10/31 alpha, while 31 is opaque on the ordinary textured path. The real-loader
lifecycle test covers all 14 nonzero pilot records and state isolation. This is a
partial material implementation, not full XL wall or gameplay qualification

For material inspection, `-xl-preview-wall <index>` repositions the player camera
to the selected wall's segment center and faces that wall. This is diagnostic
camera placement, not normal playthrough evidence. `-xl-preview-stock-materials`
disables the material adapters for comparison. Captures at 30 swaps are not yet
deterministic simulation-time comparisons; animations can differ

The preview now consumes reactor/seismic flash state in XL vertex shading. The
real-loader integration also checks all 70 pilot light sources through off/on
updates and repeated-call behavior against the pinned legacy scalar formula;
changed UVLs reach the colored-light hook. Complete dynamic colored-light generation,
reference timing, object illumination and rendered/save-state parity remain open

`DynamicVertexLight` now preserves XL's separate scalar intensity and ordered
source-color accumulation. Configure `XL_REFERENCE_LIGHT` to the pinned local
`lighting/light.cpp` to run 12288 comparisons against unchanged XL color routines.
Windows and Android native comparisons pass. Stock accumulated RGB cannot be
converted back into this state without losing information

The preview now populates this state each rendered frame for robots, debris,
explicit light objects, ordinary weapons, markers and indexed explosions. It uses visible vertices for ordinary point sources,
segment-local squared attenuation for dim sources, and original vertex brightness
in the color mix. The real-loader test checks an explicit point light reaching
shading, clearing between frames, and an unsupported multiplayer omega being counted.
These checks do not verify every emitter or attenuation boundary against XL

Multiplayer omega and unsupported player-mode emitters remain unresolved and are counted
in capture diagnostics. Alternate lighting modes remain unimplemented;
zero unresolved objects would not prove complete dynamic lighting. Powerup
lights are disabled to match the saved reference profile, pending confirmation
of the running reference settings. Object illumination, distance math mode,
reference frame timing and rendered/save-state parity remain open. The stock
lighting loop still runs for stock consumers; future emitter ports must avoid
duplicating lifetime or random-number side effects

Ordinary weapons use the reference table's initial white color, with orange
1/0.3/0 for the pinned XL missile classification (including robot missiles).
Weapon colors are forced into the ordered accumulator even when white. Markers
use their green pulse and segment-local attenuation. Their stock lifetime update
is retained exactly once; the XL hook reads the unchanged fractional lifetime.
Lifecycle coverage checks missile/non-missile shading, radius/visibility limits,
marker pulse, and clearing when dynamics are disabled. Multiplayer omega's random
suppression remains unresolved

Indexed fireball animation colors now use each resolved frame's stored palette
index, or XL's pixel-average/nearest-palette fallback when that index is zero.
The fallback excludes black/transparency pixels and preserves XL's search through
index 254, which differs from the stock palette helper. Frame colors are averaged
and normalized by their maximum channel; light intensity decays with lifetime.
The integration test covers raw/RLE_BIG pixels, stored-index precedence,
noncontiguous frame IDs, decay, and three loaded retail explosion clips.
True-color overrides, XL-only explosion render modes, full resource precedence
and matched reference scenes remain unverified. This implementation resolves
indexed GameBitmaps only; it does not establish general XL override support

Muzzle flashes are snapshotted at the stock flash loop's timestamp before queue
expiration, then cast before object lights without a source color. The guarded
lighting hook leaves expiration with the stock loop and does not mutate the
queue itself. XL's one-third-second decay and signed 16-bit elapsed-time behavior
are preserved. Integration checks cover new/half/near-expiry/expired flashes,
unchanged queue state during capture, and the stock expiration hook. These checks
do not establish matched reference wall-clock timing or save/replay behavior

The normal single-player ship light now uses captured pre-scale control thrust,
not final physics thrust or velocity. Guarded controls/object hooks capture
the vector and clear it on player reset; level loading clears it too. With the
saved reference profile's ship 0 and damage model disabled, XL's speed and drive
multipliers are both one. Death clears the captured vector. A real-controls test
distinguishes raw/scaled thrust in the resulting shading and checks death reset.
Alternate ships/damage models, multiplayer and reference timing remain
outside this result. The profile assumption still needs running-reference validation

The local headlight now follows the legacy non-darkness profile: intensity 10,
eightfold radius, forced white, squared forward-dot cone at dot >= 0.5, and
distance-faded spill outside it. Both possession and on flags are required for
the saved profile's available headlight with no built-in loadout. Lifecycle checks
cover front/back/side, an angled beam, extended reach, range boundary and flag
gating. Multiplayer obstruction limits and darkness spot sizes remain unimplemented;
this does not qualify object illumination, energy drain or rendered reference parity

Flares now use XL's `(1.75 + RandFloat(4)) * min(light, life)` formula.
Despite its name, that helper divides by four: the multiplier is 1.75..2.0.
The preview consumes one 15-bit FX sample per evaluated flare, preserving the
simulation RNG stream. Tests cover endpoint/lifetime arithmetic and seeded live
shading with exact FX-call accounting and unchanged simulation state. This
preserves the formula and sample range, not XL's libc random sequence or its
global RNG coupling. Matched captures must control samples or compare flicker
behavior rather than demand identical unconstrained frames
