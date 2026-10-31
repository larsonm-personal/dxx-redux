# XL compatibility boundary

This directory owns D2X-XL format knowledge and compatibility policy. Normal OpenGL D2 builds include stock and supported XL gameplay in one binary.
`DXX_XL_RUNTIME` marks renderer capability internally; it is not a build option.
Runtime feature and resource checks decide admission. Software-renderer builds
retain explicit rejection because XL rendering is not implemented there

XL OpenGL drawing is in `runtime/corona_renderer.c`, compiled into D2's OpenGL
target in normal OpenGL builds. Stock source retains the small alpha and
depth-policy integration hooks. D1 needs no matching XL hooks for this D2-format
pilot; shared Android save infrastructure continues to serve both engines

Game menus show `XL` in the screen's upper-right corner while a game with loaded
XL extensions is active. This includes standard menus, list boxes, pause and
control-binding screens. The indicator reflects runtime activation, not build
capability or mission-name matching; it clears when the game closes or stock
level data replaces the XL source. Drawing lives in `runtime/menu_indicator.c`

The mission-selection list prefixes detected XL missions with `XL: `, including
in ordinary builds. `mission_label.cpp` inspects the descriptor, adjacent HOG
and referenced loose level headers without activating a mission. The prefix
identifies XL content; it does not claim that its features are supported. Original
mission names, identities and ordering are retained; D1 keeps its `D1: ` prefix

`shared/mod_save_supported_data.h` is the existing production UUD presentation-pack
allowlist, not a per-level XL database or pilot fixture. Save compatibility uses
it to constrain audited presentation-only changes. Mission source identities are
captured at runtime. The XL checkpoint exporter reuses that validator; the only
data-formatting change splits the same JSON payload into MSVC-sized literals.
Do not generate another registry for each XL level

Original Ahayweh Gate is now verified playable in the OpenGL singleplayer runtime.
The [acceptance record](evidence/pilot-acceptance-audit.md) and
[19-category requirement checklist](evidence/pilot-requirements.md) identify the
exact source/APK, authored extensions and evidence limits. All 36 private host
tests pass; the current Android rendering suite passes 28/28 twice. This does
not qualify the full campaign. Admission now checks requirements and dependencies
per level; only explicit navigation fixtures stop after one level

The `XL compatibility contracts` workflow runs on pull requests and manual
dispatch, building the standalone inventory/lighting checks on Linux and Windows
and auditing the preserved survey ZIP. It needs no mission binaries. These
checks establish parser/arithmetic contracts and evidence integrity, not pilot
playability; private gameplay and reference-engine tests remain separate

The private `xl_normal_escape` CTest extends the normal key/reactor run: it
restores the earned reactor checkpoint, drives ordinary inputs to the exit
under the unchanged countdown, and requires native completion while alive.
The runtime awards normal completion bonuses and preflights the next level.
Explicit navigation fixtures stop after one level; headless tests skip the score dialog.
The explicit Android `test_xl_pilot_completion.jsonc` now passes 22 steps from
an earned exit-approach checkpoint: native exit, actual score screen/bonus,
readiness-gated dismissal, and a usable main menu with completion retained.
It does not certify Android import or an uninterrupted Android playthrough

The separate explicit-only `test_xl_pilot_import.jsonc` verifies ZIP import and
startup using unchanged original HOG/descriptor bytes. Stage their ZIP as
`files/xl-original-import.zip`, preserve and move aside any manually installed
`d2-xl` files in `files/d2x-redux/missions`, then run through the same Android
test helper with `-PreserveState`. The 15-step check passes on emulator-5554;
extracted hashes match the source and startup has active XL rendering with no
unresolved emitters. The imported descriptor still describes the full campaign;
each level must pass detected-feature and runtime dependency checks. Import success
does not qualify the other levels or promote production compatibility status

The 34-step runtime persistence/isolation test also passes with this imported
copy: authored state survives saves, retail Counterstrike clears the XL state,
and restoring the pilot brings its state back. The previous quick slot was
preserved and restored. Authored side/texture-color semantics remain open

