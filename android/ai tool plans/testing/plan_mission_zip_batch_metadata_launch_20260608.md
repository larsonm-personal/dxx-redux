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

## Notes
- Planning only. Do not implement the orchestration in this tranche.
- Seed folder: `game_data/mission_files`.
- Current seed ZIPs: `descent_maximum_fixed.zip`, `ewithin-versions.zip`, `Obsidian.zip`, `plutonia.zip`.
- Reusable inspiration: `test_level_metadata_launcher_zip_reusable.json5` for metadata analysis, `test_mod_loading.json5` and `test_abort_game_to_main_menu_d2.json5` for game launch flow, `run_test.ps1` for parameter resolution, device provisioning, SetupActivity automation, and result watching.
- Main missing pieces: batch host runner, import-and-assert-mod-entry action, all-target metadata export for multi-mission ZIPs, and a game-side selector/assertion for the imported non-base mission.
