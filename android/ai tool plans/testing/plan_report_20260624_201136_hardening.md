Report 20260624 201136 Hardening - 2026-06-25

Goal:
- Inspect the three non-passing tests in report_20260624_201136.md and choose durable hardening updates without weakening feature coverage or stretching timeouts as a first response.

Targets:
- test_launcher_dpad
- test_msaa_fbo_smoke_d2
- test_all_extracts

Plan:
- [done] Read full logs and scripts/helpers for each failure.
- [done] Separate cascade/environment symptoms from actual brittle test contracts.
- [done] Identify focused hardening changes that improve state awareness, cleanup, or diagnostics.
- [done] Apply the smallest useful updates.
- [done] Run scoped validation for changed tests/helpers.
- [done] Record verification and residual risk.

Updates:
- test_launcher_dpad: SetupActivity button introspection now accepts a label whose center is inside the clickable bounds, which preserves exact button labels while tolerating slight text-bound overflow.
- test_msaa_fbo_smoke_d2: skip_briefing now honors its timeout across all phases and taps when no front window exists yet, so blank/briefing transitions do not hang the suite.
- test_all_extracts: large app-private source staging now pushes one temporary chunk at a time through /data/local/tmp, appends into app-private storage, deletes the chunk immediately, and verifies cumulative size.

Verification:
- Built debug APK with Gradle assembleDebug.
- Passed test_launcher_dpad.ps1 -SkipBuild.
- Passed test_msaa_fbo_smoke_d2.json5 through android/helpers/run_test.ps1.
- Passed test_all_extracts.ps1 for game_data/CD images/d1 mac 2nd bin+cue/extract_regression.json5 with full launch.
- Passed scoped android/run-code-quality.ps1 -Fix over touched Kotlin, C++, PowerShell, and this plan.

Residual Risk:
- The extraction staging fix specifically validates the previously failing 719 MB Mac BIN case; broader extraction coverage should come from the next full all-tests run.
