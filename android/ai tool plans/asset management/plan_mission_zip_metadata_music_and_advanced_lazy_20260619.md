# Mission ZIP metadata music and advanced lazy loading

## Goal
- Fix extracted mission archive music preview and metadata labeling, and avoid expensive advanced-tab file scans until file/link explorers are opened

## Plan
- [x] Make extracted mission music tracks carry a direct source path for preview staging
- [ ] Show chromaprint/AcoustID names directly in the mission ZIP music metadata list
- [ ] Defer advanced storage/link scanning until explorer screens are selected
- [ ] Run focused tests, assemble, and scoped code quality

## Notes
- Verification is initially blocked because the local PowerShell tool is failing to start with exit code `-1073741502`
- Extracted music tracks now carry `sourceFilePath`, and preview staging prefers that direct file path for extracted bundles
- Remaining UI-label and advanced-tab lazy-loading patches need live inspection of `SetupSections.kt` and `AdvancedSettingsPage.kt`
