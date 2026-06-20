# Coop Level 2/3 Load Analysis - 2026-06-19

## Context
- User selected level 3 for a coop game, but the host still loaded level 2.
- Joining player still gets a corrupted-palette/error dialog during initial join, and only joins after the server has finally loaded level 2.
- New log with extra diagnostics: `game_data/coop_load_still_wrong/debuglog_20260619_194828.txt`.

## Plan
- [ ] Extract the launcher/lobby START, auto-host, and native level-selection lines from the log
- [ ] Extract join/reject/error-dialog lines around the failed and eventual successful join
- [ ] Compare log values to Kotlin `GameLaunchInfo` / `LobbyService` / `CreateGameDialog` level plumbing
- [ ] Compare native auto-host and coop restore paths for any place level 3 can be overwritten by saved level 2
- [ ] Report likely root cause and next implementation target
