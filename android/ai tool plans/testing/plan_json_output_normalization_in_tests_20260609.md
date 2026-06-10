# JSON Output Normalization In Tests - 2026-06-09

## Goal
Make regression JSON outputs pretty-printed and normalized at the test output write sites, so generated files are commit-ready without a separate formatter.

## Plan
- [x] Locate mission ZIP metadata and secret area baseline JSON producers.
- [x] Add producer-side JSON pretty-printing for level metadata automation outputs.
- [x] Add producer-side JSON pretty-printing for mission ZIP batch local copies.
- [x] Add producer-side JSON pretty-printing for secret area dump and baseline outputs.
- [x] Validate with focused generation/parsing checks and scoped code quality.

## Notes
- Mission ZIP metadata is written by the launcher metadata automation and copied by `run_mission_zip_batch.ps1`.
- Secret area baseline JSON is produced by the headless C++ dumper, then assembled/compared by `test_secret_area_baseline.ps1`.
- The batch wrapper now normalizes saved `.json` artifacts inside the test run instead of relying on a later formatter.
- Focused mission ZIP generation passed for `Chasm.zip` and produced a pretty-printed `game_data/mission_files/Chasm.json`.
- Focused secret-area baseline update/compare passed against a temp D1 baseline.
- `:app:assembleDebug` passed after the Kotlin/JNI output changes.
