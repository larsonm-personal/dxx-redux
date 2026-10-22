# Persistent store assets and regeneration

Completed on 2026-10-21

- Default outputs now use gitignored android/store-assets/capture_<timestamp>
- Both default timestamp cleanup and the broader workspace cleaner preserve the publication tree, including logs and nested scratch sources. Generator retries do not prune previous captures
- Dedicated emulator registration is repository-local and can recover its descriptor after profile cleanup. Signature replacement is restricted to the already-verified dedicated capture AVD
- Added -ScreenshotsOnly and offline validate-stills integration; full-video mode rejects absent or unsupported replay sources before building or touching the emulator
- Built the current debug x86_64 APK and generated eight native screenshots: two populated launcher views, five authored gameplay views, and D1 fly-out. Gameplay staging uses native level warp, invulnerability and robot hold during camera setup; no historical replay equivalence is claimed
- Camera framing uses native introspection geometry and an open adjoining room where a spawn segment is too small. Rejected close-ups/death views were replaced. The final level-18 choice retains an earlier same-APK classic-cockpit view with visible lasers

Final gallery: android/store-assets/capture_20261021_084430/index.html
Selected PNGs: android/store-assets/capture_20261021_084430/selected-stills/
Evidence: live-stills.json, states/, stills-validation.json and demo-triage-summary.json in that folder

## Demo triage

All four historical recipe sources are absent from android/regression_demos. Original backups were found in android/regression_demos_backup/format4_20261014_0718; all are format 4 and are unsupported by the current format-5 engine. No versions, input order, fingerprints or expected results were edited

Two newer format-5 D2 recordings in C:/local/dxx-redux-test-assets/demos, levels 7 and 9, pass full recorded-result comparison and clean exit using buildd2/main/dxx-redux-d2-headless.exe. The initial probe mistakenly supplied graphical d2x-redux.exe to the headless runner; its forced stops are invocation errors, not engine/demo hang evidence. Both complete state results also matched in that initial graphical run. Corrected native run: PASS, two demos, 50.876 seconds

## Verification

- Android assembleDebug x86_64: PASS (4m 47s; existing native warnings, no native edits)
- test_clean_old_artifacts.ps1: PASS, including persistent capture fixtures and stable-root generation retention
- test_clean_workspace.ps1: PASS, including ignored PNG/log/nested-temp publication evidence. First run conservatively preserved an emulator fixture changed since scan; retry passed
- test_store_asset_pipeline.ps1 -ScreenshotsOnly with final output: PASS; eight full-resolution PNGs, hashes, save thumbnail, native graphics and living-player level state
- Scoped mixed-language code quality: PASS
- Full-video source preflight: expected rejection naming all four missing recordings

Existing unrelated code-management plan edits were left untouched. No commits, uploads, store publication or physical-device writes
