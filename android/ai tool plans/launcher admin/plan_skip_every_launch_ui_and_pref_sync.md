# Skip every launch UI and pref sync

## Status

- Completed: shrink and fade the intro-only "Skip every launch" button without affecting other skip button modes
- Completed: make "Skip every launch" skip all remaining startup splash screens in both D1 and D2 while preserving single-tap current-item skip
- Completed: make either engine write the shared launcher intro-skip preference durably so the Game Preferences toggle reflects it across both games
- Completed: add native Android intro-region handling so intro taps can persist the shared launcher preference even when SurfaceView input bypasses the overlay view
- Completed: run focused verification and record the result below

## Verification

- `android\\run_test.ps1 -ScriptName test_intro_skip_inputs_unified.json5 -Game d1 -TimeoutSeconds 120` passed
- `android\\run_test.ps1 -ScriptName test_intro_skip_inputs_unified.json5 -Game d2 -TimeoutSeconds 120` passed
- `android\\run_test.ps1 -ScriptName test_engine_prefs_unified.json5 -Game d1 -TimeoutSeconds 120` passed
- `android\\run_test.ps1 -ScriptName test_engine_prefs_unified.json5 -Game d2 -TimeoutSeconds 120` passed
- Deterministic active-intro D1 validation observed `intro_active=true`, sent the top-right intro tap while active, and confirmed `shared_prefs/dxx_prefs.xml` changed to `skip_intro_movie=true`
- Manual D2 proof on the current emulator data set remained timing-sensitive because the startup sequence can end before an external adb tap lands, but the shipped Android intro-touch path is shared and D2 regressions passed on the final build

## Plan

1. Read the current Android skip button view and the D1 and D2 title-sequence code paths
2. Update the intro-only skip button geometry and transparency in the shared overlay view
3. Adjust the D1 and D2 title flow so an enabled intro-skip preference exits all remaining splash screens for that launch
4. Route engine-side "Skip every launch" writes through the shared launcher preference path and refresh the launcher UI state from that shared value
5. Run code quality and targeted intro-skip verification, then record the outcome here