# Plan: Autosave Post-Launch Timeout 2026-05-15

## Goal

- Resolve the current autosave launcher test failures that now stall after game launch at `wait_for screen_mode = menu`
- Distinguish emulator churn from a real game/automation regression before changing shared code or scripts

## Outcome

- Root cause was engine startup ordering, not launcher handoff or emulator health
- On resume-save launches, `d1/main/inferno.c` and `d2/main/inferno.c` were still calling `show_titles()` before the startup resume restore path ran
- With movie assets present, the second autosave launch entered `intro.mve`, so the launcher automation waited for in-game state that never arrived within the timeout
- The fix was to skip `show_titles()` when `-resume-save` is present, keeping startup resume on the direct restore path
- A secondary launcher hardening change remains in place: `SetupActivity.waitForAutomationGameExit()` now kills a stale returnable `:game` process before resuming launcher automation

## Local hypothesis

- The autosave scripts themselves were already passing earlier the same day, so the immediate timeout is likely caused by either a changed on-device runtime state or a flaky launch path that leaves the game in a different post-launch surface than the script expects
- The cheapest discriminating check is to rerun one failing autosave test on a freshly healthy emulator and capture live introspection right after failure

## Cheap checks

- Restore a healthy primary emulator
- Re-run `test_autosave_resume_unified.json5` for D2
- If it fails again, dump `introspect.json`, `automation_result.json`, and `automation_log.jsonl`
- Compare the live failed state against the earlier same-day passing logs before editing code

## Steps

- [x] Restore healthy emulator
- [x] Re-run one autosave launcher test with durable output capture
- [x] Dump live introspection and automation result after failure or success
- [x] Decide between script fix, launcher fix, or engine fix
- [x] Update this plan with outcome

## Validation

- `android/gradlew.bat :app:assembleDebug` passed after the native and engine changes
- APK reinstall on `emulator-5554` succeeded
- `android/run_test.ps1 -ScriptName test_autosave_resume_unified.json5 -Game d2` passed with `{"result":"PASS","steps_completed":33,"total_steps":32,"elapsed_ms":379}`
- `android/run_test.ps1 -ScriptName test_autosave_resume_unified.json5 -Game d1` passed with `{"result":"PASS","steps_completed":32,"total_steps":31,"elapsed_ms":267}`
- `android/run_test.ps1 -ScriptName test_autosave_resume_missing_pilot_unified.json5 -Game d2` passed with `{"result":"PASS","steps_completed":40,"total_steps":39,"elapsed_ms":25395}`
- `android/gradlew.bat :app:compileDebugKotlin` passed after the scoped code-quality fix pass on `SetupActivity.kt`