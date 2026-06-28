# Report 20260628 131121 Follow-up Plan - 2026-06-28

## Goal
Investigate `temp/test_reports/report_20260628_131121.md`, with special attention to any failures that were recently hardened, and apply contract-level fixes where the report shows a real remaining weak point.

## Tasks
- [x] Re-read repo instructions and current report context
- [x] Compare the report failures against the recently fixed cases
- [x] Inspect logs/scripts for the most relevant live failures
- [x] Implement focused robustness fixes where needed
- [x] Run targeted verification for affected tests
- [x] Run scoped code quality
- [x] Summarize outcome and residual risk

## Initial Triage
- Previously fixed and still passing: `test_all_extracts`, `test_merged_wall_two_pass_debug_mode_probe`, `test_merged_wall_two_pass_probe`, `test_mine_exit_movie_touch_skip`
- Previously hardened but failing again: `test_dpad_triggers`
- New shared launcher-state signature: several scripts fail looking for `Descent 1` or `Descent 2` buttons while the setup screen only exposes the generic game-file/import controls

## Fixes Applied
- `reset_state` now preserves `file_sets.json`; deleting it discarded the imported game-set identity and left later launcher scripts on the setup/import screen with no `Launch Descent 1/2` button
- `test_dpad_triggers` now asserts raw trigger axes plus direct joystick button latch state (`joy_buttons[19]` / `joy_buttons[21]`) instead of using frame-integrated `throttle_time` as a proxy
- Introspection now exposes `joy_buttons[]`, backed by a small D1/D2 `joy_get_button_state()` hook, and automation assertion failures dump it beside `raw_joy_axis[]`
- Automation now direct-dispatches `key` actions into the front `kconfig` window, matching the existing deterministic dispatch path for newmenu/listbox/game/automap and avoiding SDL-queue races for ESC/PageUp/PageDown in `test_controls_readability_d2`
- `test_pause_menu_return` now selects the mission for both games, matching the other robust launch-to-game scripts instead of relying on a D1 raw Enter at the mission/start-level boundary

## Verification
- `android\run-code-quality.ps1 -Fix -Paths ...` completed with all checks passing
- `android\gradlew.bat assembleDebug --no-daemon` completed successfully after code quality
- `test_pause_menu_return.json5 -Install` passed for D1 and D2; the logs show both `Launch Descent 1` and `Launch Descent 2` were tapped, so the original missing-launch-button reset failure is covered
- `test_dpad_triggers.json5` passed for D1 and D2; the trigger phase observed `joy_buttons[21]` and `joy_buttons[19]` latch on press and clear on release
- `test_controls_readability_d2.json5 -Game d2` passed; the logs show direct `kconfig` key dispatch and return to `menu_subtitle = Controls`
