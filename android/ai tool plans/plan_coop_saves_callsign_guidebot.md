# Coop Saves, Callsign Display, and Guidebot Smoothing

Six interrelated issues with coop multiplayer.

## Issue 1: Score list shows truncated callsigns ("player" instead of "player42")

### Root cause
`hud_show_kill_list()` in gauges.c pixel-truncates callsigns to fit a narrow column.
In coop mode, `x1 = FSPACX(31)` giving only ~30 scaled pixels for the name -- about 5-6
characters. The while loop chops trailing characters until the string fits.

### Files
- d2/main/gauges.c line ~2570: `x1 = FSPACX(31)` for coop mode
- d1/main/gauges.c line ~2281: same

### Fix
Widen the coop name column. The score column to the right uses `%-6d` (6 digits).
In coop, there's no kill/death/efficiency column -- just name + score. Increase
`x1` from `FSPACX(31)` to `FSPACX(43)` for coop (matching the non-coop default),
or even wider since there's no kill ratio column. The score rendering at x1 can
shift right to compensate. Both left-column and right-column cases need adjustment.

### Complexity: Small (d1+d2 gauges.c only)

---

## Issue 2: Autosaves listed as "1p" and load does nothing

### Root cause (1p label)
`coop_autosave()` fires from the player-disconnect handler in multi.c. By the time
it runs, the disconnected player is already marked not-CONNECT_PLAYING, so
`n_connected` counts only the remaining host = 1. Description says "1p".

### Root cause (load does nothing)
Multiple silent-fail gates in the load path:
1. `state_restore_all_sub` at state.c:1295 -- callsign mismatch (random callsigns
   change between sessions) -> returns 0 silently
2. `multi_restore_game` at multi.c:6850 -- `state_game_id` mismatch (new coop
   session gets a new random ID) -> shows messagebox but user may not see it
3. `state_get_game_id` at state.c:1830 -- also checks callsign, returns 0 if
   different callsign

The `state_game_id` is generated from timer + player callsigns in
`multi_initiate_save_game()`. A fresh coop session has a different ID than the one
burned into the save file. There's no bypass.

### Files
- d2/main/coop_save.c:254 -- description format, n_connected count
- d2/main/multi.c:2891 -- autosave trigger point
- d2/main/state.c:1289-1300 -- callsign check on restore
- d2/main/state.c:1829-1835 -- state_get_game_id callsign check
- d2/main/multi.c:6830-6856 -- multi_restore_game game_id validation
- d1/ equivalents of all above

### Fix

#### 2a: Fix "1p" description
Count players BEFORE the disconnect updates the connected state. Options:
- Pass the pre-disconnect count into coop_autosave()
- Count from N_players instead of connected status (all slots 0..N_players were
  playing before the disconnect)
- Simplest: use `N_players` as the player count in the description since at
  autosave time all allocated slots were recently active

#### 2b: Fix silent load failures
For Android coop autosaves, the `state_game_id` and callsign checks need to be
relaxed. The autosave is meant to be loadable across sessions.

Option A (recommended): When saving an autosave, write a well-known `state_game_id`
(e.g. 0x434F4F50 = "COOP") instead of the session-specific one. On restore, accept
this sentinel value regardless of session. This avoids touching the general save/load
validation -- only the autosave write path changes.

Option B: Skip `state_game_id` validation entirely for #ifdef __ANDROID__ coop saves.
Simpler but less surgical.

For the callsign mismatch: the autosave file is named after the host's callsign
(`CALLSIGN.mg5`). On restore, `state_get_game_id` and `state_restore_all_sub` read
the saved callsign and compare to `Players[Player_num].callsign`. Since android
callsigns are random per session, this will never match.

Fix: In `coop_autosave()`, save a stable identifier as the "callsign" in the file
header. Options:
- Use a fixed string like "coopsave" for autosave files
- Use the client_id (truncated to CALLSIGN_LEN) as the callsign in the header
- On restore, match by client_id from the coop metadata trailer instead of callsign

Simplest approach: Make the autosave filename use a fixed prefix (e.g. "coop_auto")
instead of the player's callsign, and write that same fixed string as the callsign
in the save header. Then the restore path checks for this fixed string. This avoids
any callsign matching issues.

Actually, cleanest approach: use the existing `coop_restore_slot.txt` mechanism.
The Kotlin launcher already writes the slot. The C code reads it via
`coop_arm_auto_restore()`. The auto-restore path bypasses the in-game load menu
entirely. So the in-game load menu issue is somewhat separate from the lobby flow.

For the in-game load menu specifically:
- Skip `state_game_id` check for autosave slots (5-9) under `__ANDROID__`
- Relax callsign check: match either exact callsign or "any autosave by this device"
  using the coop metadata trailer's client_id

### Complexity: Medium (state.c, multi.c, coop_save.c changes in d1+d2)

---

## Issue 3: Show saves in coop launcher lobby

