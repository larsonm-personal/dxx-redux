# Mission metadata regeneration failures 2026-08-01

## Goal

Diagnose and harden the mission metadata regression runner failures for `af-d2x.zip` and `ulterior_v1.0.6b.7z` without weakening readiness or active-base-data validation.

## Plan

- [x] Inspect the failed batch summary, per-mission logs, checkpoints, device state, and processing order
- [x] Trace SetupActivity readiness and active-base-data reset checks to their producers and existing tests
- [x] Fix the root cause while retaining fail-closed validation and deterministic retry behavior
- [x] Add or extend runner-level regression coverage for the reproduced transition
- [x] Run focused tests, scoped code quality, and proportionate integration verification

## Findings

- The original `ulterior_v1.0.6b.7z` preflight failure was transient, but a focused retry exposed the archive's previously documented 192 MiB LZMA2 dictionary allocation crashing the launcher at its 192 MiB heap growth limit.
- The launcher now requests Android's large heap class for legitimate high-dictionary 7z imports, and launcher automation detects a dead launcher process instead of waiting for the full automation timeout.

## Validation

- Scoped code quality: passed for all changed Kotlin, PowerShell, XML, and plan files.
- `android/tests/test_mission_zip_batch_recovery.ps1`: passed.
- `ModManagerMissionZipTest.missionSevenZipImportsAndStagesAtMissions`: passed.
- `:app:assembleDebug`: passed, including native builds for all configured Android ABIs.
- Focused metadata-only emulator run for `af-d2x.zip`: passed in 23 seconds.
- Focused metadata-only emulator run for `ulterior_v1.0.6b.7z`: passed in 2 minutes 40 seconds with the original 7z archive.
