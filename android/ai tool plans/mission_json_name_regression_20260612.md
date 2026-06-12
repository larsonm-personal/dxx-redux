# Mission JSON name regression

## Goal
- Investigate why `game_data/mission_files/outerrch11.json` now has a truncated `level_name` after a recent `mission_name` improvement.
- Fix the naming logic or data if appropriate, while preserving the improved `mission_name`.
- Check whether the same issue affects other mission files.

## Plan
- [x] Inspect the current diff for `outerrch11.json`
- [x] Locate the script or code that generated the mission metadata
- [x] Identify why `mission_name` improved while `level_name` regressed
- [x] Apply the smallest fix
- [x] Run a focused verification over mission files

## Notes
- `mission_name` improved because metadata requests now pass `mission_display_name`.
- The `outerrch11` level title was truncated because `Current_level_name` is capped by `LEVEL_NAME_LEN` at 35 visible characters.
- The full title is still present in the RDL header, so the metadata bridge now peeks the level header into a separate display string and keeps the gameplay buffer unchanged.