### Current state
This is MOSTLY DONE. The infrastructure exists:
- `readCoopAutosaveHistory()` in MultiplayerScreen.kt reads history JSON
- `CoopSaveOffer()` in LobbyScreen.kt shows saves in online lobby
- `LanCoopSaveOffer()` in LanDiscoveryTab.kt shows saves in LAN lobby
- `writeCoopRestoreSlot()` writes slot for C engine
- C reads `coop_restore_slot.txt` via `coop_arm_auto_restore()`

### What's missing
The display format requested is: "N players, level M, mm:ss in level"
Currently displaying: "L5 - 2p - Player1, Player2 - 5 min ago"

The "mm:ss in level" requires knowing the in-level play time. Currently:
- `GameTime64` is stored in the save header but always written as 0 (reset)
- `ThisLevelTime` exists in the engine but is NOT written to the save file
- `wall_clock_timestamp` gives the wall-clock time of the save

Options for "mm:ss in level":
- Start writing `ThisLevelTime` or `GameTime64` (non-zero) into the save header
  or the coop metadata trailer
- Add a `level_time` field to the coop metadata trailer and the history JSON
- Use wall_clock_timestamp as "time of save" (already available as "5 min ago")

### Fix
- Add `level_time_seconds` to coop_save_metadata and to coop_autosave_history.json
  entries. Compute from `GameTime64` at save time (GameTime64 is in fix64; divide
  by F1_0 to get seconds)
- Update Kotlin display to show "Np, level M, mm:ss played" from the JSON
- Sort by timestamp descending (already done)
- The existing CoopSaveOffer/LanCoopSaveOffer composables need minor UI text changes

### Complexity: Small-Medium (coop_save.c metadata + Kotlin display format)

---

## Issue 4: Create autosaves at intervals (every 10-30 seconds)

### Current state
Autosave only fires on player disconnect (multi.c:2891). No periodic timer.

### Fix
Add a periodic autosave timer in the game loop. Best location: end of
`GameProcessFrame()` in d2/main/game.c (and d1 equivalent).

```c
#ifdef __ANDROID__
static fix64 last_coop_autosave_time = 0;
#define COOP_AUTOSAVE_INTERVAL (F1_0 * 30)  /* 30 seconds */

if ((Game_mode & GM_MULTI_COOP) && multi_i_am_master() &&
    GameTime64 > last_coop_autosave_time + COOP_AUTOSAVE_INTERVAL) {
    last_coop_autosave_time = GameTime64;
    coop_autosave();
}
#endif
```

Notes:
- Only the host saves (multi_i_am_master check)
- GameTime64 resets per level, so this auto-fires early in each new level too
- The 5-slot rotation (slots 5-9) gives ~2.5 minutes of history at 30s intervals
- Don't autosave during endlevel or control center destroyed (already guarded
  inside coop_autosave)
- Consider: should we also autosave on level completion? Currently only on disconnect.
  Level completion would be a natural checkpoint. Add a call in
  `multi_endlevel_score` or wherever levels complete.

Also fix the disconnect-triggered autosave to pass the correct player count
(issue 2a).

### Files
- d2/main/game.c: GameProcessFrame() -- add timer check
- d1/main/game.c: same
- d2/main/coop_save.c: consider resetting timer on level start
- d2/main/coop_save.h: export interval constant

### Complexity: Small

---

## Issue 5: Coop save loads fail with player count mismatch

### Root cause
The existing restore code in `state_restore_all_sub` reads exactly N_players
player structs from the save file (the count is embedded). If the current session
has a different number of players, the game either:
- Reads too few/many players and corrupts state
- Fails silently at one of the validation gates (game_id, callsign)

The coop metadata trailer has `num_active_players` and `num_absent_players` with
full player records. The idea is:
1. Load the save regardless of current player count
2. Match saved players to current players by client_id or callsign
3. Unmatched current players get placed at level start position with default loadout
4. Unmatched saved players get stored as absent (their progress preserved for
   future rejoin)

### Files
- d2/main/state.c: state_restore_all_sub -- player struct loading section
  (around line 1600-1700 where player_rw structs are read)
- d2/main/multi.c: multi_restore_game -- the orchestration function
- d2/main/coop_save.c: coop_find_player_in_metadata, coop_read_save_metadata
- d1/ equivalents

### Fix approach
This is the most complex change. The current save format writes `N_players`
player_rw structs sequentially. On restore, it reads exactly that many.

For flexible restore:
1. Read all player_rw structs from the save (as stored, original count)
2. Read the coop metadata trailer
3. Map saved player slots to current session slots using
   `coop_find_player_in_metadata` (client_id then callsign)
4. For matched players: restore their progress (score, weapons, energy, etc.)
   from the saved player_rw or the coop_player_record
5. For unmatched current players: initialize at level start with default loadout
6. For unmatched saved players: store in absent list for future rejoin

The tricky part is that `state_restore_all_sub` restores a LOT of state beyond
just players -- objects, walls, triggers, AI state, etc. The player count mismatch
affects:
- Object array (player ships are objects)
- Multi player arrays
- Network state

