# Plan: test_input_demo_runtime_smoke follow-up 2026-05-23

- [x] Capture the direct runner, prior notes, and nearby phase plan anchors
- [x] Inspect the latest `test_input_demo_runtime_smoke` failure output and repro path
- [x] Trace the owning runtime smoke failure point in the PowerShell runner or replay launch path
- [x] Form one local hypothesis and identify the cheapest discriminating check
- [x] Apply the smallest fix in the owning path
- [x] Run focused validation for `test_input_demo_runtime_smoke`
- [x] Update this note with the fix, validation, and any remaining risks

## Notes

- Direct runner: `android/tests/test_input_demo_runtime_smoke.ps1`
- Related wrapper docs: `android/tests/HOST_INPUT_DEMO_REPLAY.md`
- Prior repo note confirms the supported smoke launch pattern is a sandboxed windowed `ProcessStartInfo` launch that waits on `result.actual.json`
- Existing broader phase note: `android/ai tool plans/plan_input_demo_phase5_desktop_runtime_smoke.md`
- Failure from `temp/test_reports/test_input_demo_runtime_smoke_20260522_232545.log` never reached replay validation. The guardrail rebuild for `buildd1/main/dxx-redux-d1.exe` failed first.
- Root cause: `d1/main/game.c:init_cockpit()` was corrupted by an accidental splice of input-demo replay/profiling code. That left Android-only profiling calls and replay-step logic inside cockpit initialization, which broke the Windows host build used by the runtime smoke runner.
- Fix: restore `init_cockpit()` to the normal D1 cockpit/render-buffer setup path and remove the stray replay/profiling block from that function.
- Focused validation:
	- `android/tests/test_input_demo_runtime_smoke.ps1 -Game d1`
	- `android/tests/test_input_demo_runtime_smoke.ps1`
- Outcome: both D1 and D2 runtime smoke paths pass. D1 now rebuilds cleanly and reaches replay; the full smoke run reports `PASS d1` and `PASS d2`.
- Residual note: the D2 smoke run still prints RNG mismatch lines while reporting `PASS d2` because the smoke test keys off the replay result trailer, not RNG-trace parity.