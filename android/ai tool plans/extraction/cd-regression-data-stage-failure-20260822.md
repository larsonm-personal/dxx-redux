# CD regression data stage failure 2026-08-22

## Plan

- [x] Locate the failed regression report and enumerate failing CD cases
- [x] Classify common versus independent failure causes from detailed logs
- [x] Reproduce the smallest representative failure from the saved run evidence
- [x] Implement scoped fixes and regression coverage
- [x] Run focused CD regression verification and code quality checks
- [x] Record findings and completed work

## Findings

- The failed stage was `temp/regression_data_reports/run_20260822_134642`, with five failures in `temp/extract_regression_reports/run_20260822_134859`.
- Three failures were managed mission-file path mismatches already addressed by commit `8c1ad69d`, which made extraction verification use logical content paths.
- `d1 mac 2nd bin+cue` imported correctly on its retry but intro skipping left the game in the Multiplayer submenu, so the automation template now normalizes that menu before selecting New Game.
- `Descent II (USA) (Alt)` lost the SetupActivity process during lengthy, ADB-disrupted image staging. The async import command targeted a dynamic receiver and was silently dropped, while the runner repeatedly read stale idle introspection. CD and ISO paths now establish a fresh SetupActivity process immediately before sending the import command.

## Verification

- `android/tests/test_extract_regression_workflow.ps1`: pass.
- PowerShell parser checks for both modified scripts: pass.
- Extraction automation JSONC parse: pass.
- `git diff --check`: pass.
- Focused emulator verification was attempted after two cold restarts. The emulator repeatedly crashed, went offline, and later stalled ADB file staging for more than 20 minutes, so it did not reach the changed import or menu behavior reliably. Logs are under `temp/extract_regression_reports/focused_20260822_*`.
