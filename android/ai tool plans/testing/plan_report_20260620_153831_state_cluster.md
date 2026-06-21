# Report 2026-06-20 153831 state cluster

## Goal
- Investigate `temp/test_reports/report_20260620_153831.md`
- Prefer durable suite-state fixes over isolated timeout changes
- Identify common state contamination across the nine failures
- Patch the narrowest shared boundary and verify with clustered tests

## Plan
- [x] Read repo instructions and load the report
- [x] Compare failing logs for shared state and first divergence
- [x] Select the highest-leverage shared root cause
- [x] Patch code or harness state reset, not per-test timeouts
- [x] Verify with focused cluster runs and at least one suite-wrapper filter
- [x] Run scoped code quality
- [x] Record results and residual risk

## Initial Read
- Failures: `test_autosave_resume_missing_pilot_unified`, `test_autosave_resume_unified`, `test_axis_mapping`, `test_death`, `test_door45_cover_gpu_regression`, `test_dpad_triggers`, `test_engine_prefs_unified`, `test_keyboard_defaults`, `test_launch_to_automap`.
- Automated LAN tests passed; skipped `test_lan_discovery` and `test_manual_lan_coop` are manual/interactive skips, not failures.
- Working hypothesis: these failures share leaked launcher/game state or incorrect game selection rather than short waits.

## Patch 1
- The repeated D1 failures were not on the mission picker; logs showed the level-number dialog (`You may start on any level up to 27`) after `New game`.
- Made the `${MISSION}` select optional in the affected unified scripts so the default built-in mission path can proceed whether the engine shows a mission picker or skips directly to level selection.
- This does not extend waits; it fixes the script's state assumption.

## Verification
- `android/helpers/run_test.ps1 test_death.json5 -Game d1` passed after the patch (`EXIT: 0`). This rerun exercised the mission-picker path and reached `PASS (file-based, 32/31 steps, 4220ms)`.
- `android/run_all_tests.ps1 -Filter test_launch_to_automap` no longer failed at the D1 mission select. It selected `Descent: First Strike`, entered game, opened and closed the automap, then timed out after `SCRIPT_BACKGROUND: ready` while waiting for `egl_recreate_count = 2`.
- That suite-wrapper timeout is a separate background/resume harness issue, not evidence for increasing waits on the mission select.
- Scoped code quality passed for the touched JSON5 scripts.

## Residual Risk
- The optional mission step has been applied to the repeated D1 launch-state failures from the report, but only `test_death` was rerun successfully end to end after the emulator was restarted.
- `test_launch_to_automap` still needs a separate fix around the background/resume handshake or watcher behavior.

## Optional Mission History Check
- [x] Check whether `${MISSION}` select has toggled between required and optional in the affected scripts.
- [x] Interpret what that churn means for the harness design.

### Findings
- `test_engine_prefs_unified.json5` and `test_launch_to_automap.json5` have committed required -> optional -> required history, plus the current worktree change back to optional.
- The other six affected scripts have committed required history and the current worktree change to optional.
- `optional` select waits until its timeout before skipping, so it can both mask an unexpected screen and add delay if used as a broad uncertainty escape hatch.
- The durable fix should probably make the D1/D2 mission-picker branch explicit in the harness or shared script helper, instead of letting many tests independently decide whether `${MISSION}` is required.

## Patch 2 Plan
- [x] Add a shared automation action for the optional mission-picker branch.
- [x] Rewrite affected scripts to use that explicit action instead of raw `optional` selects.
- [x] Run scoped formatting/linting.
- [x] Validate with at least one D1 launch-path test.

### Patch 2 Result
- Added `select_mission` to `android/app/src/main/cpp/shared/game_automate.cpp`.
- The action selects `${MISSION}` when the mission picker is present.
- If the picker has already been skipped, it only advances when the front menu is the start-level menu: subtitle contains `select starting level`, a text row contains `you may start on` and `level up to`, and the menu has an input item.
- Any other missing mission item remains a failure instead of a generic optional skip.
- Replaced all `${MISSION}` menu selections under `android/game_scripts` with `select_mission`.

### Patch 2 Verification
- `android/run-code-quality.ps1 -Fix -Paths android/app/src/main/cpp/shared/game_automate.cpp android/game_scripts` passed after formatting the C++ file.
- JSON5 scoped quality pass over the touched scripts passed.
- `android/gradlew.bat assembleDebug` passed.
- `android/helpers/run_test.ps1 test_death.json5 -Game d1` passed against the rebuilt APK. This run exercised the mission-picker selection branch of `select_mission`.
- The start-level skip branch was compile-verified but did not naturally occur in the focused emulator rerun.
