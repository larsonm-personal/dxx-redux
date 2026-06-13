# Report 20260612 165248 intro-state cluster

## Goal
Focus on one small failure cluster from `temp/test_reports/report_20260612_165248.md` without increasing test timeouts.

## Selected Tests
- [x] `test_controls_bottom_stage_d2`
- [x] `test_kconfig_keyboard_stage_d2`

## Plan
- [x] Inspect the selected scripts and per-test logs
- [x] Confirm whether the first wait is blocked by intro/movie state or emulator health
- [x] Patch scripts or harness state handling only if the cause is deterministic
- [x] Run focused validation for the selected tests
- [x] Run scoped code quality for touched files
- [x] Record outcome and remaining related failures

## Notes
- Both selected tests timed out immediately after launching Descent 2 while waiting for
  `screen_mode=menu`.
- The scripts reset save state but did not pin or consume intro/movie state, so the
  first menu wait could inherit launcher preference state from previous runs.
- The fix explicitly sets `skip_intro_movie=false`, waits for `intro_active=true`,
  and skips the intro before waiting for the main menu. Existing wait budgets were
  left unchanged.
- Focused validation passed for both selected scripts:
  - `test_controls_bottom_stage_d2.json5`
  - `test_kconfig_keyboard_stage_d2.json5`
- Scoped code quality passed for the two scripts and this plan file.
- `test_pause_menu_viewport_d2` appears to be a related sibling because it failed
  at the same immediate post-launch menu wait, but it was intentionally left
  outside this focused one-or-two-test pass.
