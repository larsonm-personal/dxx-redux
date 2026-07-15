# Original Homing Behavior Research and Restoration Plan

Date: 2026-07-14
Status: Follow-up required

## DescentBB Evidence Follow-up

- [x] Read the linked DescentBB thread and enumerate its concrete homing-missile claims
- [x] Follow cited posts, patches, commits, source releases, and named contributors where available
- [x] Compare the claims with retail D1/D2 source and Redux git history
- [x] Correct or extend the findings and BLUF interpretation where the evidence changes them
- [x] Record sources and unresolved claims in this plan
- [ ] Restore retail smart-child target repetition in Original mode without changing the Redux RNG path

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

History distinguishes older intentional Rebirth balance changes from the present 14.4-degree Redux retention cone. Commit `a9ee22e7` replaced D2 retention with the D1 formula and the D1 `3/4` base under the code comment `Something's busted with the D2 code. Here's D1`. That produced a 20.4-degree cone at 25 Hz, not the current 14.4-degree cone. Commit `5fee7673` later parameterized the update rate and silently fed D2's `7/8` base into that D1 formula, which produced today's 14.4-degree result. Its message only describes adding the D2 Homing Update Rate option, so the final narrowing appears to be a 2023 parameterization regression rather than an intentional PvP change. Commit `a9ee22e7` also changed D2's forced valid-target rescan to D1-style keep-current-target behavior while calling it a reversion to original release code. Commit `f8ac5033` later moved scan phasing from object index to time since missile creation explicitly to make tracking more consistent between players. No commit message justifies leaving orientation scaled by render `FrameTime` after guidance became separately scheduled, so that appears to be an implementation omission. Likewise, the later D1-in-D2 work added D1 orientation and lifetime rules but left D2's compile-time cone constants in place, with no stated balance rationale. The quick-normalization transition documented by `592388ab` and `b6120b02` is not a retail nerf because the retail code also used quick normalization.

### DescentBB thread follow-up

The 2016 thread reports that D2 homers did not turn sharply, rarely reacquired, and appeared unable to acquire targets much beyond 45 degrees. It links those observations to Drakona's work on frame-dependent retail trajectories and raises the possibility that the behavior was deliberate. Retail source resolves the angle claim: initial acquisition is limited to 41.4 degrees in D1 and 29.0 degrees in D2, so initial lock beyond 45 degrees was not retail behavior. Around-corner D1 shots can still work when the target begins within the wider D1 cone or when later geometry permits reacquisition.

The thread nevertheless points to real historical changes:

| Date | Change | Interpretation |
| --- | --- | --- |
| 2008 | Rebirth reduced homer turn rate after player feedback | Explicit balance change intended to make homers easier to dodge |
| 2011 | Rebirth reduced aggressiveness on Hotshot and above | Explicit survival and dodgeability tuning |
| 2012 | Rebirth prevented consecutive smart children from selecting the same target | Explicitly spread smart children and made homing projectiles easier to dodge |
| 2013 | Rebirth mixed D1 tracking rules into D2, then widened acquisition after tester feedback and partly to counter the afterburner | The afterburner rationale supported stronger homing, not a nerf |
| May to October 2016 | Rebirth restored D2's acquisition constant, D1/D2-specific tracking, forced D2 rescans, lifetime rules, and frame scaling | Independent confirmation that the earlier mixed implementation was historically inaccurate |
| 2014 | Retro adopted fixed D1/D2 homing ticks and used D1 retention rules in D2 | Removed retail D2 retention and rescan semantics; D2 retention was about 20.4 degrees at 25 Hz |
| 2023 | Redux passed D2's `7/8` base through the retained D1 formula while adding the update-rate option | Narrowed retention from about 20.4 to 14.4 degrees without a stated balance rationale |

The thread's afterburner speculation is therefore only partly right. Rebirth history explicitly mentions the D2 afterburner when justifying a temporary wider acquisition cone in 2013. It does not explain the weaker current cone. Some older Rebirth steering and smart-child changes were deliberate balance nerfs, but the strongest current D2 cone nerf came from the later formula and parameter mismatch.

