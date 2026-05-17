# Plan: Remaining test failures triage 2026-05-16

## Goal

- Reproduce the remaining report failures on the current build, separate stale report entries from real regressions, fix the smallest local causes, and validate with fresh reruns.

## Current report candidates

- `test_autosave_resume_missing_pilot_unified`
- `test_autosave_resume_unified`
- `test_axis_mapping`
- `test_death`
- `test_dpad_triggers`
- `test_engine_prefs_unified`
- `test_keyboard_defaults`
- `test_launch_to_automap`
- `test_pause_menu_return`
- `test_lan`

## Working assumptions

- `test_launcher_dpad`, `test_lan_lobby_discovery`, and `test_mp` already have fresher passing validation than this report
- Some listed FAIL entries may be stale, cross-test contamination, or later-phase failures not visible in the clipped report snippets
- The likeliest current real failures were `test_death`, `test_lan`, and either autosave resume script if launcher state was still leaking between tests

## Validated outcomes

- `test_death` is non-repro on a fresh isolated rerun and currently passes
- `test_lan` was a real harness failure and now passes after switching launch validation to game introspection instead of stale host log lines
- `test_autosave_resume_unified` was a real D1 regression: automation `enter_launcher` queued an exit autosave that D1 never consumed before the launcher resumed. Fixed by making automation save and post `SDL_QUIT` synchronously on the game thread. Fresh D1 rerun passes, and the earlier combined rerun advanced past this test
- `test_autosave_resume_missing_pilot_unified` advanced past the former D1 briefing/resume failure during the sequential rerun and did not reproduce afterward
- `test_pause_menu_return` advanced cleanly during the sequential rerun and is currently treated as stale/non-repro
- `test_launch_to_automap` had a real D1 script failure. First it needed the same D1 post-difficulty `Escape` handling as other D1 scripts; then the extra `Escape` was found to open the D1 in-game Game Menu. The script now uses a single D1 `Escape`, and the fresh D1 rerun passes
- `test_axis_mapping`, `test_engine_prefs_unified`, and `test_keyboard_defaults` all advanced through the final sweep without reproducing the report failures

## Steps

- [x] Pull the remaining failure sections from the report for `test_autosave_resume_unified` and `test_lan`
- [x] Fresh-rerun the highest-signal real candidates to confirm current failures
- [x] Fix the most local reproducible failure and rerun that test
- [x] Continue through remaining reproducible failures while skipping stale/non-repro entries
- [x] Update this plan with validated outcomes

## Remaining follow-up

- `test_dpad_triggers` was listed in the original report but was not rerun in this tranche
