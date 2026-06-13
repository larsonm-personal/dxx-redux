# Report 20260612 220011 autosave missing-pilot timeout

## Goal
Fix `test_autosave_resume_missing_pilot_unified` timing out in
`temp/test_reports/report_20260612_220011.md` without increasing timeouts.

## Plan
- [x] Read project instructions and report snippet
- [x] Inspect the full timeout log and automation script
- [x] Identify whether the stall is in launcher resume, game resume, or monitor
- [x] Patch the smallest root cause
- [ ] Run the focused failing test
- [x] Run scoped quality checks and record results

## Initial signal
- The report shows the script resumed at step 23 and observed
  `resume_offer_enabled = true`.
- The launcher later logged a `resume-launch-request` for the expected D2
  autosave path, but the suite wrapper killed the test after 300 seconds.

## Findings
- The D1 half of the same missing-pilot script reached the second game
  activity after `clear_pilot_files`, resumed the save, and continued.
- The D2 half reached `resume-launch-request` after deleting the pilot files,
  then emitted no later `DXX-Automate` game-surface or script-start logs.
- The launcher resume path could decide there was no returnable activity from
  `game_activity_state.json` while the `:game` process still existed. That can
  route a transient resume launch into a stale game process instead of a fresh
  startup.

## Patch
- Added direct detection of the package `:game` process.
- Automation launches now wait for any existing game process to exit, killing it
  through the existing launcher-continue cleanup path if needed, before starting
  the next game/resume activity.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths ...`
- `gradlew.bat :app:compileDebugKotlin`
- `gradlew.bat :app:assembleDebug`
- `gradlew.bat :app:testDebugUnitTest`
- Focused emulator rerun is still pending because no emulator was attached.
