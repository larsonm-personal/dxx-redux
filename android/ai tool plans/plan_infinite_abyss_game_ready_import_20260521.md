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
- [x] Review the PC-side Infinite Abyss extractor/test flow and compare it to Android import behavior
- [x] Realign Android disc import to the desktop oracle: full ISO mirror, in-place .sow extraction, then root staging for readiness
- [x] Run scoped code quality on touched files
- [x] Revalidate Kotlin compile, targeted unit tests, and native builds/tests

## Findings

- The active set is still `default`; the log shows the launcher scanning the correct set path after the clear-all-data feature
- The broken set contains `d2data/` and `winsetup/` directories, but readiness only checks the active set root for required files
- Existing extraction test tooling already handles CDs that organize data under `d1data/` or `d2data/` by recursively collecting and flattening game files
- The app import path now hoists nested game files into the set root, and it no longer relies on `renameTo()` succeeding on the imported storage volume
- The newer log showed the import session still logging only CD-audio registration, while restart revealed `d2data/` and `winsetup/` leftovers with only movies surfaced at root
- The proven PC-side Infinite Abyss path is not installer-driven: `extract_cd` mirrors the full ISO tree, expands `.sow` in place, and leaves `d2data/` nested until a later recursive flatten step
- The earlier file-based STi2 detour was speculative for Infinite Abyss and has been removed from the Android path
- Android now mirrors the desktop extractor behavior for ISO and `.sow`, then stages nested game files into the set root so launcher readiness still works
- Clear-all previously deleted manifests/files but left CD audio source metadata and unused persisted SAF grants behind
- Immediate setup refresh was already wired at dialog close, but import actions now also trigger refresh at the moment extraction or audio registration succeeds

## Validation so far

- Corrected log review: `android\temp_game_logs\gamelogs inf abyss import after hoisting change but still not ready.txt`
- New failing log review: `android\temp_game_logs\gamelogs with inf abyss try 3 still broken.txt`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.DiscImportHoistTest"`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.SafUriPermissionsTest"`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.SafUriPermissionsTest" --tests "com.dxxredux.app.DiscImportHoistTest" :app:compileDebugKotlin`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.DiscImportHoistTest" --tests "com.dxxredux.app.SafUriPermissionsTest" :app:buildCMakeDebug[arm64-v8a]-2`
- `c:\local\android-sdk\cmake\3.31.6\bin\cmake.exe -S android/app/src/main/cpp/extract -B build_test -G "Visual Studio 17 2022"`
- `c:\local\android-sdk\cmake\3.31.6\bin\cmake.exe --build build_test --target test_sti2 --config Debug`
- `c:\local\android-sdk\cmake\3.31.6\bin\ctest.exe --test-dir build_test -C Debug -R sti2_tests --output-on-failure`
- `build_test\Debug\extract_cd.exe "game_data\CD images\Descent II Infinite Abyss\DESCENT_II_ABYSS.cue" temp\infinite_abyss_pc_extract_review`
- Compared `temp\infinite_abyss_pc_extract_review` against `game_data\CD images\Descent II Infinite Abyss\data_tracks`: identical 476-file relative tree; required assets remained under `d2data\`
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:testDebugUnitTest --tests "com.dxxredux.app.DiscImportHoistTest" :app:compileDebugKotlin :app:buildCMakeDebug[arm64-v8a]-2`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SafUriPermissions.kt','android/app/src/main/java/com/dxxredux/app/FileSetManager.kt','android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt','android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/test/java/com/dxxredux/app/SafUriPermissionsTest.kt')`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/extract/game_file_extensions.c','android/app/src/main/cpp/extract/jni_disc_import.c','android/app/src/main/java/com/dxxredux/app/DiscImportBridge.kt','android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/test/java/com/dxxredux/app/DiscImportHoistTest.kt')`