# Metl154 tranche 5: cache and efficiency pass

## Scope

This tranche implements the first low-risk part of Part C.4 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- reduce redundant Android shader program binds in the GLES3 shim and the
  merged-wall shader helpers
- keep the merged-wall route split unchanged, especially the validated
  `auto_old_texmerge` fallback for the stock 64x64 metl154 target face

Out of scope for this tranche:

- changing cached-premerge route selection
- changing merged-wall snapshot ranking or regression assertions
- broader cache-policy work beyond the shader bind hot path

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [x] Add a small Android-only program-bind cache inside `gles3_shim`
- [x] Route Android direct shader binds in D1 and D2 OGL shader helpers through that cache
- [x] Re-run build, unit tests, and the strengthened merged-wall regression
- [x] Update this file with findings and validation

## Findings

- `gles3_shim` now tracks the current GL program and skips redundant
  `glUseProgram` calls when the requested program is already bound.
- The Android direct shader binds in D1 and D2 `oglprog.c` now route
  through the same helper, so the cache stays accurate even when the
  merged-wall helper code binds its own shader programs outside the shim's
  normal fixed-function path.
- This change does not alter merged-wall route selection. The intended
  runtime invariant stays the same: the strengthened metl154 regression
  still expects the target face on `route=old_texmerge` with
  `merge_impl=auto_old_texmerge` for the stock 64x64 case.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `android\run_test.ps1 -ScriptName game_scripts\test_merged_wall_snapshot_regression.json5 -Game d2 -Install`: passed

## Status

- [x] In progress
- [x] Validation complete
