# Test report 20260612 201722 failure triage

## Goal
Investigate the three failures and one timeout from
`temp/test_reports/report_20260612_201722.md`, preferring root-cause fixes over
longer timeouts.

## Plan
- [x] Read project instructions and report failure snippets
- [x] Inspect full logs and touched test scripts
- [x] Identify the most actionable failure cluster
- [x] Patch narrowly scoped root cause fixes
- [x] Run focused validation for changed tests
- [x] Run scoped code quality and record outcomes

## Initial failures
- `test_abort_game_to_main_menu_d2`: harness killed after 120 seconds while the
  runner said the script timeout should be 600 seconds.
- `test_door45_cover_gpu_regression`: missing
  `merged_wall_snapshot.target_cover_gpu.center_a`.
- `test_extract`: app-private staging failed while pushing
  `Descent [Mac].BIN` for `d1_mac_2nd_bin_cue`.

## Findings
- `test_abort_game_to_main_menu_d2` was killed by `run_all_tests.ps1` at its
  global 120 second process timeout even though `run_test.ps1` had computed a
  600 second launcher-script timeout and automation had just advanced to step
  19. The wrapper now derives JSON5 process timeouts from the script metadata
  and existing launcher minimum so it does not kill the child runner early.
- `test_door45_cover_gpu_regression` failed because the installed test run
  reported missing `target_cover_gpu.center_a`. The current source already
  serializes `center_a`, so this needs focused validation on the current APK
  before changing the merged-wall debug path.
- `test_extract` copied a 718912320 byte BIN into app-private storage but found
  only 635101184 bytes afterward. The staging helper now deletes partial
  app-private files and retries short writes, then prints device storage if all
  attempts fail.

## Validation
- `test_door45_cover_gpu_regression.json5` passed on the current installed APK.
  Final introspection included `target_cover_gpu.center_a=255`.
- `run_all_tests.ps1 -Filter test_abort_game_to_main_menu_d2` passed. The
  wrapper reported `timeout: 600s`, and the test completed in 35 seconds
  instead of being killed by the old 120 second wrapper timeout.
- `test_extract.ps1` for `d1 mac 2nd bin+cue` reached `RESULT: PASS`; the
  718912320 byte BIN staged successfully and the game reached `Lunar Outpost`.
  The outer tool command timed out after the PASS banner while the script was
  exiting or cleaning up, with no leftover validation process afterward; keep
  an eye on the extract cleanup path if this recurs.
- Scoped `run-code-quality.ps1 -Fix` passed for the touched PowerShell files
  and this plan, and `git diff --check` reported no whitespace issues.
