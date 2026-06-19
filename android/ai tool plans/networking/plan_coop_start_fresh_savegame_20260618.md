# Coop start-fresh savegame bug plan

## Goal

Make multiplayer coop "start fresh" reliably start a new game without loading stale coop savegames.

## Plan

- [x] Create a focused investigation plan.
- [x] Trace the launcher start-fresh and restore paths, including persisted coop resume fields.
- [x] Trace native/game startup save selection for coop sessions.
- [x] Add or extend tests that prove start-fresh clears or ignores stale restore/save state.
- [x] Fix the smallest stale-state leak found.
- [x] Run scoped formatting, targeted tests, and the relevant build or test runner.

## Notes

- User reports updated code still loads bogus saves when "start fresh" is selected.
- Treat this as likely stale persisted resume state or a native fallback loading coop autosave despite a fresh selection.
- Root cause found: "start fresh" was represented as an absent restore-slot file, but lobby save-offer UI treated absence as "no choice yet" and could auto-select a matching save again.
- Fix direction: write an explicit fresh sentinel, have UI distinguish no choice from explicit fresh, and parse native restore-slot text exactly.
- Verification: scoped `android/run-code-quality.ps1 -Fix`, focused `:app:testDebugUnitTest`, and `:app:assembleDebug` all passed.
