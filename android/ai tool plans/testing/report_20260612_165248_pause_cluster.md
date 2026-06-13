# Report 20260612 165248 pause cluster

## Goal
Focus on two pause-related failures from `temp/test_reports/report_20260612_165248.md`
without increasing test timeouts.

## Selected Tests
- [x] `test_pause_menu_return`
- [x] `test_pause_menu_viewport_d2`

## Plan
- [x] Inspect the selected scripts and per-test logs
- [x] Identify whether failures are intro-state, pause-state, emulator health, or script sequencing
- [x] Patch deterministic script or harness setup only if the cause is clear
- [x] Run focused validation for the selected tests
- [x] Run scoped code quality for touched files
- [x] Record outcome and related remaining failures

## Notes
- `test_pause_menu_viewport_d2` failed at the immediate post-launch
  `screen_mode=menu` wait and did not pin or consume intro/movie state.
- `test_pause_menu_return` already pins and skips intro state; the report log only
  contains the outer test-kill line, so it needs an isolated rerun for useful
  step-level evidence.
- The viewport script now explicitly sets `skip_intro_movie=false`, waits for
  `intro_active=true`, and skips the intro before waiting for the main menu.
  Existing wait budgets were left unchanged.
- Focused validation:
  - `test_pause_menu_viewport_d2.json5` passed after the intro-state fix.
  - `test_pause_menu_return.json5` passed unchanged for both D1 and D2 in an
    isolated suite-style run; the report log had no step-level failure details.
- Scoped code quality passed for the viewport script and this plan file.
