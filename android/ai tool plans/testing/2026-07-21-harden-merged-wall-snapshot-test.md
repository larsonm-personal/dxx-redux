# Harden merged wall snapshot test

## Plan

- [x] Inspect the complete failure log and preserved automation artifacts
- [x] Trace the test setup, runner cleanup, and merged-wall capture behavior
- [x] Implement the smallest root-cause hardening without extending timeouts
- [x] Run scoped code quality and a stateful predecessor-to-target validation sequence
- [x] Run relevant build or broader test verification and record results

## Findings

- The reported run never reached a merged-wall assertion; it stopped at automation step 11 after two emulator health probes failed
- Diagnostics then saw `emulator-5554` online and captured new game-process logging, so the terminal `Emulator crashed` classification was premature
- The watcher retried a health probe but did not reset accumulated ADB transport state before failing the test
- The hardening retains the existing test deadline and graphics assertions, retries once, resets ADB once, and fails if the post-reset health probe still fails

## Verification

- Added host coverage for recovery on the second probe, recovery only after an ADB reset, and rejection of persistent failure
- `test_test_helpers_process_wait.ps1` passed directly and through the unattended suite runner
- Scoped PowerShell code quality passed for both changed scripts
- On an existing 26-minute-old emulator, `test_menu_scale_d2` passed and the immediately following `test_merged_wall_snapshot_regression` passed all 31 steps
- The merged-wall result retained `status=ok`, the `merge_cached`/`gpu_cached_single` face, and the `rock331` behind-face cover
- Windows CMake configure/build completed successfully for D1 and D2
- `git diff --check` passed
