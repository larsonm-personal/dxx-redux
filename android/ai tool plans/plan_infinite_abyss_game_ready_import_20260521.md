# Infinite Abyss game-ready import fix

## Status

- [x] Inspect broken-state launcher log and active set state
- [x] Trace readiness scan and disc/ISO extraction paths
- [x] Add post-extract hoist for nested CD data directories
- [x] Add a focused regression test for nested disc data hoisting
- [x] Fix clear-all so CD audio sources and unused SAF grants are removed
- [x] Refresh setup state immediately after disc/ISO extract and audio registration
- [x] Harden nested-file hoisting with a copy fallback when rename fails
- [x] Add focused SAF permission retention tests
- [x] Run scoped code quality on touched files
- [x] Revalidate Kotlin compile and targeted unit tests

## Findings

- The active set is still `default`; the log shows the launcher scanning the correct set path after the clear-all-data feature
- The broken set contains `d2data/` and `winsetup/` directories, but readiness only checks the active set root for required files
- Existing extraction test tooling already handles CDs that organize data under `d1data/` or `d2data/` by recursively collecting and flattening game files
- The app import path now hoists nested game files into the set root, and it no longer relies on `renameTo()` succeeding on the imported storage volume
- Clear-all previously deleted manifests/files but left CD audio source metadata and unused persisted SAF grants behind
- Immediate setup refresh was already wired at dialog close, but import actions now also trigger refresh at the moment extraction or audio registration succeeds

## Validation so far

- Corrected log review: `android\temp_game_logs\gamelogs inf abyss import after hoisting change but still not ready.txt`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.DiscImportHoistTest"`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.SafUriPermissionsTest"`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.SafUriPermissionsTest" --tests "com.dxxredux.app.DiscImportHoistTest" :app:compileDebugKotlin`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SafUriPermissions.kt','android/app/src/main/java/com/dxxredux/app/FileSetManager.kt','android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt','android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/test/java/com/dxxredux/app/SafUriPermissionsTest.kt')`