`test_xl_pilot_coronas.jsonc` is a separate controlled rendering test: after
startup it repositions the camera to an authored orange emitter and checks
corona draw submissions. It also reads back a controlled owned framebuffer
using the real renderer and normal clockwise/back-face culling. All 28 steps
pass: authored orange pixels are visible, a nearer half-depth mask blocks only
that half, and a full mask blocks all pixels. This caught and corrected a
counterclockwise fan that submitted draws but produced no pixels. The
procedural halo consumes live texture/light/wall state; host tests exercise 112
visible emitters and 148 solid-wall occlusion controls. The actual scene/object
checks below supplement these isolated controls. The active pilot uses the
depth-buffered mine/object path even if classic depth ordering is requested;
this preserves the stored preference, which takes effect again in stock levels.
The same effective choice controls native custom-model depth toggles. This camera pose
is not evidence of normal-input progression

The test switches the requested classic-depth setting on and back off without
persisting it, and requires actual halo draws with depth testing enabled while
classic is requested. Native lifecycle checks verify both preference values
across pilot/stock transitions. The tested selection policy does not establish
real-scene depth contents or object-silhouette occlusion

An explicit scene hook now also tests actual wall depth immediately before
normal halo rendering. With color/depth writes disabled, GPU sample queries
see the original emitter, reject the same halo moved behind its opaque wall,
and see that behind-wall control again with depth disabled. The 24-step test
passes with zero GL errors. This verifies wall occlusion at the controlled
view. Object-pass verification follows below

A second explicit hook brackets the real object pass. At the controlled
segment-26 view, a small diagnostic halo behind an existing robot is visible
before objects, hidden afterward, and visible with depth disabled. The same-frame
GPU queries pass with zero errors and without moving/replacing robots or writing
scene pixels. Combined with the wall test, this verifies actual scene/object
depth on the tested emulator path, not every model/device or full reference
equivalence. The test still does not prove normal-input progression

The escape fixture exports `preview-user/xl-normal-escape.dximdemo`, the native
`XLTest.plr` profile and `xl-normal-exit-approach.sav`. The last save includes
the real source-definition trailer required by Android; the private export
hook is not enabled in ordinary builds. For the completion script, preserve
the emulator's existing `Players/save_sets/single/player/d2-xl/player.sg3`,
stage the approach save there, verify matching SHA-256, then run the script
with `-Game d2 -Serial emulator-5554 -PreserveState -LeaveRunning` through
`android/helpers/run_test.ps1`. Restore the previous slot afterward. See the
pilot plan for exact tested artifacts and the preserved failed attempts

Private `xl_normal_branch_1` through `xl_normal_branch_12` tests separately
verify trigger activation/target state. The nine `xl_normal_secret_*` tests add
actual ship entry into each reported secret area using normal controls from
earned checkpoints. Eight follow the red-key route; the reactor-adjacent secret
follows the normal reactor fight and reaches its room with 27.48 seconds left.
Life-count and countdown assertions reject respawned or paused runs. Together
with the existing navigation/lifecycle checks, all 25 pass. This is individual
host entry coverage, not one uninterrupted Android all-secrets run

The branch tests separately
observe native trigger activation and linked-target passability, then verify
the actual target and player state through save/restore. All 12 pass twice,
along with lifecycle and timed escape. Trigger 12 is covered on the normal key
route before restoring its save. These checks do not establish physical
traversal of every linked portal or an uninterrupted all-secrets playthrough.
Exact current artifacts are recorded in the pilot plan

Four `xl_normal_portal_*` checks now extend those routes with explicit adjacent
segment crossings (176 -> 182, 193 -> 194, 136 -> 137, 5 -> 185), starting from
earned branch saves. They require an actual frame transition across the selected
edge, an alive player, unchanged life count and an unpaused countdown. Together
with the prior route logs these cover the physical portals represented by all
17 linked-target records. This does not assert both travel directions or a
separate crossing after every trigger that shares a target

Current user scope: fully playable pilot with faithful XL engine extensions,
without requiring incidental XL retuning of classic Descent gameplay (for
example, a wholesale flare-speed change). This is not a requirement to preserve
every Redux gameplay choice. Authored extensions such as warps and speed boosts
must work wherever present. Pixel parity is not required; visual tweaks are allowed.
The historical lighting experiments below can inform implementation without
making exact image matching a prerequisite for support. Authored
XL colors/opacity and gameplay-affecting extension data must still work correctly

Build the normal Android APK with
`gradlew.bat :app:assembleDebug -PciTestAbi=x86_64`. No XL property is needed.
The D2 library retains its JNI name; D1 does not receive the D2 runtime hooks. This does not
enable the launcher's compatibility approval. Guarded `xl_pilot` introspection
reports admission, completion, shaded-vertex calls, unresolved emitters and
observed trigger activations. The x86_64 APK builds and the original mission
starts on emulator-5554 through the normal menus. The bounded emulator pilot
is qualified; physical-device and full-campaign claims remain separate

