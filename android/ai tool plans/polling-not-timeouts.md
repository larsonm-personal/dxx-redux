# Plan: Replace Fixed Waits with Polling/Callbacks

## Principle
Timeouts are safety nets for the unhappy path. The happy path should be driven by
polling/callbacks that proceed as soon as the condition is met. Reducing timeout
values is wrong -- the right fix is adding polling so the timeout is never hit.

## Phase 1: Revert safety-net timeout reductions [DONE]
- Reverted all `timeout_ms` reductions on `wait_for` actions in JSON5 scripts
- Reverted `run_all_tests.ps1` per-test timeout overrides

## Phase 2: Add `timeout_ms` to `select` (C++ change) [DONE]
In `game_automate.cpp` STEP_SELECT Phase 0: if `select_find_item` returns false
and `timeout_ms > 0` and elapsed < timeout_ms, break (retry next frame)
instead of calling `stop_script_fail`. Makes `select` self-polling.

## Phase 3: Replace `post_delay_ms` with polling in JSON5 scripts [DONE]
- All `select`, `key`, `skip_briefing` actions: `post_delay_ms: 100`
  (next step polls, so minimal delay needed)
- All `select` actions: `timeout_ms: 5000` (self-polling)

## Phase 4: Replace `wait_ms` with `wait_for` / remove redundant waits [DONE]
- Removed `wait_ms` steps that precede `select` or `wait_for` (redundant)
- Kept 21 gameplay/functional `wait_ms` (dpad timing, fire cooldown, etc.)

## Phase 5: Replace PS1 Start-Sleep with polling [DONE]
- test_double_launch.ps1: launch → poll GetGamePid; introspect → poll loop;
  HOME → reduced to 500ms; Phase 3 setup → Wait-SetupActivityReady;
  Phase 5 health → poll introspection until responsive
- test_extract.ps1: emu restart → poll adb devices + boot prop;
  game launch → poll pidof game process
- test_mp.ps1: refresh_lobbies/set_ready → removed pre-sleep (polling follows);
  start_game/launch_game → poll for game PID; sync wait → poll introspection
- test_lan.ps1: host wait → poll for game PID via Wait-ForCondition
- test_saf_archiver.ps1: post-launch → removed (polling loop follows);
  emu restart → poll adb devices

## Phase 6: Test [DONE]
- NDK build: all 3 architectures pass (arm64-v8a, armeabi-v7a, x86_64)
- Code quality: all linters pass (clang-format, ktlint, PSScriptAnalyzer, shellcheck, shfmt)
- test_launch_to_automap: PASS (31/30 steps, 12241ms)
