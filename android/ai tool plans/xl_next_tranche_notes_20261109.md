# Sol implementation notes: next XL tranche

Read with [the execution plan](xl_next_tranche_20261109.md). Facts below were
checked locally and against GitHub on November 9, 2026, around 20:30-20:50 PST.
Recheck changing branch state at implementation start. Paths are local to this
Windows workspace unless described otherwise.

## Source and evidence map

| Item | Location / identity |
| --- | --- |
| Workspace | `C:/local/dxx-redux` |
| Published-tip inspection worktree | `temp/xl-stack-conflicts-20261109`, HEAD `268e711f008cbf5770b74ee365da4d821723fb57` |
| Published source tree | `d01c46fbe590d56cfb69ba63a1bbf7432e91a74f` |
| Last observed cmake | `49e92bbcdf92b31f77c705f65587d3dfe81f299c` |
| Validated candidate source | `temp/xl-stack-review-20261109/source`; has existing uncommitted changes matching the published tree; do not reset or reuse for new edits |
| Pinned reference | `temp/d2x-xl-reference`, revision `2576a98b5de310e3ac7c9777df2677d1627b8bae` |
| Planning probe artifacts | `temp/xl-next-tranche-20261109` |
| Original private asset checkout | `C:/local/dxx-redux-test-assets`; local checkout may be older than remote overlays |
| Original XL distribution | `C:/Users/first last/Downloads/d2x-xl-2-1-8.7z` |
| Retail runtime data | `temp/xl-stack-review-20261109/retail-data` |
| Existing Windows binaries | `temp/xl-stack-review-20261109/build-d2/main` |
| Existing inspector | `temp/xl-stack-review-20261109/public-msvc/build/xl_inspect.exe` |

The original XL archive is 1,607,764,603 bytes; SHA-256
`2cfc020fbf25a048f099511b71599c7be1b77836be8fc1112e5eb310c65beb9d`.
Read/extract data; do not execute the installer. It is solid 7z, so repeated
single-file extraction may be costly. Prefer the existing bounded importer for a
complete resource set. Do not change the original archive or private checkout.

The fresh planning probes used the previously built matching-tree Windows
headless binary, SHA-256
`98c3e62188c54c7d0f160845684c0c5438730230dba4b1b99a3bcfbffc1c89e7`.
This was not a fresh build from the published Git HEAD. It is adequate to locate
baseline blockers; build new binaries for implementation acceptance.

## What was actually reproduced

`headless-baseline/results.json` is a fresh unchanged eight-case run using
`android/tests/xl_readiness_inventory.json`, pinned asset revision
`9dad21bd65ba05665ac6fe315f37fe7461e85725`. It completed with 4 loaded/ticked and
4 load failures. Runner exit 0 means the measurement completed, not that every
case passed. Passed cases are Diehard 17, Ironstar 6, Orion Nebula 4 and Phobos-E
4; all report XL inactive. The latter two do not confirm the separate navigation
probe. The catalog covers a larger archive inventory, but this denominator is 8.

| Original failing case | Fresh failure | Supplemental probe |
| --- | --- | --- |
| Boiling Point 7, `control2.rl2` | Five runtime blockers listed below | Inventory read from unchanged staged HOG |
| Dinter 1, `dinter.rl2` | 43 runtime blockers | Inventory read from unchanged staged HOG |
| Sphere 2, `sp-02.rl2` | Missing owned `lightning.wav` | Adding genuine lightning.wav exposes missing `flowing-lava.wav` |
| Warp Shock 6, `warplab.rl2` | Missing owned `computer.wav` | Adding genuine computer.wav exposes missing owned cambot.hxm or data/d2x-xl.hog |

Supplemental probe script: `temp/xl-next-tranche-20261109/probe-addon-resources.py`.
Its `genuine-addons/manifest.json` records input hashes, unchanged original HOG
hashes and both nonzero engine exits. It copies retail data into a disposable
owned startup directory; it does not modify official assets or readiness policy.
This script is disposable diagnosis, not a proposed new product importer.

Genuine source members confirmed:

