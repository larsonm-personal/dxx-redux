# Host build blank thumbnail fix

## Goal
Restore Windows host builds after the autosave thumbnail flag was left Android-only while the save path still references it for all builds.

## Checklist
- [x] Confirm the failing symbol and compare D1 and D2 ownership
- [x] Make the blank-thumbnail flag available to host builds in both D1 and D2
- [x] Validate the D1 host rebuild and the headless runner entry point

## Validation notes
- `android/tests/run_input_demo_headless.ps1` now rebuilds D1 and D2 cleanly instead of failing in `d1/main/state.c`
- Non-interactive smoke replays for `temp/input_demo_runtime_smoke/d1/smoke.dximdemo` and `temp/input_demo_runtime_smoke/d2/smoke.dximdemo` launch successfully, then fail later on existing result-compare mismatches rather than build errors