With that APK, retail D2 data, original descriptor/HOG staged under
`files/d2x-redux/missions`, and an existing `player` pilot, run:

```powershell
android/helpers/run_test.ps1 -ScriptName test_xl_pilot_runtime.jsonc -Game d2 -Serial emulator-5554 -PreserveState -LeaveRunning -TimeoutSeconds 240
```

This explicit-only runtime test passes all 34 steps, checking Ahayweh Gate,
38 robots, no invulnerability, active XL shading and zero unresolved emitters.
It quick-saves, changes difficulty, restores the saved difficulty and XL state,
then loads retail Counterstrike and requires XL diagnostics to clear before
quick-loading the pilot again. A controlled fixture additionally switches one
actual light and changes wall 6 opacity, saves them, deliberately disturbs them,
and checks all 5,472 UVLs, 228 subtraction masks and the restored material alpha
after both restores. This establishes controlled persistence, not natural
operation of that light/wall. It does not certify import, completion or all
authored visuals. The test leaves its controlled save/level state in place
The initial manual run encountered an emulator System UI ANR; that screenshot
is retained separately from the later unobstructed capture in the pilot plan

Material tests now cover all 14 authored-opacity illusions through native AI
visibility, object visibility and weapon-object collision queries, each with
a solid-boundary negative control. These controlled fixtures do not claim live
projectile damage. The full host lifecycle/escape/12-branch sweep still passes

Texture-color audit caveat: XL's legacy mode retains baked vertex colors, but
`effects/glare.cpp` also consumes texture RGB for coronas. Vertex arithmetic
tests alone do not establish this presentation behavior or other lighting modes

The optional XL runtime detects extended LVLP headers and admits supported
version-15/game-data-33 requirements under any mission or level name. There is
no production name, level-number, size or hash allowlist. `admission.cpp` rejects
unsupported/unknown serialized features and capacity overflows; runtime checks
source object IDs and transitive resources before stock code can clamp them.
Custom HAM/HXM definitions, unknown descriptor options, unaudited boss spawning
and multiplayer remain rejected with specific reasons. A private translation
retains original mission/save filenames and checks loaded geometry before XL
state activates. New-game, progression and secret-entry preflights show the
blocking reason. Other entry paths retain loader rejection diagnostics.

Static compatibility reports mark supported features eligible, not gameplay
verified. Assets are checked on load; subsequent levels are checked separately.
The original Counterstrike pilot remains gameplay evidence, not an exception.
With the original mission installed in the dedicated build's mission directory,
`-xl-preview-start -xl-pilot` starts D2-XL level 1 without a pretranslated fixture
or `-xl-source`. It remains an unverified developer pilot, not production support.
The loader rejects original pilot and explicit translated-preview admission in
multiplayer before loading. Lifecycle checks cover network/cooperative rejection,
stock-format loading and recovery to single player with authored colors/materials.
This does not qualify multiplayer UI behavior or cooperative pilot support.

The lifecycle test covers direct and original-HOG admission, original filename
preservation, renamed/rehashed admission, unsupported-version and missing-resource
rejection, recovery and subsequent stock isolation

Set `XL_PILOT_DESCRIPTOR` to the original outer `d2-xl.mn2` to enable the
original-mission save/restore slice of that test. It starts level 1 normally,
checks all 38 robots remain and invulnerability is absent, then saves controlled
wall-opacity and switched-light changes. Restore must retain mission/level
identity, player state, robots, UVLs, live opacity and XL color activation.
The test selects its private write directory before console/log initialization
and verifies the save exists there. Direct state changes make this persistence
coverage, not evidence of normal progression or a completed playthrough

The native library and command-line tool inspect the bounded level/game pairs
15/33 and 17/36. They read ordinary DHF HOGs or individual RL2 files. Runtime
translation accepts bounded 15/33 and 17/36 requirements. Inspection still does
not prove resource availability, rendered parity or mission completion.
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
for real-content inspection. The public CTest fixtures are synthetic

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

Configure private diagnostics with `-DDXX_XL_DEVELOPER_TESTS=ON`.
Normal builds keep the game and metadata executable names and entry points.
Only the diagnostic build replaces metadata with the lifecycle harness. Ordinary guarded hooks
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
UVL brightness, matching `CSide::Read` for the audited 15/33 and 17/36 geometry. Open portals
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

## Separate water arena experiment

