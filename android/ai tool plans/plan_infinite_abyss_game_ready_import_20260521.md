# Infinite Abyss game-ready import fix

## Status

- [x] Inspect broken-state launcher log and active set state
- [x] Trace readiness scan and disc/ISO extraction paths
- [x] Add post-extract hoist for nested CD data directories
- [x] Add a focused regression test for nested disc data hoisting
- [x] Run scoped code quality on touched files
- [x] Revalidate Kotlin compile and targeted unit test

## Findings

- The active set is still `default`; the log shows the launcher scanning the correct set path after the clear-all-data feature
- The broken set contains `d2data/` and `winsetup/` directories, but readiness only checks the active set root for required files
- Existing extraction test tooling already handles CDs that organize data under `d1data/` or `d2data/` by recursively collecting and flattening game files
- The app import path currently extracts into the set directory but does not hoist nested game files to the set root, so the launcher and engine do not see them as ready

## Validation so far

- Broken-state log review: `android\temp_game_logs\gamelogs with inf abyss files but game not ready.txt`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.DiscImportHoistTest"`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/test/java/com/dxxredux/app/DiscImportHoistTest.kt')`
- `cd android; .\gradlew.bat :app:compileDebugKotlin`