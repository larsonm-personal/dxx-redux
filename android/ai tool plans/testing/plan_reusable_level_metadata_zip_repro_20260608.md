# Reusable level metadata ZIP repro

## Goal
Create a reusable launcher test that provisions base D1/D2 files plus a parameterized level ZIP, opens level metadata, reproduces the Plutonia crash, and fix the underlying cause.

## Plan
- [x] Re-read project instructions.
- [x] Inspect existing launcher automation and test runners.
- [x] Add a reusable parameterized metadata test script or helper.
- [x] Run it against `game_data/plutonia.zip` to capture the crash details.
- [x] Fix the native or launcher metadata path.
- [x] Re-run the reusable test and focused verification.

## Findings
- The reusable test reproduced a native metadata failure for Plutonia: `could not load mission plutonia`.
- Plutonia staged `plutonia.mn2` at the ZIP staging root, but D2X mission discovery only scans `missions/`.
- The native descriptor check also treated a root descriptor as loadable, which caused `load_mission_by_name` to fail cleanly.
- After staging descriptors under `missions/`, the test reproduced the original native crash at `map03.rl2`.
- The crash was a debug-only `check_segment_connections()` warning trying to open a `nm_messagebox` in the headless metadata worker.

## Verification
- Reusable test failed before the final fix with `Last stage: level map03.rl2` and a native tombstone in `nm_messagebox`.
- Ran `:app:assembleDebug --no-daemon` successfully after fixes.
- Ran `android\helpers\run_test.ps1 -ScriptName test_level_metadata_launcher_zip_reusable.json5 -Game d2 -Install -Params @{LEVEL_ZIP='plutonia'} -TimeoutSeconds 900`; passed.
- Confirmed `level_metadata_automation_plutonia.json` reports status `ok` and `level_count` 32.
- Ran `:app:testDebugUnitTest --tests com.dxxredux.app.LevelMetadataTargetsTest --no-daemon`; passed.