The opt-in runtime offers `-xl-water-physics` for a detected experimental fluid
profile. Developer capture uses `-xl-preview-start -xl-water-physics -mission NAME`.
Admission does not depend on a mission name, level name, or source hash. Robot
combat/generation in fluids and multiplayer still reject with specific reasons.
This does not broaden production admission or qualify a full campaign. Its one
additional semantic area is the old-format water segment: the fluid-enabled
profile uses 75-percent movement time, thrust turbulence, water contact and the
stock ship-in-water sound. Source properties are reconstructed on save restore
and cleared on every unrelated level load. Multiplayer is explicitly refused.

Configure `XL_ARENA_HOG`, `XL_ARENA_DESCRIPTOR`, `XL_PREVIEW_LIFECYCLE` and
`XL_RETAIL_DATA` for the private `xl_water_arena` CTest. It verifies original
resource loading, real physics, ordinary-input entry/exit, checkpoint replay,
arena/stock isolation, renamed/repacked admission and unsupported-capability
rejection. The private original fixture remains hash-pinned in the test runner.
The arena has no reactor or exit; this is not campaign
completion. Android authored traversal, interactive rendering/audio and alternate
XL/fog profiles remain unqualified. See the [expansion plan](../ai%20tool%20plans/mission%20metadata/xl_level_expansion_20261024.md)
for source pins, ordered level gates, measurements and limitations.

The Android controlled probe is `android/tests/test_xl_water_runtime.ps1`, with
explicit `-Serial`, `-Hog` and `-Descriptor` arguments for that original private
fixture. It requires an installed normal OpenGL debug APK, retail D2 data and
the `player` pilot. It backs up and restores pilot/save/configuration and matching
mission files. An interrupted restoration leaves `files/xl-water-runtime-backup`
in app storage and the next run refuses to overwrite it.

Android intentionally ignores `d2x.ini`. The child script enables the experimental
profile using `set_debug` field `xl_water_physics`, value `enable`; `disable`
restores normal admission. This debug-only automation action requires the XL
build and refuses profile changes during gameplay. It does not skip any resource
or unsupported-feature check. The script repositions into water, applies thrust,
checks wet/dry save restoration and stock isolation, then verifies normal
admission rejects water. It does not establish full authored traversal or visual/audio
parity. `xl_pilot.in_water` describes the active player, not retained level data
after aborting gameplay.

On emulator-5554 the expanded controlled probe passes all 57 steps (26.768 seconds),
including thrust-driven crossing of the authored water portal in both directions
from staged poses, active SDL waterfall playback, dry-space sound cleanup, save
restoration with playback, stock transition and disabled-profile rejection.
Original pilot/save hashes match after restoration. This is an emulator runtime
result, not full spawn-to-water traversal, physical-device or reference-rendering
qualification. Interior-water loop reset/retry and sound-registry exhaustion have
native coverage; they are not evidence of waveform or reference mixing parity.

## Observatory serialization prerequisite

Inspection of original Observatory XL now decodes its 17/36 header, integer
texture colors and neutral sparse object-reference tail. Synthetic fixtures also
check packed 13-bit light counts, including counts above the stock byte range,
and reject malformed bounds and unsupported version pairs. Equipment payloads
are bounded and reported unsupported rather than silently skipped. Nonempty
game-36 sparse reference tables remain outside this bounded reader profile.

Configure `XL_OBSERVATORY_HOG` with the original local archive to enable the
hash-pinned `xl_observatory_inventory` test. It checks 328 vertices, 110 segments,
54 walls and 44 objects, complete section endpoints, 26 preserved unsupported
old-type-13 segments, and 37 preserved reserved segment-flag records. The source
archive is checked again after inspection. No private bytes enter public CI.

This is the first [Observatory prerequisite](../ai%20tool%20plans/mission%20metadata/xl_observatory_prerequisite_20261024.md),
not a gameplay unlock. The [17/36 flag audit](evidence/segment-flags-17-36.md)
now identifies those high bits as preserved reserved data, with a compiled pinned
reference oracle and all-byte inventory tests. Bounded runtime translation now
has Konflict acceptance coverage; Observatory's no-damage semantics and
presentation still need separate qualification. The
level has no exit trigger, so eventual arena qualification cannot establish
campaign completion. No new runtime or save state is introduced here.

