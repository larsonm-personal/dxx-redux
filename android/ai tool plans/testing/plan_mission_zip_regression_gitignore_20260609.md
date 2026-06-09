# Mission ZIP regression gitignore rules

## Goal
Make generated mission ZIP regression metadata JSON files available to commit while keeping proprietary or bulky mission ZIP inputs excluded.

## Plan
- [x] Inspect current ignore rules for `game_data`, ZIPs, and temp output.
- [x] Update ignore rules to allow mission metadata JSON baselines under `game_data/mission_files`.
- [x] Update the batch runner to copy passing metadata JSON baselines next to the input ZIPs by default.
- [x] Verify `git check-ignore` behavior for JSON and ZIP paths.

## Notes
- `.gitignore` now unignores `game_data/mission_files/*.json`; the existing `*.zip` and `game_data/**` rules still keep ZIP inputs ignored.
- `android/helpers/run_mission_zip_batch.ps1` now writes passing regression metadata copies to `-RegressionJsonDir`, defaulting to the ZIP input folder, unless `-NoRegressionJson` is passed.
- Seeded current baselines from `android/temp/mission_zip_batch/20260608_232715/metadata` into `game_data/mission_files/`.
- Verification:
  - Temporary `game_data/mission_files/_gitignore_probe.json` appeared as untracked.
  - `game_data/mission_files/plutonia.zip` remained ignored.
  - `git ls-files --others --exclude-standard -- game_data/mission_files` lists the three JSON baselines.
