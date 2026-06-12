# Mission ZIP batch used old code

## Goal
- Determine why a full mission ZIP analysis run could leave `outerrch11.json` with the old truncated level name after the native fix was built.
- Identify whether the batch script skipped install, reused an old APK, copied an old temp artifact, or did not reach `outerrch11`.
- Document the correct rerun path.

## Plan
- [x] Inspect mission ZIP batch temp directories and timestamps
- [x] Inspect `run_mission_zip_batch.ps1` install and output behavior
- [x] Check whether a build-only command updates the installed app used by automation
- [x] Identify the likely failure mode for the stale `outerrch11.json`
- [x] Provide a concrete verification/rerun command

## Result
- The 20260611_202500 run installed an APK that did contain `read_level_display_name()`.
- The helper still failed because it parsed from the start of the `.rdl` wrapper instead of seeking to `gamedata_offset`, where `load_game_data()` reads `Current_level_name`.
- Fixed the helper to read the `PLVL` wrapper, seek to `gamedata_offset`, then read the full title.
- Verified with a focused `outerrch11.zip` batch run at 20260611_221257; it generated `Defense Outpost for Orcean Militaries`.
