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
