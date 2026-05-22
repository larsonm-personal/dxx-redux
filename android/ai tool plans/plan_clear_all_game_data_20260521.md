# Clear all game data option

## Status

- [x] Trace current Advanced danger-zone actions and set/mod storage ownership
- [x] Add a file-set clear helper that preserves pilot files and saved games
- [x] Add a mod clear helper that removes mod archives and mounted-mod artifacts
- [x] Add an Advanced danger-zone action for clearing all game data
- [x] Run scoped code quality on touched launcher files
- [x] Revalidate Kotlin compile after formatter pass

## Notes

- Player data lives inside set directories, so deleting sets outright would also delete pilots and saves
- The clear-all action therefore removes non-pilot data from every set, deletes non-default sets only when they become empty, and leaves controls untouched in `filesDir`
- Mod cleanup must also remove `.active_mod_paths` and generated patch override directories so the next launch sees no stale mounted mods

## Validation so far

- `cd android; .\gradlew.bat :app:compileDebugKotlin`
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/FileSetManager.kt','android/app/src/main/java/com/dxxredux/app/ModManager.kt')`
- `cd android; .\gradlew.bat :app:compileDebugKotlin`