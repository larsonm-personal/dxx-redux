# Level metadata restore flash

## Goal

Stop the level metadata view from repeatedly opening and closing when the app returns from a minimized state, while preserving the intended restored-open state.

## Plan

- [x] Trace the activity and metadata-view lifecycle, including any earlier attempted fix and the three repeated visibility updates
- [x] Add a focused regression test or lifecycle test that reproduces the restore sequence
- [x] Make the smallest source change that removes redundant visibility transitions
- [x] Run scoped formatting, the focused test, and the relevant Android build/test verification

## Findings

- `SetupActivity.onResume()` performs one immediate refresh and schedules three delayed refreshes at 250 ms, 1 s, and 2.5 s
- The earlier metadata resume fix removed `refreshTrigger` from `LevelMetadataDialog`, preventing repeated analysis, but `FileDetailDialog` still gated the open dialog on a freshly recomputed nullable target
- Each delayed refresh cleared that derived target while file metadata reloaded, removing and recreating the open dialog exactly three times
- The open dialog now owns the target selected by the user, so background file-detail refreshes cannot change its visibility or restart its analysis

## Validation

- Scoped `run-code-quality.ps1 -Fix` passed for `SetupSections.kt` and this plan
- `LevelMetadataAnalysisSingleFlightTest` and `LevelPreviewReturnRefreshGateTest` passed, four tests total
- `:app:assembleDebug` passed with all Android native variants and Kotlin compilation
- Installed the debug APK on the emulator, opened `descent2.hog` file details and its loaded metadata view, minimized, then restored the launcher
- Sampled the window stack for 5.3 seconds across all three delayed refreshes; all 40 samples retained the launcher plus both dialogs, and the metadata level list remained present afterward
