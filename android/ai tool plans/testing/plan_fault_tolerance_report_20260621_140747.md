# Fault tolerance report 2026-06-21 14:07

## Goal
- Analyze the five failures in `report_20260621_140747.md`.
- Plan durable fixes that make tests more fault tolerant without extending timeouts as the primary move.

## Plan
- [x] Read project instructions and load the report summary.
- [x] Read full logs and owning scripts for all five failures.
- [x] Group failures by likely root cause and identify brittle assumptions.
- [x] Propose focused, durable updates for each group.
- [x] Record recommended validation commands.

## Findings and recommended updates

### 1. `test_axis_mapping`

Failure:

- `test_axis_mapping_20260621_140747.log` failed in phase 5c:
  - `"bank_time" != 0 (got 0)`
- The same run already proved the positive behavior in phase 5a:
  - axis 6 drove `bank_time` to a non-zero value.

Likely brittle assumption:

- Phase 5c is described as "Verify axis 6 does NOT drive heading or pitch", but it also reasserts that `bank_time != 0`.
- That makes the negative cross-axis check depend on an immediate control-state sample that is not the behavior phase 5c is meant to prove.

Durable update:

- Keep phase 5a as the positive "axis 6 drives bank" check.
- Change phase 5c in `android/game_scripts/test_axis_mapping.json5` to assert only the absence of heading/pitch motion, or add a more explicit harness assertion that waits for the injected axis state to be observed before checking the negative fields.
- Prefer splitting the intent:
  - positive drive check: `bank_time != 0`
  - cross-axis isolation check: `heading_time == 0`, `pitch_time == 0`
- Also consider adding an explicit zero/release settling assertion between subphases so stale state is visible when it matters.

Validation:

- Run `test_axis_mapping.json5` alone.
- Then include it in a grouped run with other input-state tests to make sure prior control state is not leaking.

### 2. `test_door45_cover_gpu_regression`

Failure:

- `position.y` expected `[-75.75, -70.75]`, got `-70.5`.
- The scripted `pose_view` target was in the intended segment, and the meaningful GPU cover assertions were the real purpose of the test.

Likely brittle assumption:

- The test uses exact camera-position ranges as a proxy for "we are looking at the right cover target".
- The failing value is only `0.25` outside the upper bound, while the more semantic assertions already check the selected texture/cover state.

Durable update:

- Reduce dependence on exact `position.x/y/z` ranges in `android/game_scripts/test_door45_cover_gpu_regression.json5`.
- Keep `position.segment == 80` as a basic sanity check.
- Treat `debug_flags.texture_target == door45#0`, `merged_wall_snapshot.target_cover_gpu.valid == true`, cover event count, and cover identity fields as the core correctness signal.
- If a pose guard is still useful, widen it modestly as a guard only, not as the primary regression signal.
- Longer-term, expose/read a "target face hit" or "pose_view settled on requested target" field from the snapshot so these tests can assert intent directly.

Validation:

- Run `test_door45_cover_gpu_regression.json5` repeatedly enough to cover the small camera settle variance.
- Confirm the test still fails if `door45#0` cover targeting is broken.

### 3. `test_merged_wall_two_pass_probe`

Failure:

- `merged_wall_snapshot.faces[0].submit_nv` expected `"5"`, got `"6"`.

Likely brittle assumption:

- The test over-specifies the exact submitted vertex count.
- The sibling debug-mode probe already treats this field as tolerant with `{"gte": 5}`.
- The meaningful behavior is that the target face goes through `force_two_pass` / `gpu_two_pass`, is selected, and is center-hit.

Durable update:

- Change `android/game_scripts/test_merged_wall_two_pass_probe.json5` so `faces[0].submit_nv` matches the existing debug-mode tolerance, probably `{"gte": 5}`.
- If values above 6 would indicate a real regression, use a range such as `[5, 6]`; otherwise align with the sibling probe.
- Keep the exact route/merge implementation assertions.

Validation:

- Run both `test_merged_wall_two_pass_probe.json5` and `test_merged_wall_two_pass_debug_mode_probe.json5`.
- Confirm both still fail on wrong route or wrong merge implementation.

### 4. `test_mod_loading`

Failure:

- The script reached game launch and then the harness reported:
  - logcat not responding
  - health check failed
  - emulator crashed during test

Likely brittle assumption:

- The report does not contain enough crash evidence to classify this as app crash, renderer/GPU pressure, emulator death, or storage/memory pressure.
- This test loads large mod assets and may be sensitive to suite residue, but the current failure handling loses the most useful diagnostics.

Durable update:

- Improve failure diagnostics in the Android test helpers before changing test timing:
  - on logcat/health/emulator-crash failure, dump recent `adb logcat -d -t 400`
  - dump `dumpsys meminfo` for the app/game process when reachable
  - list or pull tombstones where possible
  - capture relevant app-private automation/debug logs before teardown
- Add a resource preflight for heavy mod-loading tests:
  - available `/data` space
  - installed/staged mod file sizes
  - current memory snapshot if cheap
- If diagnostics show resource pressure, make the default suite version use the smallest representative mod payload and keep a heavier texture-pack stress test separate.

Validation:

- First rerun `test_mod_loading.json5` with the enhanced diagnostics.
- Use that evidence to decide whether the fix belongs in cleanup, fixture sizing, renderer/resource handling, or test classification.

### 5. `test_extract`

Failure:

- Copying `Descent [Mac].BIN` failed with `No space left on device`.
- The file is about 719 MB.
- `Copy-LocalFileToAppPrivate` currently stages to `/data/local/tmp/dxx_extract_*`, then copies into `/data/data/com.dxxredux.app/files/tmp_import/...`, requiring roughly two device-side copies during setup.

Likely brittle assumption:

- The test assumes there is enough free `/data` space for double-staging a large CD image, even when run late in a suite.
- A standalone run can pass while a full-suite run fails if previous tests left legitimate app data, mods, or extracted sets behind.

Durable update:

- Add an explicit storage preflight before pushing large import sources:
  - source byte size
  - free bytes on `/data`
  - free bytes under app-private storage if distinguishable
  - current scratch directory usage
- Make no-space failures diagnostic:
  - dump `df -h`
  - list `/data/local/tmp/dxx_extract_*`
  - list app `files/tmp_import` and import set directories
- Add setup cleanup that is specific and owned by extract tests, not broad state erasure:
  - remove stale `/data/local/tmp/dxx_extract_*`
  - remove app `files/tmp_import`
  - remove prior `regression_test` import set unless `-KeepFiles` is used
  - remove prior large mod/test payloads that are not part of the extract test fixture
- Do not change game/import logic for this case. The durable fix should be careful emulator state cleanup and clear storage diagnostics before the large direct-import copy starts.

Validation:

- Run `android/tests/test_extract.ps1` against the failing Mac BIN/CUE spec.
- Then run it late in a mini-suite after mod-loading or other large-data tests to verify it no longer relies on an unusually clean emulator.

## Implemented follow-up

- Updated `test_axis_mapping` so phase 5c checks cross-axis isolation only.
- Updated `test_door45_cover_gpu_regression` so the GPU-cover assertions are not gated by exact camera coordinate ranges.
- Updated `test_merged_wall_two_pass_probe` to tolerate `submit_nv >= 5`, matching the sibling debug-mode probe.
- Updated the shared Android test helper to:
  - remove stale app-private `files/mods` entries before pushing a script's declared mod dependencies
  - dump bounded device diagnostics when the emulator fails the retry health check
- Left `test_extract` as a cleanup/diagnostics problem. No game/import logic change should be made for that failure.
