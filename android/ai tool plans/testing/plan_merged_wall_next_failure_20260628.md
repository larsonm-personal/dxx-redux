# Merged Wall Next Failure Plan - 2026-06-28

## Goal
Apply the same robustness process to the next remaining failure from `temp/test_reports/report_20260628_101133.md`, starting with `test_merged_wall_two_pass_debug_mode_probe`.

## Failure
- `test_merged_wall_two_pass_debug_mode_probe`: failed on `merged_wall_snapshot.center_hit_count >= 1` with `got 0`.
- `test_merged_wall_two_pass_probe` reports the same assertion failure, so any durable contract fix may apply to both scripts.

## Tasks
- [x] Inspect current merged-wall scripts, prior plans, and snapshot fields
- [x] Choose the smallest transformational contract change
- [x] Implement the change
- [x] Run targeted verification
- [x] Run scoped code quality and sanity checks
- [x] Summarize outcome and remaining risk

## Decision
The two-pass probes should verify that the forced route submits projected target-face geometry through the intended implementation. Requiring `center_hit` conflates that contract with an incidental camera/framing detail. The snapshot already exposes stronger fields for this test: target face identity, `submit_nv`, `bbox_area`, route, implementation, and `merged_wall_last_draw_state.screen_area`.

## Verification
- `android\helpers\run_test.ps1 test_merged_wall_two_pass_debug_mode_probe.json5 -TimeoutSeconds 600`: passed.
- `android\helpers\run_test.ps1 test_merged_wall_two_pass_probe.json5 -TimeoutSeconds 600`: passed.
- `android\run-code-quality.ps1 -Fix -Paths <changed files>`: passed.
