# Coop Restore Desync Log Study 2026-07-08

## Goal
- Identify why a LAN coop game desyncs when the host starts from a selected autosave

## Steps
- [x] Read project instructions and paired host/client logs
- [x] Extract the restore-related timeline from both logs
- [x] Map the logged restore packet handling to the source code
- [x] Identify the root cause and recommend the smallest fix path

## Findings
- Host selects an autosave slot, starts level 7 fresh, then triggers auto-restore from `coopsave.mg#`
- Client starts level 7 fresh, receives `MULTI_RESTORE_GAME`, and exits before calling `multi_restore_game`
- The client remains on the fresh level state while the host has loaded the save state
- The peer-side autosave skip appears in both `multi_do_restore_game` and `multi_restore_game`
- The resync fallback is not equivalent to applying the saved game state
