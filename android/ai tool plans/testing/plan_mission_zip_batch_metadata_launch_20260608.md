# Mission ZIP batch metadata and launch planning

## Goal
Plan a reusable orchestration test for a folder of single-player mission or level ZIPs that imports each ZIP, captures metadata JSON, launches the mission, and writes full per-ZIP results for regression use.

## Plan
- [x] Re-read project instructions.
- [x] Inventory existing mission ZIP inputs.
- [x] Inspect current launcher automation and reusable metadata tests.
- [x] Identify missing automation/introspection hooks.
- [x] Design host-side orchestration, device-side script flow, and output schema.
- [x] Recommend phased implementation and regression integration.
- [x] Add launcher automation actions for clearing mods, importing a staged mission ZIP, and exporting all metadata targets.
- [x] Add game automation actions to assert/select the first non-base mission in the D2 mission list.
- [x] Add reusable JSON5 template for import/metadata/launch validation.
- [x] Add host-side batch runner for `game_data/mission_files`.
- [x] Build and smoke-test on emulator with the seed folder.

## Notes
- Seed folder: `game_data/mission_files`.
- Current seed ZIPs: `descent_maximum_fixed.zip`, `ewithin-versions.zip`, `Obsidian.zip`, `plutonia.zip`.
- Reusable inspiration: `test_level_metadata_launcher_zip_reusable.json5` for metadata analysis, `test_mod_loading.json5` and `test_abort_game_to_main_menu_d2.json5` for game launch flow, `run_test.ps1` for parameter resolution, device provisioning, SetupActivity automation, and result watching.
- Implemented files:
  - `android/helpers/run_mission_zip_batch.ps1`
  - `android/game_scripts/test_mission_zip_batch_import_metadata_launch.json5`
  - `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt`
  - `android/app/src/main/cpp/shared/game_automate.cpp`
- The batch runner writes per-ZIP metadata arrays, import summaries, automation logs, final introspection, `summary.jsonl`, and `summary.json` under `android/temp/mission_zip_batch/<timestamp>/`.
- ZIPs larger than 500 MB are skipped by default and recorded as `skipped_large`; pass `-IncludeLarge` to include them.
- Smoke test output: `android/temp/mission_zip_batch/20260608_232715`.
  - `descent_maximum_fixed.zip`: passed, two metadata targets, 36 total levels.
  - `Obsidian.zip`: passed.
  - `plutonia.zip`: passed, one metadata target, 32 levels.
  - `ewithin-versions.zip`: skipped-large.
