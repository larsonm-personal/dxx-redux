# Multiplayer Resume, Texture, Palette Notes - 2026-06-15

## Context
- User saw D2 base-game multiplayer issues after committed D1-in-D2 changes:
  - level textures messed up on both host and client
  - connection error dialog has bad palette/text
  - resuming a saved level 2 game starts host on level 1 and the second player does not join

## Plan
- [x] Recheck current committed state and repo instructions
- [x] Inspect launcher-to-engine resume level handoff
- [x] Fix or instrument resume level mismatch if the launcher has the save level available
- [x] Add narrow texture-state instrumentation around D2 base multiplayer level load if needed
- [x] Add narrow palette/dialog instrumentation or fix if the connection error path is confirmed
- [x] Run scoped validation feasible in this session

## Findings
- Initial suspicion: the resume selection writes `coop_restore_slot.txt`, but host launch may still use the dialog's level number. If that remains level 1 for a level 2 save, joiners request/sync against level 1 while the host later tries to restore level 2.
- Texture corruption on both host and client is unlikely to be only late-join object sync. Shared level mutation candidates are multiplayer goal texture application, monitor/destroyed texture state, or asset state left from D1-in-D2 transitions.
- Confirmed online and LAN lobby restore offers could select a restore save after lobby creation without updating the advertised/active launch level.
- Confirmed `DUMP_LEVEL` and related UDP rejection dialogs could be shown while the D2 level palette was active.
- Added Android `DLOG_TEXTURE` phase signatures around D2 multiplayer level load to compare host/client texture table and segment texture state on the next repro.
