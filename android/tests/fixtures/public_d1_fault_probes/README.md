# Ordinary-start D1 fault demos

Five small input-only demos use the pinned Descent 1 v1.4 shareware data. No saves,
warps, injected object state, inventory grants or cheats are included. Each input
and each separate oracle is at most 100000 uncompressed bytes.

These are a separate diagnostic corpus. Existing public and retail gates are
unchanged. A reproduced known fault is **not** a full imported-D1 parity pass.

## What they show

| Demo                     | Frames | Input bytes | Observation                                                      |
| ------------------------ | -----: | ----------: | ---------------------------------------------------------------- |
| `d1-monitor-phase`       |    181 |       31665 | Imported real monitor animation starts one frame early           |
| `d1-self-splash`         |    139 |       21824 | One concussion against a solid wall; bounded gameplay matches    |
| `d1-self-splash-no-fire` |    139 |       21768 | Actual live no-fire control; no missile or player damage         |
| `d1-mine-blast`          |    424 |       73186 | Concussion force moves a live proximity mine only in imported D1 |
| `d1-mine-no-concussion`  |    424 |       73166 | Actual live control preserves the mine trajectory across engines |

The first three start level 1 on difficulty 2. Mine probes start level 3 on the
ordinary trainee difficulty, collect a real proximity pickup, clear nearby
robots with lasers, drop a mine, strafe, then fire or withhold the concussion.
The mine route is about 26.5 seconds at its recorded fixed timestep.

### Monitor startup phase

The first meaningful difference precedes movement: eclip 24 starts with timer
4369 in native D1 and 0 in imported D1. After the first simulation step, texture
349 selects native bitmap 881 versus imported 882. The authored asset is
`misc066.abm`, not a skipped shareware placeholder.

At frame 170, both cameras are in segment 11 at position
`[1590, -1324467, 7875461]`, facing `[65439, 3007, -1882]`. Native D1 shows
`misc066#4` / bitmap 885; imported D1 shows `misc066#5` / bitmap 886. Full uncropped
640x480 images are retained:

- [Native D1 frame 170](screenshots/d1-monitor-frame170.png)
- [Imported D1 frame 170](screenshots/imported-d1-monitor-frame170.png)

All 3764 high-contrast differing pixels lie in the monitor region. Lower-contrast
image differences elsewhere remain untouched; complete pixel parity is not
claimed. Collision and transparency consequences are unproven.

Native live phase/camera/control comparison passes. Imported live phase comparison
has 1812 raw differences and matches only the explicit startup-zero / subsequent
one-frame-offset fault. Native console and headed phase traces pass; imported
trace comparison has 728 differences in its declared timer/frame/bitmap scope.
Wrong native phase, absent fault and wrong offset controls all reject. Screenshot
on/off native captures have identical full recording, RNG and compact input bytes.

### Concussion force on a live mine

Pickup is frame 377, mine drop 399, and concussion fire 407. Mine slot 130 /
signature 323 agrees physically through raw boundary 409. At boundary 410, after
simulation frame 409, velocity is `[79237, -44531, 89760]` in native D1 versus
`[-353071, 99215, -21970]` in imported D1. Both mines remain alive.

Native `d1/main/fireball.c` excludes weapons from radial explosion force. D2's
`d2/main/fireball.c` explicitly includes proximity mines without an imported-D1
guard. The separate no-concussion capture matches all 25 mine trajectory samples.
The control has the same native history before the intervention; it carries its
own observed RNG afterward.

All 424 summary gameplay/RNG boundaries and terminal results still match. This is
an intentional regression against summary-only false positives. The detailed
mine oracle checks live identity, parent binding, lifetime and trajectory.
Raw player/parent signatures remain 1 in D1 versus 0 in imported D1. Strict SIM
actor-context comparison also fails; algorithm values, ordering and carried RNG
are reported separately. No identity fields are discarded to obtain a pass.

## Replaying

Use the executables and pinned assets supported by
[`run_input_demo_replay.ps1`](../../run_input_demo_replay.ps1). For example,
from the repository root on Linux:

