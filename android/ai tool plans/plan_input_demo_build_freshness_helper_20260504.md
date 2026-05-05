# Plan: Input Demo Build Freshness Helper (2026-05-04)

## Goal
- survey input-demo-related test entry points that launch host binaries or repeatedly invoke replay wrappers
- extract the host build freshness check and auto-rebuild path into one shared PowerShell helper
- wire affected callers so batch scripts do one explicit preflight instead of relying on buried per-run checks

## Scope
- PowerShell under `android/tests/`
- keep the freshness roots simple and shared: game dir, `common/`, `arch/`, and `android/app/src/main/cpp/shared/`
- no gameplay source changes

## Execution Plan
- Phase 1 complete
  - surveyed `android/tests/*.ps1` for direct `buildd1/buildd2` executable usage and replay-wrapper callers
  - identified direct host-binary callers: `run_input_demo_replay.ps1`, `test_input_demo_runtime_smoke.ps1`
  - identified batch callers that benefit from a single explicit preflight: `run_input_demo_regressions.ps1`, `run_input_demo_headless.ps1`, `test_input_demo_determinism_matrix.ps1`
- Phase 2 complete
  - added a shared host build guard helper under `android/tests/`
  - moved the existing replay freshness logic onto that helper
- Phase 3 complete
  - added caller-side preflight hooks to the affected batch scripts
- Phase 4 complete
  - validated focused D2 replay and headless wrapper runs through the shared helper path
  - verified the edited PowerShell files are parser-clean and completed the PowerShell quality pass
  - noted an existing `test_input_demo_runtime_smoke.ps1` failure where the replay state trace lacked `frame_state` records after the run completed