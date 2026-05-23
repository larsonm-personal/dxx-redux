# General scroll repeat 2026-05-07

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

## Scope

- Fix the controller axis-picker repeat bug where holding down to the footer keeps pulling focus back to `OK`
- Move the vertical repeat handler into shared launcher D-pad utilities
- Apply the shared repeat handler to the main launcher settings pages and the music track-list dialog
- Validate with a narrow Kotlin compile and then finish the tranche bookkeeping

## Steps

- [x] Confirm the stuck-repeat root cause is the dialog-body-only key handler losing key-up once focus moves to the footer button
- [x] Add shared vertical D-pad repeat support in `DpadFocusUtils.kt` and switch callers to it
- [x] Apply the shared repeat modifier to the music page, track preview list, and launcher settings pages with large vertical scroll content
- [x] Run focused validation and then update the plan with final status

## Result

- Added shared vertical repeat navigation in `android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt`
- Moved controller picker repeat handling from the dialog body boxes to the full `AlertDialog` modifiers so footer buttons stay in the same key-up/key-down stream
- Applied the shared repeat modifier to:
	- `ControllerConfigPage.kt` picker dialogs
	- `MusicPickerPage.kt` main page container
	- `MusicPickerPage.kt` track preview dialog
	- `AdvancedSettingsPage.kt`
	- `GraphicsSettingsPage.kt`
	- `EnginePreferencesPage.kt`
- Kept the music slider helper name as a thin wrapper over the shared modifier so the remaining slider call sites use the centralized repeat behavior without another local implementation

## Validation

- `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt','android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt','android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt','android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt')`
- `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest` rerun after formatting
- `run-windows-build.ps1 -Target both -Compiler vs2022-community`

## Notes

- The Windows host build completed with the same pre-existing native warnings seen in prior runs, including `loadgl.h` macro redefinitions and existing C4113/C4715 warnings in D2 sources