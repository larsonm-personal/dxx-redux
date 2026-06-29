# Report 20260628 151027 Follow-up Plan - 2026-06-28

## Goal
Investigate the two remaining failures in `temp/test_reports/report_20260628_151027.md` and apply contract-level robustness fixes where the failures show real weak points.

## Tasks
- [x] Re-read repo instructions and current report
- [x] Inspect failing logs and scripts for `test_trine2_d1_in_d2_custom_textures`
- [x] Inspect failing logs and scripts for `test_lan`
- [x] Implement targeted fixes without timeout-only changes
- [x] Run targeted verification for changed behavior
- [x] Run scoped code quality
- [x] Summarize outcome and residual risk

## Initial Triage
- Passing in this report: previous launcher reset, controls readability, D-pad triggers, mission zip batch, mod loading, pause menu, long-hold delete, and quick-record sidecar tests
- Failing: `test_trine2_d1_in_d2_custom_textures` with a sparse launcher-script log
- Failing: `test_lan` with no inline report log excerpt

## Findings
- `test_trine2_d1_in_d2_custom_textures` passed when rerun after a healthy primary emulator was available, so no contract change was made there
- `test_lan` reached `=== LAN MP TEST PASSED ===`, but the redirected wrapper did not return after auto-starting `Nexus5X_Light_2`
- Patched `Start-ManagedEmulatorProcess` so Windows test runs start long-lived emulators through hidden ShellExecute instead of PowerShell redirected stdio

## Verification
- `test_trine2_d1_in_d2_custom_textures.json5`: reran after primary emulator recovery and passed file-based automation result
- `test_lan.ps1 -SkipBuild -TimeoutSeconds 120`: forced `emulator-5556` offline first, then verified auto-start, boot, LAN sync, and `EXIT: 0`
- `android/run-code-quality.ps1 -Fix -Paths android/helpers/test_helpers.ps1 ...`: passed

## Residual Risk
- Windows emulator stdout/stderr are no longer captured by `Start-ManagedEmulatorProcess`; the tradeoff is intentional because inherited stdio handles were making redirected test wrappers hang after success
