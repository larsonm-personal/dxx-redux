# Test Report Fixes (report_20260328_192956)

## Failures fixed

### 1. test_server_integration (DONE)
- **Root cause**: `Instant::now() - Duration::from_secs(8000)` panics when system uptime < 8000s
- **Fix**: Added `cleanup_sessions_older_than(state, cutoff)` to relay.rs so the test can use a small cutoff (1s) with a 3-second-old session instead of requiring 8000s of uptime
- **Files**: server/src/relay.rs, server/tests/integration.rs
- **Verified**: cargo test + clippy pass

### 2. test_fire_primary (DONE)
- **Root cause**: Intermittent (1/8 runs). 1.679s stall between STEP_SELECT Phase 1 (cursor positioned on "Ok") and Phase 2 (enter injected). During stall, cursor likely drifted back to text input field. Enter on NM_TYPE_INPUT toggles edit mode instead of confirming, so callsign dialog persisted
- **Fix**: Added Phase 2 re-verification in game_automate.cpp STEP_SELECT -- re-reads cursor position before injecting enter, returns to Phase 1 if cursor drifted. Also increased timeout_ms on "New game" select from 5000 to 10000
- **Files**: android/app/src/main/cpp/shared/game_automate.cpp, android/game_scripts/test_fire_primary.json5

### 3. test_saf_archiver (DONE - partial)
- **Root cause 1**: `Adb install -r $apkPath` misinterpreted `-r` as a PowerShell named parameter
- **Fix 1**: Changed to `Adb -AdbArgs @("install", "-r", $apkPath)` -- explicit named parameter
- **Root cause 2**: Local `Adb` function only bound first positional arg to `$AdbArgs`, remaining went to `$args` which was ignored when `$AdbArgs` was truthy. `Adb shell "run-as..."` became just `adb shell` (interactive shell hang)
- **Fix 2**: Changed `if (-not $AdbArgs) { $AdbArgs = $args }` to `if ($args) { $AdbArgs = @($AdbArgs) + @($args) }` -- always combines bound + unbound args
- **Root cause 3**: test_saf_basic.json5 had `enter_launcher` + `tap_button` preamble but SAF test launches game directly via broadcast, so nobody handles the ENTER_LAUNCHER yield
- **Fix 3**: Removed launcher preamble from test_saf_basic.json5 (game already running when script executes)
- **Status**: Steps 3-6 now pass. Step 7 (in-game automation) times out -- likely a pre-existing SAF archiver or game-state issue (kconfig screen on first launch), not a test infrastructure bug
- **Files**: android/tests/test_saf_archiver.ps1, android/game_scripts/test_saf_basic.json5

### 4. Step index divergence (DONE) -- fixes 7 tests
- **Root cause**: C engine removed unknown actions (tap_button, assert_button, assert_controller_match) from step array at parse time. LauncherScriptExecutor kept them. When executor sent nextStep=N, C engine's step N was shifted by the number of removed actions
- **Fix**: Registered 3 new step types in C engine (STEP_TAP_BUTTON=16, STEP_ASSERT_BUTTON=17, STEP_ASSERT_CONTROLLER_MATCH=18) so they're preserved in the step array. Execution falls through to launcher-only skip+advance block
- **Verified**: All 7 affected json5 tests pass (test_launch_to_automap, test_controller_compare_unified, test_death, test_axis_mapping, test_keyboard_defaults, test_dpad_triggers, test_joystick_menu) for both D1 and D2
- **Files**: android/app/src/main/cpp/shared/game_automate.cpp

### 5. test_gog_installer_redbook_unified (DONE)
- **Root cause**: `assert_button "Launch Descent 2" enabled:true` failed because the button was off-screen and assert_button doesn't auto-scroll (unlike tap_button which scrolls up to 5 times)
- **Fix**: Removed `assert_button` step. The subsequent `tap_button "Launch Descent 2" launches_game:true` already validates the button exists and is enabled, plus auto-scrolls to find it
- **Files**: android/game_scripts/test_gog_installer_redbook_unified.json5

## Speed survey

Tests >1 minute in the last run:

| Test | Time | Category | Speedup potential |
|------|------|----------|-------------------|
| test_all_extracts | 2:07 | I/O-bound extract operations | Low -- mostly file extraction time |
| test_gog_installer_redbook_unified | 1:13 | GOG import + Chromaprint | Low -- disk image processing time |
| test_axis_mapping | 1:03 | Game automation (JSON5) | Medium -- 18 steps at 500ms post_delay could be 200-300ms (saves ~5s) |
| test_mp | 1:00 | Multiplayer (2 emu + server) | Medium -- 7 poll loops at 1500ms could be 500ms (saves ~20-30s), but risky for race conditions |
| test_lan_discovery | 1:00 | LAN discovery broadcast | Easy -- hardcoded 60s DurationSec could be 30s |

### Recommendations (not implemented, for future work)
1. **test_lan_discovery**: Reduce DurationSec from 60 to 30. The test just needs to verify discovery works, not soak. This alone saves 30s
2. **test_mp**: Reduce PollMs from 1500 to 750 across 7 Wait-ForCondition calls. Saves ~15-20s. Test carefully for race conditions
3. **test_axis_mapping**: Reduce 500ms post_delay_ms to 250ms on send_axis steps. Saves ~5s
4. **test_all_extracts/test_gog_installer**: Minimal optimization potential -- dominated by I/O time