The DXX-Retro issue cited by this research also explains the delayed or inconsistent reacquisition observation. Retail distributes broad scans across missiles with `(object_index ^ FrameCount) % 4`, so a missile can wait up to 160 ms in D1 or 133 ms in D2 before its next scan. Drakona characterized that per-missile delay as intentional retail tactical behavior; the networking problem was that object identities and clocks could disagree between clients. Redux commit `f8ac5033` changed the phase to time since missile creation for cross-client consistency. Original mode correctly restores the retail phase while Redux keeps the network-consistent phase.

One related retail difference remains outside the current Original mode. Retail independently chooses a target for every smart child and permits consecutive children to select the same object. Rebirth commit `1475ecc` added a reroll to avoid consecutive repeats, explicitly to distribute targets and make the projectiles easier to dodge, and current D1/D2 Redux still contains that reroll. Restoring the retail distribution should be a small Original-mode gate around the reroll, but it affects RNG consumption and therefore needs matching D1/D2 deterministic tests. Legacy demos should continue to default to Redux, while newly recorded Original-mode demos already preserve the setting.

Sources:

- [DescentBB discussion](https://www.descentbb.net/viewtopic.php?t=22701)
- [DXX-Retro issue 80](https://github.com/CDarrow/DXX-Retro/issues/80)
- [Retro 1.2.6 fixed-tick and D1-in-D2 tracking change](https://github.com/dxx-redux/dxx-redux/commit/a9ee22e77e10ad7d271ea3a6e9f5a39cee59e799)
- [D2 Homing Update Rate parameterization](https://github.com/dxx-redux/dxx-redux/commit/5fee7673f89a3a1189c4ce5813b70f9beba6b53d)
- [Network-consistent scan phasing](https://github.com/dxx-redux/dxx-redux/commit/f8ac5033e9bc959ffa47c747fe5fbee425feb3b6)
- [2008 Rebirth turn-rate balance change](https://github.com/dxx-rebirth/dxx-rebirth/commit/96f1a961f616428a5db39c7a2a9ba806362b9a06)
- [2011 Rebirth difficulty balance change](https://github.com/dxx-rebirth/dxx-rebirth/commit/ea16967383e8c2eeb0221878e24042a055b6c16d)
- [2012 Rebirth smart-child distribution change](https://github.com/dxx-rebirth/dxx-rebirth/commit/1475eccac4ae9f735b60b62ed471305a1f95a6e1)
- [2013 Rebirth D1 tracking merge into D2](https://github.com/dxx-rebirth/dxx-rebirth/commit/2b632008b2ecc242ef8380e99a757c61bb1ffb19)
- [2013 Rebirth D2 acquisition widening](https://github.com/dxx-rebirth/dxx-rebirth/commit/27a4a62b19fe29e7819fc4c4e11548b4ce26d89f)
- [2013 tester-requested shared acquisition constant](https://github.com/dxx-rebirth/dxx-rebirth/commit/f2cdd906a66efd7c1795493938f953038aa0e27d)
- [2016 Rebirth D2 acquisition restoration](https://github.com/dxx-rebirth/dxx-rebirth/commit/64400ff284bd4bece33ee2c78b9ff2a22ccdec20)
- [2016 Rebirth D1/D2 tracking restoration](https://github.com/dxx-rebirth/dxx-rebirth/commit/dad3e953e9731a8d87f128f651751e996dddb45a)
- [2016 Rebirth D2 rescan restoration](https://github.com/dxx-rebirth/dxx-rebirth/commit/89a2df0e1884d48293dc288360800a7f61520d08)
- [2016 Rebirth D1/D2 retention scaling restoration](https://github.com/dxx-rebirth/dxx-rebirth/commit/407805be429fe7d3caeee4ba9b3246cfee951304)

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

Original mode also does not yet restore retail smart-child target repetition. Current Redux deliberately prevents consecutive smart children from selecting the same target. Any restoration must preserve Redux's existing RNG path and cover both D1 and D2 with deterministic target-selection tests.
