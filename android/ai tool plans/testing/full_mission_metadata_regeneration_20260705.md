# Full Mission Metadata Regeneration Plan

## Goal
Regenerate all mission metadata JSON files from the current analyzer so recent pathing and crash-context changes are reflected in the checked-in metadata.

## Tasks
- [x] Build the current debug APK with JDK 21 so native scanner and launcher changes are included.
- [x] Run the mission ZIP batch metadata-only flow for all archives in `game_data/mission_files`, including large archives.
- [x] Review pass/fail summary and identify any archives that did not regenerate.
- [x] Inspect resulting JSON diff at a high level.
- [x] Run scoped JSON/code-quality checks if generated files changed.
- [x] Update this plan with output paths and validation.

## Notes
- Output directory: `android\temp\mission_zip_batch\full_regen_20260705_000021`.
- The worktree already contains uncommitted scanner, crash-report, and KCXF2 metadata changes from the previous tasks; this regeneration should build on them, not revert them.
- First direct full run hit an Android Java heap OOM while importing `ulterior_v1.0.6b.7z`; Commons Compress requested a 192 MB LZMA2 buffer.
- Workaround used for regeneration: extracted `ulterior_v1.0.6b.7z` with local 7-Zip and repacked the same contents as `temp\mission_metadata_regen_archives\full_20260705_000021\ulterior_v1.0.6b.zip`, preserving the base name so `game_data\mission_files\ulterior_v1.0.6b.json` is generated.
- Final batch output directory: `android\temp\mission_zip_batch\full_regen_20260705_000021_repacked`.
- Final batch summary: 109 total, 108 passed, 1 skipped unknown, 0 failed. The skipped archive was `ewithin-versions.zip`, which has no D1/D2 mission descriptor or level hints.
- Metadata diff after regeneration: 48 JSON files changed, including new `game_data\mission_files\ulterior_v1.0.6b.json`.

## Validation
- `$env:JAVA_HOME='C:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"; .\gradlew.bat assembleDebug --no-daemon`
- `.\android\helpers\run_mission_zip_batch.ps1 -ZipDir temp\mission_metadata_regen_archives\full_20260705_000021 -RegressionJsonDir game_data\mission_files -MetadataOnly -Install -IncludeLarge -OutDir android\temp\mission_zip_batch\full_regen_20260705_000021_repacked -TimeoutSeconds 300 -MaxEmulatorRecoveries 8`
- Changed metadata JSON files parsed successfully with `ConvertFrom-Json`.
- `.\android\run-code-quality.ps1 -Fix -Paths @('game_data\mission_files','android\ai tool plans\testing\full_mission_metadata_regeneration_20260705.md')`
