# Next Remaining Failure Plan - 2026-06-28

## Goal
Continue the report hardening sequence by identifying the next still-unhandled failing test and applying a transformational robustness fix rather than timeout or retry tweaks.

## Tasks
- [x] Re-read current instructions, report context, and workspace status
- [x] Identify the next remaining failure that still needs work
- [x] Inspect the relevant scripts/helpers and choose the contract-level fix
- [x] Implement the focused change
- [x] Run targeted verification
- [x] Run scoped code quality and sanity checks
- [x] Summarize outcome and residual risk

## Current Finding
No newer report exists. Four of the five report failures pass in the current tree. `test_all_extracts` still fails because the app-private source staging path streams file contents through `adb exec-in`, and the stream truncates at varying byte counts across attempts. Replace that transfer path with the existing file-based chunk staging contract for all source sizes.

## Verification
- `test_dpad_triggers.json5`: pass
- `test_merged_wall_two_pass_debug_mode_probe.json5`: pass
- `test_merged_wall_two_pass_probe.json5`: pass
- `test_mine_exit_movie_touch_skip.json5`: pass
- `android/tests/test_all_extracts.ps1 -Seed 342861260`: failed before the staging change, then passed after moving all app-private source copies to file-based chunk staging
- `android/run-code-quality.ps1 -Fix -Paths android/tests/test_extract.ps1 android/ai tool plans/testing/plan_next_remaining_failure_20260628.md`: pass

## Outcome
The remaining live failure from the report was `test_all_extracts`. The fix removes `adb exec-in` stdin streaming from the app-private source staging path and uses file-based chunk staging for all sizes, with chunk and accumulated app-private size checks before the atomic rename.
