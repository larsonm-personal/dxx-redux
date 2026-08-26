# Save load immediate feedback

## Goal

Remove any avoidable launcher delay after selecting a saved game and show immediate loading feedback until the game activity takes over.

## Plan

- [x] Trace the saved-game tap, launch handoff, and route metadata precompute shutdown path
- [x] Remove artificial or unnecessarily blocking delay while preserving safe engine startup
- [x] Show a launcher loading popup immediately on a valid saved-game selection
- [x] Add or extend focused regression coverage
- [x] Run scoped formatting, unit tests, and an Android build; record results here

## Notes

- Preserve unrelated working-tree changes, especially the existing edits in `MainActivity.kt` and its tests.
- The prior 1 to 2 second coordinator join had already been removed. The remaining explicit metadata shutdown grace was 100 ms and is now zero.
- The launcher dialog had been restricted to the metadata phase and cleared before `startActivity`; it now covers preparation, metadata shutdown, and activity startup.

## Verification

- Scoped code quality passed for the changed Kotlin and plan files.
- Focused `ResumeSavePanelTest`, `RouteMetadataSchedulingTest`, and `RouteMetadataPrecomputeMonitorTest` passed.
- Full `:app:testDebugUnitTest` passed.
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- The D2 autosave-resume emulator flow was attempted twice. Both runs were invalidated by a stale concurrent `mission_zip_batch_current.jsonc` automation run that reset launcher state; this was unrelated to the changed code, and the JVM/build verification remained clean.
