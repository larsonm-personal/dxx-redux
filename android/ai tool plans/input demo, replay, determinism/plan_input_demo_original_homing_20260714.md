# Input Demo Original Homing Setting Plan

Date: 2026-07-14
Status: Complete

## Objective

Persist the effective Original Homing mode in new `.dximdemo` recordings, restore it during replay, and make demos written before the new field default to Redux homing.

## Plan

- [x] Read repository instructions and locate the input-demo format and runtime setup paths
- [x] Identify the smallest backward-compatible metadata extension and its existing defaulting behavior
- [x] Add shared codec/fixture support with an absent-field default of Redux
- [x] Capture and apply the setting symmetrically in D1 and D2
- [x] Extend focused recorder, fixture, and replay tests
- [x] Run scoped code quality, native tests, and Windows/Android build verification
- [x] Record the final format behavior and verification results here

## Format Decision

The current input-demo format already stores simulation-relevant player settings in the header's optional `player_cfg` object. New recordings now add:

```json
"original_homing": 0
```

or value `1` when Original Homing is enabled. The outer demo format version is unchanged because this is an optional additive member of the existing version 3/4 header structure.

The shared player-config parser zero-initializes its structure before reading members. Therefore an older `player_cfg` object with no `original_homing` member resolves to Redux. Replays with no `player_cfg` object also explicitly set `PlayerCfg.OriginalHoming` to zero before level or checkpoint startup, so the local pilot preference cannot alter an old recording.

For new-level replays, the recorded value is applied before `StartNewGame`, which initializes the homing scheduler. For checkpoint replays, the value is set before restoring the checkpoint so the restore's level initialization uses the recorded mode, then it is reapplied after the local player profile is reloaded.

## Verification

- Scoped code-quality formatting and checks passed
- Windows D1 and D2 full builds passed through `run-windows-build.ps1`
- D1 native CTest suite passed, 21/21
- D2 native CTest suite passed, 24/24
- Android `:app:assembleDebug` passed for all configured ABIs
- Recorder tests verify that new D1 and D2 headers write `original_homing: 1`
- Fixture parsing verifies that a legacy `player_cfg` without the field defaults to zero
- Replay tests verify that the new field survives codec and replay-session round trips

## Runtime Corpus Note

The committed regression corpus was attempted as an additional check. The first D1 visual replay timed out after 180 seconds, and the first D2 checkpoint replay hit a Windows access violation inside checkpoint restoration after the header parsed and before level startup. These runtime harness failures prevented a complete 15-demo live corpus result in this environment. The focused legacy parser test and all native suites pass.
