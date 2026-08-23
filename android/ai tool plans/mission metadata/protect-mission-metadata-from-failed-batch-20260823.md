# Protect mission metadata from failed batch writes 2026-08-23

## Plan

- [x] Identify the process and failure path that replaced the four metadata files
- [x] Restore the complete metadata without discarding unrelated work
- [x] Prevent failed or mismatched batch runs from publishing failure JSON over checked-in metadata
- [x] Add focused regression coverage for publication safety
- [x] Run focused tests and scoped code quality
- [x] Confirm the four files are valid, complete, and naturally clean in git diff

## Findings

- `regenerate_all_mission_metadata.ps1` started the emulator batch at 2026-08-22 21:38 and it remained active until 2026-08-23 10:52, overlapping the extraction regressions.
- The mission batch did not send or require an automation run ID. It accepted unrelated extraction automation results, then reported missing mission metadata output.
- Two failure paths wrote a compact `failure_text` object to the checked-in regression path. Those paths clobbered `Vignettes.json`, `af-d2x.json`, `diehard.json`, and `nefarious.json`.
- The `.7z` preflight for `diehard` and `nefarious` also exposed strict-mode use of PowerShell Core-only platform variables. Platform detection now works under Windows PowerShell and PowerShell Core.
- Failed runs continue to write diagnostic JSON under the batch artifact directory, but only passed runs with validated metadata can publish checked-in JSON.

## Verification

- Restored tracked metadata: `af-d2x.json` 15 levels, `diehard.json` 19 levels, `nefarious.json` 19 levels, and `Vignettes.json` 27 levels. All four parse as JSON and report `status: ok`.
- All four restored files match `HEAD` and are absent from `git diff`.
- `test_mission_zip_batch_publication.ps1`: pass.
- `test_dep_platform.ps1`: pass, including Windows PowerShell strict mode.
- `test_test_helpers_process_wait.ps1`: pass.
- `test_mission_metadata_travel_times.ps1`: pass for 1,473 levels.
- Scoped code quality: pass.
- The full route corpus test remains independently blocked by existing `failure_text` stubs in `KCXF2RMv11.json` and `ulterior_v1.0.6b.json`; none of the four restored files contains a null route step.

## Failed stub follow-up

- [x] Trace `KCXF2RMv11.json` and `ulterior_v1.0.6b.json` to their source archives and batch failures
- [x] Determine whether complete tracked metadata exists in history or can be regenerated locally
- [x] Fix the underlying analyzer or archive handling defect if needed
- [x] Replace both stubs only with validated complete metadata
- [x] Run focused metadata and route-corpus tests
- [x] Record final results and remaining limitations

### Results

- Both committed stubs came from the same strict-mode `$IsWindows` preflight failure fixed above. Complete versions also existed immediately before commit `d2c0408e`.
- Focused Android metadata regeneration now imports both `.7z` archives correctly and correlates automation by run ID.
- `KCXF2RMv11.7z` passed Android metadata generation and published one mission with 8 levels.
- `ulterior_v1.0.6b.7z` imported successfully on Android but its `ulterior ultima thule` target hit the device-side analysis timeout. The protected publisher left its tracked stub unchanged.
- Focused host regeneration then analyzed `ulterior_v1.0.6b.7z` successfully in 6 seconds and published two missions with 28 total levels. This confirms valid mission data and isolates the remaining Android result to device-side performance or worker timeout behavior.
- Both checked-in files are normalized complete metadata with `status: ok`; neither contains `failure_text` or null route steps.
- Metadata normalization passes and 1,509 travel-time records pass validation.
- The route corpus now advances beyond the former null-stub crash. It reports 113 reviewed-baseline differences across the wider metadata corpus from earlier successful regenerations; updating that broad baseline is outside this focused stub repair.
