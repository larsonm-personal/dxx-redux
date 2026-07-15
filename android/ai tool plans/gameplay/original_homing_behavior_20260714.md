# Original Homing Behavior Research and Restoration Plan

Date: 2026-07-14
Status: Complete

## DescentBB Evidence Follow-up

- [ ] Read the linked DescentBB thread and enumerate its concrete homing-missile claims
- [ ] Follow cited posts, patches, commits, source releases, and named contributors where available
- [ ] Compare the claims with retail D1/D2 source and Redux git history
- [ ] Correct or extend the findings, implementation, and BLUF if the evidence changes them
- [ ] Record sources, unresolved claims, and verification results in this plan

## Objective

Reconstruct the shipped Descent and Descent II homing guidance behavior from original source, quantify how Redux/Rebirth differs, and add an optional original-homing gameplay setting. The setting applies only to single-player and cooperative games. Competitive multiplayer always retains Redux behavior.

## BLUF

This comparison treats retail as though its render loop were locked to exactly 25 FPS. All angles are cone half-angles.

| 25 Hz comparison | Initial acquisition | Continuous retention | Steering and orientation | Broad target scans |
| --- | ---: | ---: | --- | --- |
| Restored/retail D1 | 41.410 degrees | 20.364 degrees | 25 steering updates/second; orientation scale 8 | When retention fails, every fourth phased frame: up to 6.25/second |
| Redux D1 single-player | 41.410 degrees | 20.364 degrees | 25 steering updates/second; orientation uses render time | Same 6.25/second maximum, phased from missile creation |
| D1-in-D2, Original enabled | 41.410 degrees | 20.364 degrees | 25 steering updates/second; D1 orientation scale 8 | D1 retail scan rules at up to 6.25/second |
| D1-in-D2, Redux | 28.955 degrees | 14.362 degrees | 25 steering updates/second; D1 orientation scale 8 using render time | Redux scan phasing at up to 6.25/second |
| Restored/retail D2 | 28.955 degrees | 37.989 degrees | 25 steering updates/second; orientation scale 16 | Invalid target: up to 6.25/second; forced valid-target rescan: 3.125/second |
| Redux D2 single-player | 28.955 degrees | 14.362 degrees | 25 steering updates/second; orientation uses render time | Invalid target: up to 6.25/second; no forced valid-target rescan |

The normalized steering blend is otherwise the same: ordinary homers add one target-direction vector per update, while blob-rendered smart children add it twice. Redux multiplayer can select 20-30 steering updates/second. At exactly 25 rendered FPS, standalone Redux D1 is close to retail D1; the strong difference seen in higher-FPS retail or source-port footage comes from retail steering and rescanning once per rendered-frame schedule. D1 hosted inside D2 retains its existing D2-based cones in Redux mode and now selects the D1 retail cones in Original mode.

## Completed Work

- [x] Read repository instructions and locate current homing guidance, targeting, update scheduling, and settings code
- [x] Trace repository history for frame-independent homing, update-rate controls, Retro Homing Speed, and later retuning
- [x] Obtain and inspect the official Parallax D1 v1.5 and D2 v1.2 source releases
- [x] Derive acquisition and retention cones, range, straight-flight delay, cadence, steering, acceleration, orientation, rescan, and lifetime-loss behavior
- [x] Quantify the effective behavior at a fixed 25 Hz reference cadence and define a render-frame-independent compatibility model
- [x] Add a persisted Original Homing toggle to D1 and D2 gameplay settings
- [x] Synchronize the host setting for cooperative games and force Redux behavior in competitive multiplayer
- [x] Implement D1-in-D2 scan phasing, lifetime loss, orientation scale, and time-step rules
- [x] Add focused automated coverage for game-mode policy and exact fixed-point retention thresholds
- [x] Run scoped code quality, native tests, Windows D1/D2 builds, and Android all-ABI debug assembly
- [x] Correct the remaining D1-in-D2 acquisition and retention base constants
- [x] Replace release-disabled `assert` checks in the homing formula test with active failures

## Retail Parameter Restoration Follow-up

- [x] Represent the D1 and D2 acquisition thresholds as shared compatibility tuning parameters
- [x] Select the D1 threshold for D1-in-D2 only when Original Homing is active
- [x] Feed the selected threshold into both initial acquisition and continuous retention while leaving Redux selection unchanged
- [x] Correct the focused formula test and make failures active in release-style builds
- [x] Run scoped code quality, focused native tests, headless demo regressions, Windows builds, and Android assembly

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
| Retail D1 formula with D1 base `3/4` | `61440 / 65536` | 20.3641 degrees |
| Retail D2 formula with D2 base `7/8` | `51651 / 65536` | 37.9887 degrees |
| Current Redux D1-style formula with D2 base `7/8` | `63488 / 65536` | 14.3615 degrees |

The current D2 code applies the D1 frame-scaling formula to D2's `7/8` base. At 25 Hz this narrows D2 target retention from about 38 degrees in retail to about 14.4 degrees. This confirms the reported weaker tracking, and the largest cause is the formula substitution rather than weapon data or an explicit PvP turn-rate nerf.

The earlier 16.262 and 11.480 degree figures incorrectly evaluated the middle branch of the D1 piecewise formula. A 25 Hz frame time is below the `1/16` cutoff, so the first branch applies. The focused test contained the same incorrect expected values and used standard C `assert`, which was disabled by the RelWithDebInfo test build.

Git history provides no evidence that these changes were intentional PvP balance nerfs. Commit `a9ee22e7` replaced D2's retention calculation with the D1 formula under the code comment `Something's busted with the D2 code. Here's D1`, and changed D2's forced valid-target rescan to D1-style keep-current-target behavior while calling it a reversion to original release code. This suggests a mistaken compatibility or bug-fix rationale rather than deliberate weakening. Commit `f8ac5033` later moved scan phasing from object index to time since missile creation explicitly to make tracking more consistent between players. No commit message justifies leaving orientation scaled by render `FrameTime` after guidance became separately scheduled, so that appears to be an implementation omission. Likewise, the later D1-in-D2 work added D1 orientation and lifetime rules but left D2's compile-time cone constants in place, with no stated balance rationale. The quick-normalization transition documented by `592388ab` and `b6120b02` is not a retail nerf because the retail code also used quick normalization.

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

D1-in-D2 uses one runtime acquisition-threshold parameter. It retains the existing D2 `7/8` value in Redux mode and selects D1's retail `3/4` value only in Original mode. The selected value drives both acquisition and retention, so the restoration does not introduce a parallel guidance algorithm.

The default is off. The gameplay checkbox is `Original homing (Single/Coop)`, and the host advanced-network checkbox is `Original homing (Coop only)`.

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- D1 native CTest: 21/21 passed
- D2 native CTest: 24/24 passed
- Input demo regression set: 15/15 passed; all 11 D2 recordings used the dedicated headless runner, while the four D1 recordings used the runner's existing visual fallback
- D1 Windows x86 RelWithDebInfo full build: passed
- D2 Windows x86 RelWithDebInfo full build: passed
- Android `:app:assembleDebug`, including arm64-v8a, armeabi-v7a, x86, and x86_64 native builds: passed
- `git diff --check`: passed

## Remaining Limitation

There is no isolated automated flight-trajectory harness in the engine, so verification covers the formulas, policy gating, all native suites, and all production build targets rather than pixel-for-pixel missile paths. A manual gameplay comparison against DOS at a controlled 25 FPS would still be useful for subjective confirmation, especially for smart children and tight-corner reacquisition.