| Member inside original archive | Bytes / relevant identity |
| --- | --- |
| `D2X-XL/sounds/d2x-xl/lightning.wav` | 786,476; SHA-256 `149382490dfec4517e2594921c8dcb71518c372846b73ee3c157a0f9ca3efd30` |
| `D2X-XL/sounds/d2x-xl/computer.wav` | 4,277,216; SHA-256 `44ea1d3dc5c9329161eb059a7de06a845e61ba7d8a77beb7253d6526c3f4d8ed` |
| `D2X-XL/sounds/d2x-xl/flowing-lava.wav` | 1,196,884; listed, not extracted in this probe |
| `D2X-XL/data/d2x-xl.hog` | 3,815,963; listed, not extracted in this probe |
| `D2X-XL/textures/D2/slowmotion#0.png` | 551,831 |
| `D2X-XL/textures/D2/bullettime#0.png` | 826,728 |

[Private assets PR 4](https://github.com/larsonm-personal/dxx-redux-test-assets/pull/4)
is **merged**, merge commit `f23b692c4eea3b6dca437bed13bf23d814bacb70`, head
`8a09f8288bcedd323535a4a4c10c1b0482b30b70`. Its description documents the
`xl-time-pickups` overlay with those two genuine strips, 256x4096 RGBA and 16
distinct square frames each. Manifest SHA-256 is
`12d91454c70033d8b8d2ec5ebc32336518806cd8664ea26e2ec4c9e1e6bdd2bd`.
Fetch/read the exact merged tree to locate files and verify bytes; do not confuse
the description's older validation SHA with the final merge. The overlay's
host-only extraction profile is not engine importer/rendering integration.

Earlier `android/ai tool plans/xl-rendered-readiness.md` reports additional
full-resource leads: Sphere HXM needs the qualified Vertigo data and then exposes
an XL boss-profile blocker; Warp Shock reaches pickup 48 after real GL/resource
setup. These are useful leads, **not fresh current-tip results**. Reproduce after
A before claiming they are the current blockers or implementing boss support.

## Resource and platform seams

- `android/app/src/main/java/com/dxxredux/app/ResourceModCatalog.kt`: existing
  `xl-resources` mapping includes d2x-xl.hog/d2x.ham, `textures/d2x-xl` effects
  textures and `sounds/d2x-xl` named waves. lightning, computer, flowing-lava,
  slowdown and speedup are already catalogued. The time-pickup `textures/d2`
  pair is not yet integrated there.
- Adjacent `ResourceModImport.kt` and `ResourceModStore.kt` own normal import,
  storage and enabling. Reuse the shared catalog and bounded extraction path;
  consult `ResourceModStoreTest.kt` for existing tests.
- `android/XL_compat/runtime/audio_resource.cpp::load_audio_resource` searches
  mission HOG, owned level/mission sound folders, then `sounds/d2x-xl` in the
  owned startup installation. Arbitrary extra-directory WAVs are not equivalent.
- `runtime/resource_validation.cpp`: owned installation and camera addon loading.
  Desktop startup origin derives from base HAM ownership; Android uses launcher
  owned file-set projections. Camera addon lookup accepts owned cambot.hxm or
  data/d2x-xl.hog. Follow the actual importer destination instead of guessing.
- `runtime/addon_audio_resource_test.h` already covers precedence, startup
  ownership, malformed/foreign sources and retained bytes. Extend only for new
  behavior; don't recreate this contract in a new suite.

## Slow-motion implementation seams

Under `android/XL_compat`:

- `slow_motion.h/.cpp`: existing reference-derived state machine, default speedup
  profile 6. Equipment bits 32768 and 65536. State has separate world and console
  player clocks (speed, direction, update milliseconds, active), plus transition
  sound state. Inputs include equipment/energy/frame/difficulty/time, both press
  flags and XL/network/nostalgia/reactor/mixer context.
- `runtime/slow_motion_runtime.h/.cpp::slow_motion_step`: production seam exists
  but only `slow_motion_physics_test.h` and `water_actors_test.h` call it. Header
  explicitly says gameplay input/pickup activation is not enabled.
- Existing consumers cover displacement, lifetime, rotation, AI timers/turning,
  firing deadlines, charge/orientation, missile last-fire and Omega render-clock
  behavior. Search usages before touching per-frame timing. Add the missing
  central adapter, not new independent clocks in each consumer.
- Sink is called in source order. Only play events consume its success result.
  Returned events are audit data; dispatching them again duplicates effects.
  `energy_delta` belongs at the existing qualified player-resource updater.
- `runtime/slow_motion_save.h` and `slow_motion_runtime.cpp` currently persist
  weapon-clock details only. `runtime/trigger_save.cpp` schema 32 validates before
  applying, stores `weapon_clock`, and has exact member-count checks. Integrate
  active clocks/equipment atomically there and update the complete schema
  contract together. Repo policy allows replacing pre-release XL save formats;
  do not build an unnecessary migration/compatibility-test stack.
- Existing reset and Omega rules are source-specific. Preserve the deliberate
  rejection of input demos when active XL owns Omega; full recording/replay
  render-clock support is separate work, not a guard-removal task.

Reference entry points: `physics/slowmotion.cpp::DoSlowMotionFrame`,
`ToggleSlowMotion`, sound helpers; `main/game.cpp` calls DoSlowMotionFrame at
approximately line 1099. `include/powerup.h` defines IDs 48 and 49. Locate actual
pickup/input/save implementations with `rg --files` and symbol searches; do not
assume a file named main/powerups.cpp exists. Trace the effective compiled path.

## Camera/lightning findings worth implementing

Current original inventories are `boilpnt-inventory.json` and
`dinter-inventory.json` under the planning artifacts. Read each selected level's
**runtime_blockers**, not `inventory.unsupported`; the latter is the reader's
generic format inventory and includes features the runtime already implements.

Boiling Point's five findings:

- Object trigger 5: type 16; owner object 33, type 20 effect, id 1 lightning,
  control/movement 0, render 12, flags 0. Camera target face is segment 930 side 5,
  flags 0, time -1, value 327680.
- The same record also fails generic owner event-lifecycle admission.
- Lightning object 32: fixed-target qualification fails. Payload is a zero-bolt
  anchor-like record with authored id 3, target 0. Inspect payload definitions
  before using raw indexes in implementation.
- Equipment center record 1: powerups 48 and 49 unsupported.

Dinter's 43 findings: 9 camera triggers, 17 lightning findings, 11 particle
findings and 6 effect-object findings (some describe the same objects).
The nine camera records are 20,37,44,49,56,70,76,88,93. Owners are robots
877,1028,1193,1194,1243,1346,1350,1465,1466: type 2, control 1, movement 1,
render 1. This needs moving attachment semantics, not just relaxing a type check.
Many rejected lightning records have zero bolts; records 1523,1526,1557 contain
actual nontrivial emitter definitions and may expose additional profile limits.

Current camera seams:

- `camera_bindings.h` already enumerates object-camera bindings for general
  nondeleted owners, with target-face validation and bounded camera count.
- `camera_admission.h::qualified_camera_binding` only admits a narrow CAMBOT
  shape (type 16), explaining the original camera-owner failures.
- `runtime/object_trigger.cpp` has separate binding/event-operation logic and a
  CAMBOT-specific path. Trace this together with admission and render ownership;
  camera metadata must not accidentally become an unsupported firing action.
- Reference `render/cameras.cpp` around lines 580-600 loops objects and their
  camera triggers and passes the owner object into camera creation. Follow the
  renderer's subsequent attachment/death handling before generalizing support.

Current lightning seams:

- `lightning_admission.h::authored_lightning_target` requires one stationary
  target for most fixed cases; `prepare_authored_lightning` requires bolts > 0
  and the implemented style-0 endpoint profile. Do not conflate target-only
  anchors with emitters that require those capabilities.
- Reference `effects/lightning_manager.cpp::StaticFrame` skips objects with
  `nBolts <= 0` before target/shape creation. `FindTargetPos` returns the first
  matching other lightning object and returns null for target 0. If there is no
  target, StaticFrame computes endpoint = position + forward * authored length.
- Verify object iteration ordering, active/deleted lifetime, RNG and renderer
  effects before extending the runtime. Preserve original owner objects and IDs.
  A removed admission error without correct runtime behavior is not a fix.

## Reproduction and validation commands

Run from the repository root. Existing work directories may not be reused by
these runners; choose a new output name and use `retain-recent-artifacts.ps1`
according to repository instructions. Commands below are baseline examples;
replace inspection source/binary paths with the implementation worktree and
fresh candidate binaries for acceptance.

```powershell
$inspection = 'C:/local/dxx-redux/temp/xl-stack-conflicts-20261109'
$env:SDL_AUDIODRIVER = 'dummy'
$env:SDL_VIDEODRIVER = 'dummy'
python "$inspection/android/tests/xl_readiness.py" run `
  --engine 'temp/xl-stack-review-20261109/build-d2/main/dxx-redux-d2-headless-route.exe' `
  --assets 'C:/local/dxx-redux-test-assets' `
  --data 'temp/xl-stack-review-20261109/retail-data' `
  --catalog "$inspection/android/tests/xl_readiness_inventory.json" `
  --work 'temp/xl-next-tranche-20261109/headless-repeat' `
  --seven-zip 'C:/Program Files/7-Zip/7z.exe' `
  --target-sha 268e711f008cbf5770b74ee365da4d821723fb57
```

For supplemental rendered probes use `android/tests/xl_rendered_readiness.py`:
required options are `--engine --data --extra --work --mission --level
--level-file`. It requires a real GL context, exact original level identity and
120 simulated/rendered frames; timeout 45 seconds. On desktop Windows remove
the dummy video driver for rendering. On Linux use Xvfb with x11 and llvmpipe as
documented in `xl-rendered-readiness.md`. Software GL is not hardware GPU proof.
Use original staged extras under `headless-baseline/6/extra` (Sphere) or
`headless-baseline/7/extra` (Warp Shock), with a correctly owned complete addon
installation. Original mission names are `sphere` and `wshock20`.

From the implementation source root:

```powershell
# Supported normal Windows entry point; developer lifecycle builds also need
# DXX_XL_DEVELOPER_TESTS=ON in their separate CMake configuration
./run-windows-build.ps1 -Target both
python android/XL_compat/tests/public_contracts.py --work temp/xl-next-public

# Android JDK setup before the repository's documented Gradle/build entry point
$env:JAVA_HOME = 'C:/local/jdk-21'
$env:Path = "$env:JAVA_HOME/bin;$env:Path"
& 'C:/local/android-sdk/platform-tools/adb.exe' devices
```

Linux public selection adds `--gpu --audio`. Existing Pumo regression entry:

```text
cmake -DENGINE=<fresh-xl-preview-lifecycle> -DDATA=<retail-data> -DRESOURCES=<original-pumo-root> -DOUTPUT=<new-output> -P android/XL_compat/tests/pumo_scenarios.cmake
```

See current `android/XL_compat/README.md` section "Focused Pumo gameplay
scenarios" and `.github/workflows/xl-compat.yml` / `xl-pr-assets.yml` for full
required selections. Existing `temp/xl-stack-review-20261109/build-local.ps1`
and `run-focused.ps1` document local MSVC/cache setup but hardcode old source and
output paths; adapt into the new work area, do not run blindly against the old
validated tree. Never attribute cached old binary results to new source.

The prior cleanup passed required private 113/113, public Windows 56/56, public
Linux 61/61 and Android runtime 65/65. These are baselines, not new-feature
acceptance results. The private run used native Linux output storage after
demonstrating WSL Windows-mounted I/O timeouts; do not relax required selection
or deadlines. WSL launch here may need sandbox escalation; an E_ACCESSDENIED
launch is infrastructure, not a native feature failure.

Final report should state what a user can actually do, give executable/APK and
resource setup paths, and attach the smallest useful reproducible evidence.
Prefer one shared scenario and a few diagnostic assertions to another matrix
of synthetic microfeatures. Preserve blockers and uncertainty in plain language.
