# Cleanup report quibbles from full-green audit

## Goal
- Make green full-suite reports preserve evidence for passing tests that had internal skips, warnings, or sidecar-only logs

## Plan
- [completed] Patch `run_all_tests.ps1` so PASS rows can include audit notes and empty primary logs can append known sidecar logs
- [completed] Patch `test_all_extracts.ps1` so child-level skip results are summarized as skips instead of passes
- [completed] Run parser checks and scoped code quality on the touched scripts
- [completed] Update this plan with final status and verification notes

## Verification
- `pwsh` parser check passed for `android/run_all_tests.ps1` and `android/tests/test_all_extracts.ps1`
- `android/run_all_tests.ps1 -Filter "__NO_MATCH__"` smoke report completed after the edits
- Historical-log probe found notes for the audited extraction skip, mission ZIP skip summary, double-launch warning, and multiplayer sidecar timeout
- Scoped `android/run-code-quality.ps1 -Fix -Paths ...` passed over both scripts and this plan file

## Not Run
- Full emulator extraction or full suite rerun
