# Report 2026-06-20 next failing test

## Goal
- Pick one remaining non-passing test from `temp/test_reports/report_20260620_133235.md`
- Find a narrow root cause without disturbing recent fixes
- Patch the smallest appropriate source or test boundary
- Run scoped quality checks and focused verification

## Plan
- [x] Read repo instructions and report context
- [x] Inspect remaining failing logs and choose target
- [x] Patch the selected issue
- [x] Run focused verification
- [x] Run scoped code quality
- [x] Record results and residual risk

## Target
- Selected `test_kconfig_keyboard_stage_d2`.
- The report shows a timeout waiting for the `Controls` submenu after leaving the keyboard kconfig page. Earlier transitions in the same log took roughly 6 to 7 seconds, but the script used 3 second waits for menu and kconfig transitions.

## Fix
- Widened this script's menu-transition waits from 3 seconds to 10 seconds while leaving the assertions unchanged.

## Results
- Cleared logcat and ran `android/helpers/run_test.ps1 -ScriptName test_kconfig_keyboard_stage_d2.json5 -Game d2`; the runner returned `EXIT: 0`.
- Durable result: `{"result":"PASS","steps_completed":47,"total_steps":46,"elapsed_ms":10876}`.
- The automation log shows each return from kconfig back to the `Controls` submenu completed, including step 27, which was the report failure point.
- Scoped quality check passed for `android/game_scripts/test_kconfig_keyboard_stage_d2.json5`.

## Residual Risk
- This fixes a brittle test timeout rather than changing menu performance. If the underlying transition speed becomes much slower than 10 seconds under full-suite load, the test may still expose that separately.