A simpler approach: always restore the full state as saved (including its N_players),
then add/remove player slots afterward. Extra players get spawned at the start
position. Missing players get their objects removed.

### Complexity: Large (touches save/restore core logic)

---

## Issue 6: Guidebot still bands when changing direction

### Background
The velocity contamination fix (swapping extract_shortpos/set_thrust_from_velocity
order) was applied. The improvement helps with steady-state movement but direction
changes still exhibit visible snapping because:
1. No position interpolation -- all robots hard-snap via extract_shortpos every ~100ms
2. Escort gets same ~10Hz update rate as other robots (lower if sharing slots)
3. Escort AI rebuilds path every 5 seconds with abrupt direction changes
4. `move_towards_vector` halves velocity when direction mismatch > 41deg

### Options (in order of increasing complexity)

#### 6a: Escort priority in round-robin (LOW complexity)
In `multi_send_robot_frame()`, always process the companion slot first. This
guarantees the escort gets 10Hz updates even when other robots compete for slots.

#### 6b: AI damping on direction changes (MEDIUM complexity)
In `do_escort_frame()` or in the escort's `move_towards_vector` calls, add
acceleration ramping for the first few frames after a goal/path change. Instead
of immediately snapping velocity to the new direction, blend over 5-10 frames.
This reduces the dead-reckoning divergence that causes visible snapping on clients.

Concrete approach: add a `goal_change_timer` to the escort AI. When a new path
is created, set the timer. While the timer is active, scale the acceleration in
`move_towards_vector` by a ramp factor (e.g. 0.2 -> 1.0 over 10 frames). This
makes the host's escort accelerate gradually, which the client can dead-reckon
much better.

#### 6c: Client-side position interpolation (MEDIUM-HIGH complexity)
Add per-robot interpolation state in `multi_do_robot_position()`. Instead of
hard-snapping via extract_shortpos, store the target position and blend toward
it over the next ~100ms (until the next update). Based on the observer mode
interpolation code in gamecntl.c.

This would help ALL robots, not just the escort, but requires per-object state
tracking and changes to the object update path.

#### 6d: Higher packet rate for escort (LOW complexity, limited effect)
Force `robot_send_pending[slot] = 2` for the companion robot, giving it priority
in the round-robin. Combined with 6a, could push effective rate to 10Hz guaranteed.
Diminishing returns above that.

### Recommended approach: 6a + 6b
- Priority send (6a) guarantees consistent 10Hz
- AI damping (6b) reduces the severity of direction changes at the source
- Together they should significantly reduce visible banding without the complexity
  of client-side interpolation

### Files
- d2/main/multibot.c: multi_send_robot_frame -- companion priority
- d2/main/escort.c: do_escort_frame -- damping timer
- d2/main/ai2.c: move_towards_vector -- could be modified but prefer escort.c
- d1/ has no escort, so d1 changes are not needed

### Complexity: Medium

---

## Phased execution plan

### Phase 1: Callsign display fix (Issue 1)
- [x] Widen coop kill list name column in d2/main/gauges.c
- [x] Mirror in d1/main/gauges.c
- [x] Build, verify visually

### Phase 2: Periodic autosaves (Issue 4) + fix 1p label (Issue 2a)
- [x] Add periodic autosave timer in d2/main/game.c GameProcessFrame
- [x] Fix n_connected count in coop_autosave (use N_players)
- [x] Add level_time_seconds to history JSON
- [x] Mirror in d1
- [x] Build

### Phase 3: Fix autosave load failures (Issue 2b)
- [x] Use sentinel game_id (COOP_AUTOSAVE_GAME_ID) and stable callsign (COOP_AUTOSAVE_CALLSIGN)
- [x] Relax state_get_game_id and state_restore_all_sub callsign checks under __ANDROID__
- [x] Add autosave filename fallback in state_get_savegame_filename
- [x] Handle missing file gracefully on peers in multi_restore_game
- [x] Mirror in d1
- [x] Build

### Phase 4: Lobby save display improvements (Issue 3)
- [x] Add levelTimeSeconds to CoopSaveEntry
- [x] Update display format to "Np, level M, mm:ss played - callsigns - time ago"
- [x] Mirror in LanCoopSaveOffer
- [x] Build

### Phase 5: Player count mismatch tolerance (Issue 5)
- [x] Match CONNECT_WAITING players in callsign loop (host sets peers to CONNECT_WAITING)
- [x] Re-derive Netgame.numplayers/numconnected from live state after restore
- [x] Don't disconnect extra players not in save on Android
- [x] Mirror in d1
- [x] Build

### Phase 6: Guidebot smoothing (Issue 6)
- [x] Companion priority in multi_send_robot_frame (d2 only)
- [x] Velocity blending in ai_path_set_orient_and_vel for companion
- [x] Build

### Phase 7: Integration test + code quality
- [x] Run run-code-quality.ps1 --fix
- [x] Final build (BUILD SUCCESSFUL)
