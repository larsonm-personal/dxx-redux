# Plan: Input Demo Actual Result Sidecar Cleanup (2026-05-04)

## Goal
- Stop input demo test runners from leaving `.dximdemo.actual.json` files next to committed demo fixtures
- If the replay helpers still need an on-disk actual-result file for compare logic, redirect it into a temp directory instead of writing beside the source demo

## Local Hypothesis
- The sidecars are produced by a shared replay-helper default that derives the actual-result path from the input demo path when callers do not override it
- The cheapest check is to find that shared path-selection seam, patch it to use a temp sandbox path, then run a focused replay test and confirm no new `.actual.json` file appears next to the demo input

## Execution Plan
- Survey the replay helpers and test runners to find the controlling actual-result path logic and every caller that still relies on the default
- Patch the controlling seam so replay tests stop emitting sidecars beside demo fixtures, using a temp output path only when an on-disk actual-result file is still required
- Run a focused validation pass on the touched replay test helper and confirm no adjacent `.dximdemo.actual.json` files are created

## Status (2026-05-04)
- Phase 1 complete
  - traced the shared actual-result path selection in native replay startup plus the PowerShell replay wrapper
- Phase 2 complete
  - added an explicit `-inputdemo-actual-result` override so test runners can redirect actual-result files into temp sandboxes instead of beside source demos
- Phase 3 complete
  - validated `android/tests/run_input_demo_replay.ps1` with a focused replay run and confirmed no `.actual.json` files are created next to demos under `android/temp_game_logs` or `android/regression_demos`
- Follow-up note
  - `android/tests/test_input_demo_runtime_smoke.ps1 -Game d2` still has an unrelated failing assertion about missing `frame_state` records, but its actual-result path redirection worked