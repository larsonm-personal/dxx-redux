# Plan: report 20260528 115705 failures

## Goal
Improve the four non-passing results from `temp/test_reports/report_20260528_115705.md` without broad source churn.

## Failures in scope
- `test_abort_game_to_main_menu_d2`
- `test_launch_to_automap`
- `test_launcher_dpad`
- `test_saf_archiver`

## Steps
- [x] Read the report and relevant repository notes.
- [x] Collect complete logs and test definitions for each failing case.
- [x] Identify whether each failure is app behavior, automation timing, stale state, or a test assumption.
- [x] Patch focused fixes in scripts or launcher/game code.
- [x] Run targeted validation for changed tests.
- [x] Update this plan with results and remaining risk.

## Notes
- Prefer harness/state fixes when the report shows stale state or missing readiness checks.
- Avoid broad d1/d2 source edits unless a game-engine behavior is clearly implicated.
- `test_abort_game_to_main_menu_d2`: exit autosave can be immediately followed by a highest-progress save, making the default resume candidate `auto_progress`.
- `test_launch_to_automap`: D2 `skip_briefing` direct-closes the briefing window and can strand level startup.
- `test_launcher_dpad`: main-page button focus is not reflected in setup introspection.
- `test_saf_archiver`: `test_saf_basic` assumes intro is active; prior prefs may skip it and leave the script waiting forever.

## Results
- Patched Android autosave ordering so highest-progress metadata is written before exit/minimize autosaves, leaving the exit autosave as the newest resume candidate.
- Patched `skip_briefing` to dispatch ESC through the front fullscreen window handlers instead of direct-closing D2 movie/briefing windows.
- Patched setup button introspection to treat focused child text nodes as button focus, and updated the DPAD test to enter controller-navigation mode with a non-navigation button before asserting focus on phone-style emulators.
- Removed the brittle `intro_active = true` wait from the SAF smoke automation script.
- Validation passed: `android\run-code-quality.ps1 -Fix`, `android\gradlew.bat :app:assembleDebug`, `test_abort_game_to_main_menu_d2`, D2 `test_launch_to_automap`, `test_launcher_dpad`, and `test_saf_archiver`.

## Remaining risk
- Launcher DPAD focus policy intentionally avoids seeding focus in touch mode until controller navigation is active, so focused tests should opt into controller mode before expecting focused buttons.