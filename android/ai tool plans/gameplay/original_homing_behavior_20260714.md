# Original Homing Behavior Research and Restoration Plan

Date: 2026-07-14
Status: Complete

## Objective

Reconstruct the shipped Descent and Descent II homing guidance behavior from original source, quantify how Redux/Rebirth differs, and add an optional original-homing gameplay setting. The setting applies only to single-player and cooperative games. Competitive multiplayer always retains Redux behavior.

## Completed Work

- [x] Read repository instructions and locate current homing guidance, targeting, update scheduling, and settings code
- [x] Trace repository history for frame-independent homing, update-rate controls, Retro Homing Speed, and later retuning
- [x] Obtain and inspect the official Parallax D1 v1.5 and D2 v1.2 source releases
- [x] Derive acquisition and retention cones, range, straight-flight delay, cadence, steering, acceleration, orientation, rescan, and lifetime-loss behavior
- [x] Quantify the effective behavior at a fixed 25 Hz reference cadence and define a render-frame-independent compatibility model
- [x] Add a persisted Original Homing toggle to D1 and D2 gameplay settings
- [x] Synchronize the host setting for cooperative games and force Redux behavior in competitive multiplayer
- [x] Implement symmetric D1 and D2 guidance changes, including D1 gameplay inside D2
- [x] Add focused automated coverage for game-mode policy and exact fixed-point retention thresholds
- [x] Run scoped code quality, native tests, Windows D1/D2 builds, and Android all-ABI debug assembly

## Primary Sources

The source archives were downloaded from the Icculus D2X archive, which publishes the original source releases made available by Parallax:

- `d1srcpc.tar.gz`, D1 source v1.5, SHA-256 `37F5ABDB433EEFFB015BF6895299F2028CF1051EB8763B5F1C1A2B82761F1382`
- `d2src.tar.gz`, D2 source v1.2, SHA-256 `029459B19314B559B3D6528020CD8B779EAEDB478DC77098F5878906C6A608F4`

The extracted evidence is under the ignored research directory `android/temp/original_descent_source/`. The most relevant retail files are `main/laser.c`, `main/laser.h`, and `main/game.c` in each archive.

These are the final official DOS source snapshots rather than the first 1995 D1 executable or unpatched 1996 D2 executable. They are the strongest available primary evidence for the shipped, patched retail behavior and preserve the relevant code and revision history.

## Findings

### Fixed weapon parameters

| Behavior | Descent | Descent II |
| --- | ---: | ---: |
| Initial target acquisition dot | `3/4` | `7/8` |
| Initial acquisition cone half-angle | 41.4096 degrees | 28.9550 degrees |
| Maximum acquisition distance | 250 units | 250 units |
| Initial straight flight | 1/8 second | 1/8 second |
| Visible missile orientation scale | 8 | 16 |
| Omega acquisition dot | N/A | `15/16`, or 20.3641 degrees, with Omega range |

The weapon data still supplies each weapon's maximum speed by difficulty. There is no separate angular-rate constant. Retail steering normalizes current velocity, adds one normalized target-direction vector, normalizes the sum, and restores speed. Blob-rendered smart children add the target vector twice. This discrete vector blend is the effective turn rule.

On each guidance frame, speed increases by `max_speed * FrameTime / 2` until it is within one unit of maximum speed. Polygon missile orientation separately blends its forward vector toward normalized velocity by `FrameTime * scale`.

### Retention cone is the major D2 regression

Retail recomputed `Min_trackable_dot` from render `FrameTime`. At the compatibility reference cadence of 25 Hz, the exact 16.16 fixed-point results are:

| Model | Dot threshold | Cone half-angle |
| --- | ---: | ---: |
| Retail D1 formula with D1 base `3/4` | `62914 / 65536` | 16.2620 degrees |
| Retail D2 formula with D2 base `7/8` | `51610 / 65536` | 38.0469 degrees |
| Current Redux D1-style formula with D2 base `7/8` | `64225 / 65536` | 11.4796 degrees |

The current D2 code applies the D1 frame-scaling formula to D2's `7/8` base. At 25 Hz this narrows D2 target retention from about 38 degrees in retail to about 11.5 degrees. This confirms the reported weaker tracking, and the largest cause is the formula substitution rather than weapon data or an explicit PvP turn-rate nerf.

