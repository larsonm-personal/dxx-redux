# SAF link removal and CD filter 2026-05-07

## Goal
- Add a remove action in the Advanced SAF viewer so broken SAF-backed entries can be cleaned up from launcher state
- Exclude broken SAF-backed CD audio sources from the CD audio page so unusable entries do not appear there
- Validate the Kotlin build after the change

## Findings
- The Advanced SAF viewer already probes each URI and labels it `OK` or `BROKEN`, but it only stores display text and the URI string, so it cannot map a row back to the owning launcher record for removal
- CD audio sources are loaded directly from `AudioSourceManager.getSources()` in `MusicPickerPage.kt`, so broken SAF-backed sources remain visible even when the Advanced page already detects them as inaccessible

## Plan
- [completed] Add owner metadata to SAF viewer rows and route removal through the correct manager operation or permission release path
- [completed] Add a focused helper for removing referenced custom-audio files from a set and use it from the SAF viewer
- [completed] Filter inaccessible SAF-backed CD sources out of the music page state refresh path
- [completed] Run focused Kotlin validation and record the result

## Result
- The Advanced SAF viewer now lets the user select an entry and remove either:
	- a referenced custom-audio file from its launcher set
	- a SAF-backed CD audio source from the launcher
	- an orphaned persisted SAF permission
- The CD audio page now filters out SAF-backed sources whose BIN or CUE URI no longer opens, and the CD track preview dialog uses that same filtered list
- Added a shared `canAccessSafUri()` probe plus focused pure-helper tests for custom-audio referenced-file removal and CD source visibility

## Validation
- `android\gradlew.bat :app:compileDebugKotlin`
- `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest --tests com.dxxredux.app.SafUriPermissionsTest --tests com.dxxredux.app.CustomAudioSetManagerTest --tests com.dxxredux.app.CdAudioSourceVisibilityTest`
- reran the same focused Gradle command after the code-quality pass to confirm the current file state still passes

## Notes
- `android\run-code-quality.ps1 -Fix -Paths ...` again treated the provided scoped paths as missing and fell back to a broad pass; it reported the same pre-existing non-autofixable max-line-length issues in `SetupActivity.kt`