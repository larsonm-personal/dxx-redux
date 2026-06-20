# Mission archive import progress

## Goal
- Show progress while importing mission ZIP/7z/RAR archives after files are picked

## Plan
- [x] Locate mission archive import state and status UI
- [x] Add progress state for mission archive copy/inspect/extract work
- [x] Render progress bar where import status text appears
- [x] Run scoped tests/code quality and update results

## Results
- Added mission archive import progress state in the setup screen
- Displayed the progress indicator in the same status area used by the final import result message
- Used determinate progress while copying the picked archive, then indeterminate progress while inspecting, extracting, and finalizing
- Kept the import button disabled while mission archives are still being processed
- Passed scoped code quality, `:app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest`, and `:app:assembleDebug`

