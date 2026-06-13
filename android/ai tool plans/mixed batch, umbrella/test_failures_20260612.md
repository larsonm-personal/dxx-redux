# Test failures 2026-06-12

## Goal
Fix or narrow the non-passing tests from `temp/test_reports/report_20260612_143255.md`.

## Failures
- [x] `test_abort_game_to_main_menu_d2`
- [x] `test_dpad_triggers`
- [x] `test_lunar_series_revamped_metadata_only`
- [x] `test_mod_loading`
- [x] `test_secret_area_baseline`

## Plan
- [x] Inspect logs, scripts, and recent state for each failure
- [x] Fix deterministic data or script issues first
- [x] Fix app or test harness behavior where failures point to code regressions
- [x] Run focused tests for changed behavior
- [x] Run scoped code quality on touched files
- [x] Record final verification and remaining risks

## Notes
- `test_abort_game_to_main_menu_d2` inherited `skip_intro_movie=true` from prior tests, then waited for `intro_active=true`. The script now forces `skip_intro_movie=false`.
- `test_lunar_series_revamped_metadata_only` imported from `files/mission_zip_batch_cache` without declaring the ZIP dependency. The script now declares and imports `Lunar Series Revamped.zip`.
- `test_mod_loading` declared stale DXA hashes. The script now matches the local `game_data/mods/d2x-xl` assets.
- `test_dpad_triggers` hit the suite wrapper's 240 second cap while D2 was still progressing slowly. The per-test cap is now 420 seconds.
- `test_secret_area_baseline` was schema drift only: the current dumper adds empty `travel_note` and `notes` fields. The baseline was regenerated.

## Verification
- `android/tests/test_secret_area_baseline.ps1 -RequireAssets`: pass.
- `android/helpers/run_test.ps1 -ScriptName test_lunar_series_revamped_metadata_only.json5`: pass.
- `android/helpers/run_test.ps1 -ScriptName test_abort_game_to_main_menu_d2.json5`: pass.
- `android/helpers/run_test.ps1 -ScriptName test_mod_loading.json5`: pass.
- `android/run_all_tests.ps1 -Filter test_dpad_triggers`: pass in report `temp/test_reports/report_20260612_161919.md`.
- `android/run-code-quality.ps1 -Fix -Paths ...`: pass for touched files.
