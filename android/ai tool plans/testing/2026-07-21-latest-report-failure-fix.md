# Latest test report failure fix

## Plan

- [x] Review all four failures in `report_20260721_001118.md` and select one with a plausible cross-test emulator-state cause
- [x] Trace the failing test, runner setup, and state cleanup to identify the root cause
- [x] Implement the smallest root-cause fix without extending timeouts
- [x] Add or extend regression coverage for sequential-run state contamination
- [x] Run scoped code quality, the focused test, a stateful sequence, and the relevant build/test verification
- [x] Record findings and completed verification in this plan

## Findings

- Selected `test_random_level_preview`, which failed after reaching a live preview because its first native framebuffer probe was black
- The host-driven test bypassed `run_test.ps1`, so it did not use the normal stop-then-reset ordering before launch
- The readiness loop retained its first active native snapshot even when the framebuffer or presentation probes had not rendered readable map pixels yet
- The fix force-stops the prior app process, resets shared test state, and refreshes the snapshot until the existing render assertions are satisfied within the unchanged 180-second test budget

## Verification

- `test_resolution_unified.json5` passed with 32/32 steps, followed immediately by the fixed `test_random_level_preview.ps1` using report seed `876943607`
- The sequential random preview passed its native framebuffer, presented surface, composed window, input, close, and cleanup assertions
- Scoped `android/run-code-quality.ps1 -Fix` passed for the changed PowerShell test
- `run-windows-build.ps1` completed successfully for D1 and D2
- `git diff --check` passed