Retail D2's piecewise formula is:

```
FrameTime <= 1/64:  min = 7/8
FrameTime <  1/32:  min = 7/8 + 1/64 - 2*FrameTime
FrameTime <  1/4:   min = 7/8 + 1/64 - 1/16 - FrameTime
otherwise:          min = 7/8 + 1/64 - 1/8
```

### Frame dependence and cadence

The retail loops clamp `FrameTime` between 1/150 and 1/5 second, but the homing turn executes once per rendered frame. Retail therefore has no single original turn rate: higher render rates execute more normalized vector blends per second, while the acceleration, orientation, cone, and lifetime terms also depend on `FrameTime`.

Redux commit history shows the transition:

- `a9ee22e7` introduced a fixed 25 Hz Retro homing scheduler
- `78cc3bb8` and `5fee7673` added configurable network homing update rates to D1 and D2
- `eed96224` corrected D2 acceleration to use the ideal homing timestep
- `592388ab` added Retro Homing Speed, and `fd03b71e` disabled it by default
- `b6120b02` replaced that option and standardized quick normalization, which is also what retail used
- `f8ac5033` made tracking update phasing relative to missile creation

The fixed scheduler made steering render-frame independent, but D2 retained the substituted D1 retention formula. It also left visible orientation using render `FrameTime` even though the orientation function now runs only on homing ticks. At high render rates, that makes orientation lag and indirectly makes retention checks fail because tracking visibility is tested against missile orientation.

### Target scans and lifetime loss

- D1 retains a valid target continuously and scans for a replacement every fourth phased frame when invalid.
- D2 forces a valid target rescan every eighth phased frame and permits reacquisition every fourth phased frame.
- Retail phases scans with `(object_index ^ FrameCount)`. Redux changed this to homing ticks relative to missile creation. Original mode restores the retail object-index phase on the fixed compatibility clock.
- D1 computes the velocity-to-target dot for turning lifetime loss. It ignores error at or below 1/8, caps error at 1/4, and subtracts `error * 16 * FrameTime`.
- D2 uses the retention orientation-to-target dot, has no threshold, and subtracts `error * 32 * FrameTime`.

## Compatibility Definition

"Original" uses the retail algorithms at an explicit fixed 25 Hz reference cadence. This cadence matches Redux's established single-player compatibility clock and common emulator-era behavior. It cannot reproduce every historical machine simultaneously because the DOS code was render-frame dependent, but it gives deterministic behavior that is independent of display refresh rate.

Original mode restores:

- the retail D1 or D2 retention formula
- retail scan/reacquisition semantics on the compatibility clock
- D1 and D2 lifetime-loss rules
- orientation, acceleration, and steering time terms evaluated at 1/25 second
- D1 rules when D1 gameplay is hosted inside D2

Redux mode remains unchanged, including the selected network homing update rate. Competitive modes ignore both the player and host Original Homing values. Cooperative games use the host's synchronized value.

## Implementation

- Shared policy and fixed-point formulas: `android/app/src/main/cpp/shared/homing_compat.h`
- Focused tests: `android/tests/test_homing_compat.c`
- Player persistence and Misc Options toggle: D1/D2 `playsave.*` and `menu.c`
- Cooperative host setting and synchronization: D1/D2 `multi.h`, `net_udp.c`, and netgame profile persistence
- Runtime selection: D1/D2 `gameseq.c`, `object.*`, and `laser.*`

The default is off. The gameplay checkbox is `Original homing (Single/Coop)`, and the host advanced-network checkbox is `Original homing (Coop only)`.

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- D1 native CTest: 21/21 passed
- D2 native CTest: 24/24 passed
- D1 Windows x86 RelWithDebInfo full build: passed
- D2 Windows x86 RelWithDebInfo full build: passed
- Android `:app:assembleDebug`, including arm64-v8a, armeabi-v7a, x86, and x86_64 native builds: passed
- `git diff --check`: passed before final documentation update

## Remaining Limitation

There is no isolated automated flight-trajectory harness in the engine, so verification covers the exact formulas, policy gating, all native suites, and all production build targets rather than pixel-for-pixel missile paths. A manual gameplay comparison against DOS at a controlled 25 FPS would still be useful for subjective confirmation, especially for smart children and tight-corner reacquisition.
