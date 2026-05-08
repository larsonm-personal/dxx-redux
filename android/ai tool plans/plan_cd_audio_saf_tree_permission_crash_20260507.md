# CD Audio SAF Tree Permission Crash Plan 2026-05-07

## Goal
- Stop the launcher from crashing when previewing a SAF-backed CD source imported from a folder picker
- Persist the correct SAF permission for later CD BIN and CUE access
- Validate the Kotlin build after the fix

## Findings
- The crash is a Java `SecurityException` in `MusicPickerPage.kt` when preview playback calls `openFileDescriptor()` on a saved child document URI
- Directory import scans a picked tree into child document URIs, but later source registration only tries to persist the child URI
- For tree-derived document URIs, durable access may need to be taken on the owning tree URI instead of only the scanned child URI
- Disc source registration saves `cueContentUri` without persisting read access for it

## Plan
- [completed] Add a shared SAF helper that can persist and release either the direct URI or its owning tree URI
- [completed] Use the helper when registering CD sources and referenced audio files
- [completed] Catch preview SAF open failures so broken older sources fail cleanly instead of crashing
- [completed] Run a focused Kotlin validation

## Validation
- `android\\gradlew.bat :app:compileDebugKotlin`
- `android\\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.SafUriPermissionsTest`