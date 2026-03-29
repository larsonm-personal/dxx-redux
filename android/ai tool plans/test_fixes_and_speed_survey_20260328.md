# Test Fixes and Speed Survey - 2026-03-28

## Source: temp\test_reports\report_20260328_203226.md
23 passed, 2 failed, 3 skipped

## Phase 1: Fix test_axis_mapping D1 failure [DONE]

### Problem
Step 16 assertion `heading_time != 0` failed with heading_time=0. D2 passed the same test.

### Root cause analysis
- kconfig_read_controls requires CONTROL_USING_JOYSTICK (ControlType=1)
- D1's config.c lacked android_apply_initial_defaults() for first-launch
- android_apply_gamepad_defaults() sets ControlType=1 on player load, but descent.cfg also needed defense-in-depth

### Changes
- `d1/main/config.c`: Added android_apply_initial_defaults() that sets ControlType=CONTROL_USING_JOYSTICK and AutomapFreeFlight=1, called when descent.cfg doesn't exist
- `android/app/src/main/cpp/shared/game_automate.cpp`: Added diagnostic dump on ASSERT failure (control_type, heading_time, pitch_time, bank_time, slide/bank_on_state, raw_joy_axis)

### Verification
- Built APK, deployed, ran test_axis_mapping D1 -- PASS (all 96 steps)

## Phase 2: Fix test_saf_archiver timeout [DONE]

### Problem
Test timed out at 6:01 during `am force-stop` adb command (19th test in suite, emulator under stress).

### Root cause
Local `Adb` function in test_saf_archiver.ps1 had no timeout wrapping, unlike `Adb-Timeout` in test_helpers.ps1.

### Changes
- `android/tests/test_saf_archiver.ps1`: Added `Adb-WithTimeout` function using ProcessStartInfo pattern, replaced both `am force-stop` calls (Step 2 and cleanup) with timeout-wrapped versions (10s timeout)

## Phase 3: Speed survey implementations [DONE]

### test_lan_discovery (was ~35s, target ~20s)
- `android/tests/test_lan_discovery.ps1`: DurationSec parameter default 60 -> 30

### test_mp (was ~95s, target ~70s)
- `android/tests/test_mp.ps1`: 7 Wait-ForCondition PollMs values 1500 -> 750

### test_axis_mapping (was D1 63s D2 45s, target ~20s)
- `android/game_scripts/test_axis_mapping.json5`: post_delay_ms 500 -> 300 (axis injection), 200 -> 100 (axis reset)

### Verification
- D1 axis test: 18s (was 63s) -- PASS
- D2 axis test: 12.5s (was 45s) -- PASS

## Phase 4: Lint [DONE]
- Ran run-code-quality.ps1 --fix
- Fixed PSScriptAnalyzer whitespace issue (space after comma in AdbArgs arrays)
- All linters pass: clang-format, ktlint, PSScriptAnalyzer, shellcheck, shfmt

## Files modified
| File | Change |
|------|--------|
| d1/main/config.c | android_apply_initial_defaults() for ControlType |
| android/app/src/main/cpp/shared/game_automate.cpp | Diagnostic dump on ASSERT failure |
| android/tests/test_saf_archiver.ps1 | Adb-WithTimeout for force-stop calls |
| android/tests/test_lan_discovery.ps1 | DurationSec 60->30 |
| android/tests/test_mp.ps1 | PollMs 1500->750 (7 calls) |
| android/game_scripts/test_axis_mapping.json5 | post_delay_ms 500->300, 200->100 |
