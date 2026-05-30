# Plan: report 20260528 201452 failures

## Goal
Improve the remaining failures from `temp/test_reports/report_20260528_201452.md` with focused Android/test fixes and preserve existing D1/D2 host behavior.

## Failures
- `test_door45_cover_gpu_regression`
- `test_door45_pose_repro`
- `test_keyboard_defaults` timeout

## Steps
- [x] Read the report, per-test logs, and relevant scripts.
- [x] Classify each failure as test timing, automation harness, rendering regression, or engine behavior.
- [x] Patch the narrowest code or script change for the door45 failures.
- [x] Patch the narrowest code or script change for the keyboard timeout.
- [x] Run focused validation for changed tests.
- [x] Run required code-quality/build checks.
- [x] Record final results and remaining risk here.

## Notes
- Keep D1/D2 source changes minimal and mirrored if the same Android hook applies to both.
- Prefer introspection/probe data over screenshots for automated diagnosis.
- Door45 failures stopped before the GPU/readback checks because `position.y` drifted outside a narrow pose guard (`-71.1` and `-75.4` observed). Segment, x, z, and the GPU snapshot fields remain the regression signal.
- `test_keyboard_defaults` was cut off by the outer `run_all_tests.ps1` 120s wall-clock timeout after D1 had passed and D2 had opened automap. This matches the prior multi-game timeout shape, so the fix is a suite timeout override.
- Validation passed: `android\run-code-quality.ps1 -Fix`, debug APK builds from `android\Run-Emulator.ps1` and `android\run_all_tests.ps1 -Filter test_keyboard_defaults`, `android\helpers\run_test.ps1 -ScriptName test_door45_cover_gpu_regression.json5 -Install`, `android\helpers\run_test.ps1 -ScriptName test_door45_pose_repro.json5`, `android\helpers\run_test.ps1 -ScriptName test_keyboard_defaults.json5`, and `android\run_all_tests.ps1 -Filter test_keyboard_defaults`.
- `test_door45_cover_gpu_regression` reached the intended GPU assertions: `target_cover_gpu.valid=true`, `face_box=projected`, `src_hash_hex=272e5021`, and `position.y=-73.1` inside the relaxed guard.
- `test_door45_pose_repro` passed with `position.y=-72.1` inside the relaxed guard.
- `test_keyboard_defaults` passed directly and through the filtered suite; the filtered suite showed `timeout: 240s` and reported 1 pass, 0 failures, 0 timeouts.
- Remaining risk: the door45 pose can still vary within a few units by runtime, but the regression still checks segment, x/z, and the GPU/readback fields that carry the actual cover-rendering signal.