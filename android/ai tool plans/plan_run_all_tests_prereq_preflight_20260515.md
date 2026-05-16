# Plan: Run All Tests Prereq Preflight 2026-05-15

## Goal

- Make `android/run_all_tests.ps1` cure runner-owned prerequisite problems before the first test starts
- Abort the suite before any test runs if runner-owned prerequisites cannot be restored
- Work through the remaining failures that are still real after the preflight fix

## Local hypothesis

- The current runner provisions infra tier by tier and only checks for shallow signals like `adb devices`, so a stale emulator/app state can survive until the first launcher-backed test and then fail as `SetupActivity not responding`.
- The smallest root fix is to add an upfront preflight phase that resolves the required infra for the selected tests, uses the stronger helper checks already present in `android/test_helpers.ps1`, and exits before Tier 0 if that preflight cannot restore the environment.

## Cheap check

- Patch `android/run_all_tests.ps1` to compute required suite infra before running any test and validate emulator health, APK/data provisioning, launcher readiness, and server availability upfront.
- Re-run one previously failing launcher-backed test through `run_all_tests.ps1 -Filter ...` and confirm it either passes or moves past the old `SetupActivity not responding` failure.

## Steps

- [x] Inspect the reported failures and identify the runner-owned prereq path to fix
- [x] Add upfront preflight and fail-fast behavior to `android/run_all_tests.ps1`
- [x] Run a focused suite validation against one prior launcher-backed failure
- [ ] Fix remaining reported failures that are still local and actionable
- [x] Update this plan with outcomes and validation notes

## Outcomes

- `android/run_all_tests.ps1` preflight was repaired earlier in the session so the suite resolves runner-owned prerequisites before Tier 0 and aborts before running tests if recovery fails.
- Imported-set path drift was fixed in shared helpers and the affected test scripts.
- `android/run_test.ps1` now treats the default `300` timeout as a floor instead of collapsing to the script estimate, and launcher-backed scripts now get a `600s` minimum default floor.
- `android/test_helpers.ps1` now extends long launcher-backed tests from durable signals instead of a plain wall clock:
	- ogl cache start/progress/done tracking
	- post-cache grace
	- automation-log-driven grace after timeout
	- stronger emulator health check that now rejects a missing Android package service before install starts
- Shared launcher automation was hardened in `LauncherScriptExecutor.kt` by giving `launches_game=true` button taps a longer default timeout.
- Shared native automation in `android/app/src/main/cpp/shared/game_automate.cpp` was hardened for the cold D2 path:
	- `skip_briefing` now completes as soon as the game window is reachable after the long blocking load
	- supported `STEP_KEY` actions now dispatch direct `EVENT_KEY_COMMAND` events to game and automap windows when possible, which fixed TAB opening automap on the long D2 path
	- the local unused-variable warning in `close_front_fullscreen_window()` was removed
- The temporary D2 `ogl_cache` diagnostics were reduced from per-bitmap spam to coarse checkpoints so the runner still sees progress without flooding logcat during long cold loads.

## Validation Notes

- Repeated `android\gradlew.bat assembleDebug --console=plain` builds succeeded after the runner and automation changes.
- Focused fresh-install D2 launcher runs of `test_launch_to_automap.json5` progressed far past the original failures:
	- launcher `Launch Descent 2` button was reached reliably after the timeout change
	- `skip_briefing` advanced into `in_game`
	- automap TAB open was validated after direct game-window key dispatch
- Remaining blocker at handoff time is environment-heavy and still open:
	- repeated very long emulator runs eventually became inconsistent, including package-service loss, `SetupActivity not responding`, emulator crash under load, or a cold D2 cache run that did not emit the expected progress signals in time
	- because of that instability, the latest automap-close validation did not finish with a clean end-to-end PASS in this tranche even though the functional path moved substantially further than before

## Next Tranche

- Local hypothesis:
	- many of the new launcher-backed failures are not pilot-select or intro regressions at all; they are setup-state contamination because the shared runner reset preserves pilots but also leaves exit/minimize save files behind, which triggers the launcher `Load Last Save` offer in unrelated tests
	- `android/run_all_tests.ps1` still treats the regression demo corpus as a single wrapper test, so the startup counts do not expose how many demos will actually run and the unattended suite only covers the headless pass today
- Cheap checks:
	- clear resume-save artifacts in the shared reset path, rerun one launcher-backed failure, and confirm the expected `Descent 2` / `Launch Descent 2` buttons return without touching each individual script
	- update the demo wrapper plus `run_all_tests.ps1` catalog/summary so the suite prints the discovered regression demo count and runs both headless and graphics regression passes as separate unattended entries