```sh
pwsh -NoProfile -File android/tests/run_input_demo_replay.ps1 \
  -DemoPath android/tests/fixtures/public_d1_fault_probes/d1-mine-blast.dximdemo \
  -Game d1 -DataEdition shareware -DataDir "$D1_DATA" \
  -ExecutablePath "$D1_HEADLESS" -Runner headless-console -Mode accelerated \
  -StrictComparison -RequireCleanExit \
  -StateLogPath "$OUTPUT/state.jsonl.gz" -RngLogPath "$OUTPUT/rng.jsonl" \
  -ResultCopyPath "$OUTPUT/result.json"
```

For visible native playback, select the normal D1 executable, `-Runner visual`
and `-Mode realtime`. Use a working display or Xvfb. A passing terminal result
alone does not check monitor phase or mine trajectory.

### Imported comparison profile

Native D1 records missile camera disabled; normal D2 defaults to enabled. Keep the
strict projection guard. In a new profile directory, run the clean native D2
fixture executable with `--public-d1-comparison-profile`. It writes `d1match.plr`
and `.plx` through the engine's own writer, checks only one preference byte changes,
and verifies native readback. Later normal gameplay can update mission progress
and other saved pilot preferences; the one-byte claim describes profile creation.
Copy only those named files to
`$PROFILE_HOME/.d2x-redux/`.

Replay the unchanged input with the normal D2 executable, `-Game d2 -D1InD2
-Pilot d1match -Runner visual`. On Unix the wrapper copies this named pilot from
its inherited HOME into a fresh child HOME. When using an Xvfb wrapper that itself
sets HOME, place `env HOME="$PROFILE_HOME"` **after** the Xvfb wrapper. The native
console runtime does not load pilots, so these unchanged projection contracts
currently require headed imported playback. Normal production defaults remain
unchanged.

## Checking the fault rather than just the ship summary

The source recipes, capture helper and checker contracts are documented in
[`public_d1_fault_probes.md`](../../public_d1_fault_probes.md).

- `compact_public_replay.verify` checks native replay against each immutable live
  summary oracle, including full per-frame diagnostics and SIM event history
- `check_public_effect_phase.py compare-trace` checks raw replay phase fields using
  `d1-monitor-phase.detail.oracle.json`; `compare` checks full live phase/camera
  observations. `--expect-known-imported-fault` still returns exit 1 for the raw
  imported mismatch, and separately identifies the exact known pattern
- `check_public_mine_blast.py compare` checks positive and negative paired traces
  against their native-only detailed oracles. Both raw imported reports remain
  failed. Its `causal` command returns `known_fault_reproduced` only for the exact
  documented force difference plus the physically matching no-concussion control
- `test_public_d1_fault_tools.ps1` runs the offline checker and corpus-integrity
  tests; it does not substitute for executing the game

Full comparison reports in `evidence/` retain mismatches. Large phase reports use
lossless gzip, with compressed and uncompressed hashes in `manifest.json`.
The strict ordered-input decoder compares compact aliases and full capture inputs
by their actual operations, residual controls and timesteps. Original input spelling
differences remain in `raw_control_encoding`; native oracle bytes are unchanged.
Malformed evidence or a runner failure cannot stand in for a reproduced fault.

## Provenance and limits

All recordings came from independently rebuilt published source
`8b19ab9ff29c5f54d815cc735b37c8e1ddf3c3fb`, retained at branch
`capture-public-d1-fault-probes`. The followup checker revision
`c1b8099733abd602ceca60afa5669f1937233cba` corrects acceptance of the actual frame
170 screenshot landmark; no native source, recording, oracle expectation or
engine behavior changed for that correction.

Fifteen executions cover five inputs with native true-console, native headed and
matched-profile imported headed runtimes. All ten native executions match their
independent live summary/RNG oracles. Both native backends reproduce the same
imported mine force fault. Self-splash's actual no-fire input is rejected by the
positive native oracle. No engine repair is included.

`provenance/` contains capture/build hashes and per-file raw evidence manifests.
The full live recordings, raw world/RNG traces, logs and all route screenshots
remain in the capture workspace at the paths recorded in the manifest; they are
not replaced by these summaries. Native expectations were never regenerated from
an imported replay. Broader imported-D1 fidelity remains unqualified.
