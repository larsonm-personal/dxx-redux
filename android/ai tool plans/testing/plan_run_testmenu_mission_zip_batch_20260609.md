# Run-TestMenu mission ZIP batch

## Goal
Make selecting `test_mission_zip_batch_import_metadata_launch` in `Run-TestMenu.ps1` run the full mission ZIP batch wrapper over the mission ZIP directory instead of launching the unresolved support template directly.

## Plan
- [x] Inspect menu dispatch and current support-template handling.
- [x] Add a menu dispatch path for the mission ZIP batch wrapper.
- [x] Keep normal menu build/install behavior for the wrapper run.
- [x] Validate script parsing and scoped code quality.

## Notes
- `Run-TestMenu.ps1` now routes `test_mission_zip_batch_import_metadata_launch` to `helpers/run_mission_zip_batch.ps1`.
- The menu still performs its normal APK build first; when it builds, it passes `-Install` to the batch wrapper.
- Validated PowerShell parsing for the menu and helper scripts, then ran scoped `android/run-code-quality.ps1 -Fix`.
