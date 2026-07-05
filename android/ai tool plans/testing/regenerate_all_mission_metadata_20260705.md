# Regenerate All Mission Metadata Plan

## Goal
Regenerate the mission ZIP metadata JSON files in `game_data/mission_files` through the launcher automation path, using the normalized JSON writer added in the previous pass.

## Tasks
- [x] Confirm the batch generator inputs and output path.
- [x] Build the current debug APK so metadata generation uses the latest scanner and launcher code.
- [x] Add a metadata-only batch template so full regeneration does not also smoke-launch every mod.
- [x] Run the mission ZIP metadata batch over the importable mission archives.
- [x] Monitor completion and summarize passed, skipped, and failed archives.
- [x] Validate regenerated JSON formatting.
- [x] Update this plan with results and any follow-up failures.

## Notes
- The batch writes regression metadata next to each source archive unless `-NoRegressionJson` is used. For this regeneration, leave regression JSON enabled.
- The batch helper normalizes JSON through `android/helpers/normalize_json.py` as each file is saved.
- Built the current debug APK before regeneration with `android/gradlew.bat assembleDebug`.
- Added `android/game_scripts/test_mission_zip_batch_import_metadata.json5` and `-MetadataOnly` support in `android/helpers/run_mission_zip_batch.ps1`.
- Regeneration output: `android/temp/mission_zip_batch/regen_all_metadata_only_20260704_201854`.
- Batch result for the regenerated set: 107 total, 107 passed, 0 skipped, 0 failed.
- Validation passed for all 107 regenerated regression JSON files: `normalize_json.py --check`, JSON parse, no tab characters, scoped code-quality pass, and `git diff --check`.
- Two archives under `game_data/mission_files` did not produce metadata in this pass:
  - `ewithin-versions.zip` was classified as unknown because the ZIP does not contain D1/D2 mission descriptors or level hints.
  - `ulterior_v1.0.6b.7z` was retried by itself with the metadata-only template and timed out after 180 seconds inside launcher `import_mission_zip`, before metadata analysis could start. Retry output: `android/temp/mission_zip_batch/ulterior_metadata_retry_20260704_204449`.
