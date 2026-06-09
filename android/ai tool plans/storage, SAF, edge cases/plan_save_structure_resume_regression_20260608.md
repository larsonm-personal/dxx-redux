# Plan: save structure resume regression

Investigate and fix the June 7 integration report failures around autosave resume
and weapon autoselect after recent save/load file layout changes.

## Steps

- [x] Read failing report logs and automation scripts to identify the stuck step
- [x] Compare launcher-side save discovery/cleanup paths with native save paths
- [x] Patch the smallest shared source of truth for save file layout
- [x] Run focused code quality and targeted tests for the three failing cases
- [x] Record final outcome and any remaining risk

## Notes

- `test_autoselect_crash_unified` was failing before entering the game because
  the launcher was on the D1 panel and the script tapped `Launch Descent 2`.
- The autosave resume timeouts left `pending_resume_launch.json` and the
  consumed transient launch token behind. That stale state can redirect later
  launches back to setup before the game surface starts.
- `test_autosave_resume_missing_pilot_unified` also still asserted the old flat
  `Players/player.sg8` restore path; the current save-set path is under
  `Players/save_sets/single/player/...`.
- Focused validation passed: code quality, Gradle debug unit tests, and the
  three report failures rerun on emulator.
