Pilot Long-Hold Delete Hardening - 2026-06-24

Goal:
- Harden another failure from report_20260623_230322.md without weakening the test or reaching for timeout changes.

Target:
- test_pilot_long_hold_delete_unified, especially the D1 failure after returning from the game to the launcher/pilot delete flow.

Plan:
- [done] Inspect the report log, script, and launcher/game automation handoff around the D1 failure.
- [done] Identify the semantic wait or state transition that is currently brittle.
- [done] Patch the smallest script or automation helper change that makes the behavior fault tolerant.
- [done] Run the focused test for D1 first, then the unified test if practical.
- [done] Record verification and residual risk.

Notes:
- The report's D1 run did not fail inside the long-hold delete assertion. It yielded to the launcher, resumed, tapped the second launch, then the emulator health check failed.
- The script used a fixed 500 ms pause after enter_launcher before relaunch. That makes the second launch depend on exit cleanup timing rather than launcher/game state.
- Added setup introspection fields game_running, has_returnable_game_activity, and running_game_pid, then changed the script to wait for game_running=false before relaunching.
- Verification:
  - Scoped code quality passed for the touched Kotlin/script/plan paths.
  - `assembleDebug` passed with JDK 21.
  - Installed the debug APK with `adb install -r`.
  - Focused D1 run of `test_pilot_long_hold_delete_unified.json5` passed.
  - Unified run of `test_pilot_long_hold_delete_unified.json5` passed for both games.
- Residual risk:
  - The new setup introspection field intentionally reports launcher-observable process/activity state; it does not inspect game internals after the game has exited.
