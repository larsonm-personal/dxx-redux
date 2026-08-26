# Regenerate Missing Mission Metadata Menu Plan

## Goal

Add a regression-data menu action that generates mission metadata only for
supported mission archives whose expected JSON regression file is missing.
The selection must use the shared archive-source configuration so special
sources such as `game_data/mission_files/d2xxl_downloads` work consistently.

## Plan

1. [Complete] Document the existing archive-to-regression-output mapping and menu flow.
2. [Complete] Add a reusable missing-archive selector for all configured archive sources.
3. [Complete] Add a top-level menu/category action and propagate missing-only mode to the
   mission metadata regeneration script.
4. [Complete] Add regression tests covering mixed `.zip` and `.7z` archives, including a
   special archive source, plus menu dispatch behavior.
5. [Complete] Run focused tests and scoped code-quality checks, then record the results.

## Verification

- `android/tests/test_mission_metadata_archive_sources.ps1`: passed.
- `android/tests/test_regenerate_all_regression_data.ps1`: passed.
- `android/run-code-quality.ps1 -Fix -Paths ...`: passed for the five changed
  PowerShell files.
- `git diff --check`: passed for all task files.
- Current repository inventory: 110 primary mission archives with 1 missing
  JSON, and 18 `d2xxl_downloads` archives with 15 missing JSON files.
