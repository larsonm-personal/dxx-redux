# Coop Save Restore Repeat-Fire Bug Fix

## Symptoms (from logs 2026-04-11)
1. Host selects a coop save from lobby, game starts, auto-restore fires **every frame for 300 frames** then times out
2. Host shows "A multi-save game was restored that you are missing..." dialog that is non-dismissable
3. Both players eventually time out and disconnect

## Root Causes

### Bug 1: multi_restore_game uses wrong save file
- `coop_arm_auto_restore()` correctly falls back from `Player32.mg6` (game_id=0) to `coopsave.mg6` (game_id=COOP_AUTOSAVE_GAME_ID) when the regular file has the wrong game_id
- But `multi_restore_game()` independently rebuilds the filename from `Players[Player_num].callsign` and only falls back to `coopsave.mg6` if the callsign file **doesn't exist**
- When an old `Player32.mg6` EXISTS in slot 6 but has a different game_id, `multi_restore_game` uses it anyway -- the game_id check fails and shows the "you must rejoin" error dialog
- Same issue on peer side with `Player68.mg6`

### Bug 2: auto-restore doesn't disarm before calling restore
- `coop_try_auto_restore()` calls `multi_send_restore_game()` + `multi_restore_game()` BEFORE reaching the `disarm:` label
- If `nm_messagebox` opens a modal dialog whose event loop re-enters game frame processing, or if `state_restore_all_sub` runs game frames during level load, `coop_try_auto_restore` is called again with `armed=1`
- Result: 300 consecutive restore attempts (until timeout), each sending MULTI_RESTORE_GAME to peers

## Fixes

### Fix 1: Improve file fallback in multi_restore_game (d2/main/multi.c, d1/main/multi.c)
- After building filename from callsign, also check if the file's game_id matches
- If the callsign file exists but has wrong game_id AND slot is an autosave slot, fall back to coopsave.mg6
- Mirror the same logic that coop_arm_auto_restore uses

### Fix 2: Disarm before calling restore (d2/main/coop_save.c, d1/main/coop_save.c)
- Set `coop_auto_restore_armed = 0` before calling `multi_send_restore_game` / `multi_restore_game`
- Use `return` after the calls (the `disarm:` label is still there for error paths)

## Files Changed
- [x] d2/main/multi.c - multi_restore_game fallback improvement
- [x] d2/main/coop_save.c - disarm before restore
- [x] d1/main/multi.c - multi_restore_game fallback improvement
- [x] d1/main/coop_save.c - disarm before restore