- Remaining work in this tranche:
	- [x] Clear save-file resume offers in the shared launcher reset path
	- [x] Update `run_all_tests.ps1` summary output to print discovered regression demo counts
	- [x] Run committed regression demos in both headless and graphics unattended modes
	- [x] Re-run the focused failing slice from the new report and confirm the dominant launcher failures move past SetupActivity resume offers

## Latest Outcomes

- `android/run_test.ps1` now clears launcher save files before sending `SETUP_AUTOMATE`, so unrelated launcher-backed tests start from a clean launcher instead of inheriting the `Load Last Save` resume offer from a prior autosave test.
- `LauncherScriptExecutor.kt` now includes `.sg*` and `.mg*` save slots in `reset_state`, which keeps launcher script resets aligned with the clean-start intent instead of only clearing pilots/config.
- `android/tests/test_input_demo_regressions_graphics.ps1` was added as a second unattended wrapper around the committed regression corpus so the suite now covers both graphics and headless replay passes.
- `android/run_all_tests.ps1` now:
	- treats `test_input_demo_regressions_graphics` as the same base demo suite for filtering/timeouts
	- reports discovered regression demo counts plus selected replay-run totals at startup
	- keeps `-Filter test_input_demo_regressions` selecting both the headless and graphics unattended passes

## Latest Validation

- Focused launcher-backed validation after the save-reset fix moved past the previous SetupActivity contamination symptom and reached the normal `Descent 2` / `Launch Descent 2` launcher path, then progressed deep into the D2 in-game launch flow instead of failing on the stray resume-offer UI.
- `android\run_all_tests.ps1 -Filter test_input_demo_regressions -StopOnFail` passed with:
	- `Tests found: 2 total, 2 runnable`
	- `Demo regressions: 11 demo(s), modes: graphics+headless, replay runs: 22`
	- both `test_input_demo_regressions_graphics` and `test_input_demo_regressions` reporting `RESULT: PASS (11 regression demo(s))`

## Latest Targeted Failures

- `android/tests/test_launcher_dpad.ps1` was updated to the current launcher layout and interaction model:
	- main-page assertions now use `Define Controls` and `Touch Layout`
	- controller-page detection now keys off `Cancel` plus `Save`
	- the PS1 helper now reads one fresh setup introspection snapshot after each controller action instead of relying on the old `cmd timeout` wait helper
- `android/game_scripts/test_title_music_skip_pref_unified.json5` now makes its baseline phase self-contained by forcing `skip_intro_movie=false`, skipping the intro during clean pilot creation, and still restoring the pref to `false` at the end.
- `android/test_helpers.ps1` now restarts `SetupActivity` for each distinct `LAUNCHER_CONTINUE next_step`, which fixed multi-phase launcher scripts that return to the launcher more than once in a single run.
- `android/game_scripts/test_pause_menu_return.json5` now explicitly selects `Descent ${GAME_NUM}`, forces `skip_intro_movie=false`, and skips the intro before waiting for the initial menu flow.
- Shared native automation in `android/app/src/main/cpp/shared/game_automate.cpp` was tightened for menu/cutscene handling:
	- `skip_briefing` no longer direct-closes movie windows and instead uses the normal movie abort input path
	- direct game-window dispatch now lets `KEY_ESC` fall back to injected input so the in-game ESC menu does not immediately re-consume the same event on open

## Latest Targeted Validation

- `android\tests\test_launcher_dpad.ps1` passed end-to-end after updating the launcher button expectations and removing the flaky wait helper.
- `android\run_test.ps1 -ScriptName test_title_music_skip_pref_unified.json5 -Game d2 -TimeoutSeconds 180` exited `0` after the baseline intro-skip and multi-phase launcher resume fixes.
- `android\run_test.ps1 -ScriptName test_title_music_skip_pref_unified.json5 -Game d1 -TimeoutSeconds 180` exited `0` as a cross-game check of the same script and launcher resume changes.
- `android\gradlew.bat :app:assembleDebug --no-daemon` passed after the shared `skip_briefing` and ESC automation changes.
- `android\run_test.ps1 -Install -ScriptName test_pause_menu_return.json5 -Game d2 -TimeoutSeconds 180` exited `0` after the shared movie-skip and ESC input fixes.
- `android\run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d1 -TimeoutSeconds 180` exited `0` as a cross-game check of the same automation changes.