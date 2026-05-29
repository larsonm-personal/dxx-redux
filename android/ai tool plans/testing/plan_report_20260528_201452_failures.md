# Plan: report 20260528 201452 failures

## Goal
Improve the remaining failures from `temp/test_reports/report_20260528_201452.md` with focused Android/test fixes and preserve existing D1/D2 host behavior.

## Failures
- `test_door45_cover_gpu_regression`
- `test_door45_pose_repro`
- `test_keyboard_defaults` timeout

## Steps
- [ ] Read the report, per-test logs, and relevant scripts.
- [ ] Classify each failure as test timing, automation harness, rendering regression, or engine behavior.
- [ ] Patch the narrowest code or script change for the door45 failures.
- [ ] Patch the narrowest code or script change for the keyboard timeout.
- [ ] Run focused validation for changed tests.
- [ ] Run required code-quality/build checks.
- [ ] Record final results and remaining risk here.

## Notes
- Keep D1/D2 source changes minimal and mirrored if the same Android hook applies to both.
- Prefer introspection/probe data over screenshots for automated diagnosis.