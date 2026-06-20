# Coop Level 2/3 Load Analysis - 2026-06-19

## Context
- User selected level 3 for a coop game, but the host still loaded level 2.
- Joining player still gets a corrupted-palette/error dialog during initial join, and only joins after the server has finally loaded level 2.
- New log with extra diagnostics: `game_data/coop_load_still_wrong/debuglog_20260619_194828.txt`.

## Plan
- [x] Extract the launcher/lobby START, auto-host, and native level-selection lines from the log
- [x] Extract join/reject/error-dialog lines around the failed and eventual successful join
- [x] Compare log values to Kotlin `GameLaunchInfo` / `LobbyService` / `CreateGameDialog` level plumbing
- [x] Compare native auto-host and coop restore paths for any place level 3 can be overwritten by saved level 2
- [x] Report likely root cause and next implementation target

## Findings
- The host was already advertising and launching `lvl=2` before native restore logic ran.
- Native auto-restore then armed slot 8 and timed out while the joining player was already connected but killed, which matches the repeated welcome/rejoin loop.
- The launcher could keep a level 2 save selected after the host typed level 3, then write slot 8 and let the LAN save offer set the hosted level back to level 2.
- Implementation target: only honor a selected coop restore save when its level matches the hosted level; otherwise write the explicit start-fresh sentinel.