Native D2 discovery recognizes exact `d2x-name` descriptors and resolves their
ordinary level lists and companion DHF archives. XL identity remains distinct
from stock extra-HAM missions. OpenGL builds apply per-level feature and owned-resource
checks; default builds retain rejection. Alternate HOG and critical directives
remain unsupported. Configure `XL_OBSERVATORY_DESCRIPTOR` together with
`XL_OBSERVATORY_HOG`, `XL_PREVIEW_LIFECYCLE` and `XL_RETAIL_DATA` to run the
private original-descriptor discovery, rejection and stock-isolation test.

Configure `XL_REFERENCE_SEGMENT_FLAGS` with pinned `main/findsegment.cpp` to run
the optional `xl_segment_flags_reference` oracle. Raw flags remain visible in
inventory records and optional `observations`; reserved-bit acceptance does not
widen the treatment of unaudited flags in other version profiles.

### Konflict 17/36 acceptance

The unchanged Konflict distribution exercises the bounded 17/36 translator and
owned D2 LGT brightness overrides. Configure `XL_KONFLICT_HOG` and
`XL_KONFLICT_DESCRIPTOR`, plus `XL_PREVIEW_LIFECYCLE` and `XL_RETAIL_DATA`, to
enable the private `xl_konflict_features` CTest. Its hash checks identify test
inputs only; runtime admission contains no mission-name or hash exception.
It checks original geometry/light records, the 910-entry LGT snapshot, runtime
emitters, changing-light save/restore, reactor/door behavior and stock isolation.
Derived malformed/foreign/absent companion controls are reported separately.

The explicit Android runner is `android/tests/test_xl_konflict_runtime.ps1`
with `-Serial`, `-Hog` and `-Descriptor`. It requires an XL-enabled APK, retail D2
and pilot `player`; backs up and restores test-touched state; and checks startup,
render activity, quick-save/restore, the XL menu indicator, stock transitions and
a controlled authored-light face with actual corona draws.
It does not navigate the level or establish uninterrupted playthrough or visual
equivalence with the reference engine. D2 CLR overrides and multiplayer remain
rejected. See the [tranche record](../ai%20tool%20plans/mission%20metadata/xl_konflict_tranche_20261025.md).

# True-color POG acceptance

The single-frame RGBA implementation has independent source conversion and
desktop shader comparisons, original-resource installation, gameplay pixel
consumers and normal Android lifecycle controls. See
[the evidence and reusable commands](evidence/truecolor-pog-runtime.md) for
exact scope, frame/cockpit rejection and original Pumo coverage. This does not
qualify the Pumo campaign. The historical fixture results above retain their
original scope.

# Owned named-sound acceptance

Action 30 has separate XL wall/object playback with owned SND/WAV resources,
stereo output and saved voice positions. Positive counts, infinite loops,
cooldowns, autoplay, pause, volume and level/Redux cleanup have independent
controls. See [named-audio evidence and commands](evidence/named-audio-runtime.md).
The public Linux owner requires `--gpu --audio`; Windows requires the portable
contracts, with actual native output covered by the private runtime owner.
The [sound-folder increment](evidence/audio-scopes.md) adds owned mission,
normal-level and secret-level resources, with actual Android output and saved
scope controls. Global addons and ambient sound objects remain separate
requirements.

Ordinary sound replacements have different selection rules from named actions.
The [classic bank controls](evidence/classic-audio-bank.md) cover owned resources,
bank suppression, stereo decoding and explicit rejection while the replacement
playback consumer remains unimplemented.

# Modern wall flags and placed mines

Modern wall flags retain their complete authored value while native mutable
flags use the low byte. Marker obstruction and sphere/plane tests follow the
pinned XL source; see [marker-door evidence](evidence/door-markers.md).
[Placed-mine evidence](evidence/placed-mines.md) covers weapon-model admission,
fresh empty hit histories and the original Pumo authored route through level 7,
followed by hub checkpoint recovery. This is controlled dispatch, not a human
playthrough or whole-campaign qualification.

The [spray controls](evidence/spray-particles.md) extend that original route to
level 8, including all 15 authored spray owners, checkpoint activation recovery,
live emission, smoke-atlas GPU checks and actual world-scene framebuffer
readback at an authored spray face. XL effect positions retain their
authored segment association as in the pinned loader. Other particle kinds and
later campaign requirements remain gated.

The [lightning simulation](evidence/lightning-simulation.md) has pinned-source
setup, geometry and lifetime comparisons. It is not yet connected to the native
rendering, lighting or sound consumers, so lightning remains gated. Native
ownership and activation/save integration are covered by the effect lifecycle owner.
