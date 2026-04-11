# Coop Inventory Preservation & Lobby Restore Plan

## Goal

Two-track system for coop game continuity:

**Track A -- Full Save Restore**: Load a complete autosave (level state, robots, player positions, inventory). The lobby shows available saves; the host picks one; the engine loads it after players join. This is the "resume exactly where we left off" path.

**Track B -- Level + Inventory Restore**: Start at the most recently completed level with fresh level state, but restore each player's inventory/shields/energy from cached data. This is the "continue from checkpoint" path -- simpler, always works even without a save file, and can be beefed up over time.

Both tracks should preserve player inventory across:
1. Sudden disconnects (including host migration)
2. Rejoins within the same session
3. Save/load cycles across sessions
4. Level transitions where a player is absent

---

## Critical Bug: Lobby Save Restore is Broken

### Root cause: callsign mismatch in `coop_arm_auto_restore()`

Autosaves are written with `COOP_AUTOSAVE_CALLSIGN` ("coopsave") as the filename prefix:
- File saved as: `coopsave.mg5`, `coopsave.mg6`, etc.

But `coop_arm_auto_restore()` looks for the file using `Players[Player_num].callsign`:
- File looked up as: `<actual_callsign>.mg5` -- **does not exist**

`state_get_game_id()` returns 0, restore silently fails, `coop_auto_restore_armed` never set.

**Location**: `d2/main/coop_save.c` lines 556-563, `d1/main/coop_save.c` same.

`multi_restore_game()` (d2/main/multi.c ~L6781) has an Android fallback that tries `COOP_AUTOSAVE_CALLSIGN` when the normal filename is missing. But `coop_arm_auto_restore()` **lacks this fallback** -- the restore never gets armed in the first place.

### Secondary issues
- **One-shot gate**: `coop_auto_restore_attempted = 1` is set on first call. If the file read fails for any reason, there is no retry -- the flag prevents `coop_arm_auto_restore()` from ever running again
- **Silent failure**: No HUD message or user-visible indication that restore failed. Only a `con_printf` log

### Fix needed (Chunk 0)
Add the same `COOP_AUTOSAVE_CALLSIGN` fallback to `coop_arm_auto_restore()` that already exists in `multi_restore_game()`. Both d1 and d2.

---

## Current State (what exists)

### Already working (when the bug above is fixed)
- **Absent player tracking**: `coop_track_absent_player()` snapshots a disconnecting player's full inventory into `coop_absent_list[]` (up to 16 players). Called from `multi_disconnect_player()` on Android
- **Save file metadata trailer**: `coop_save_metadata` includes both `active_players[8]` and `absent_players[16]` arrays of `coop_player_record`, each storing callsign, client_id, score, energy, shields, laser_level, weapon flags, ammo, player flags
- **Autosave system**: Rotating slots 5-9, triggered on player disconnect. Writes `coop_autosave_history.json` for lobby consumption
- **Auto-restore from lobby (Track A plumbing)**: Kotlin lobby reads history, host picks a save, writes `coop_restore_slot.txt`, engine loads via `coop_try_auto_restore()` -> `multi_restore_game()` -> `state_restore_all_sub()`. **Broken due to callsign bug**
- **Progress tracking (Track B plumbing)**: `coop_write_progress_json()` writes `coop_progress.json` with `last_completed_level` at end of each coop level. Kotlin reads it via `readCoopProgress()` in CreateGameDialog, auto-sets starting level to `last_completed_level + 1`
- **MULTI_SHIP_STATUS packets**: 43-byte packets sent 6x/sec in coop, broadcast to all peers. Contains: laser_level, player flags, primary_weapon_flags, primary_ammo[1], all secondary_ammo[0-9], secondary_weapon_flags, energy, afterburner_charge. Missing: shields, primary_ammo[0,2-9], score
- **Host migration**: Android-only, coop-only. Elects lowest-numbered connected player as new master
- **Lobby save UI (CreateGameDialog)**: Lists autosaves filtered by mission + client_id. Host can pick a save or "Start fresh". When a save is selected, `writeCoopRestoreSlot()` writes the slot number. When no saves exist, falls back to `coop_progress.json` level suggestion
- **Lobby save UI (LobbyScreen CoopSaveOffer)**: In the lobby waiting room, shows the best-matching save and Restore/Start Fresh toggle. Also writes `coop_restore_slot.txt`

### Gaps
1. **Save restore is broken** (callsign mismatch bug above)
2. **No inventory restore on rejoin (Track B)**: When a player reconnects mid-session, they get `init_player_stats_new_ship()` defaults. The `coop_absent_list[]` data is never applied to a rejoining player
3. **MULTI_SHIP_STATUS missing shields**: No periodic full-shields sync. Other players only have a "best guess" shields value
4. **MULTI_SHIP_STATUS missing most primary ammo**: Only `primary_ammo[1]` (Vulcan) is sent
5. **No periodic autosave timer**: Autosaves only trigger on disconnect events. If the host crashes 10 minutes into a level, the last save is from level start
6. **Track B has no inventory restore component**: `coop_progress.json` only stores the level number. When the game starts at the resumed level, players get default loadout. The progress track needs to be augmented with per-player inventory, OR the absent player cache needs to survive across sessions
7. **Game stats not preserved**: `coop_player_record` stores only inventory/loadout. The `player` struct also tracks `net_kills_total`, `net_killed_total`, `num_kills_total`, `hostages_rescued_total`, `time_total`, `hours_total` -- none of these survive disconnect/rejoin or are included in save metadata. Per-level session stats (`Coop_kill_stats`) are also lost
8. **Lobby doesn't distinguish save types**: Both full autosaves (Track A) and level-start checkpoints (Track B) could be offered, but the UI doesn't label which type it is. A full save restores robot/door/level state; a checkpoint just sets the start level + inventory
9. **Lobby doesn't show player match count**: The `CoopSaveOffer` and `CreateGameDialog` save lists show callsigns from the save, but don't highlight how many current lobby players match (e.g. "2/3 players match")

---

## Key Files

### C Engine (d2/) -- all changes mirrored to d1/
| File | Role |
|------|------|
| `d2/main/coop_save.c` | Absent player tracking, save metadata, autosave, auto-restore, progress tracking |
| `d2/main/coop_save.h` | Structures: `coop_player_record`, `coop_save_metadata`, function declarations |
| `d2/main/multi.c` | Disconnect handler, ship status packets, host migration, restore game, `multi_do_frame()` |
| `d2/main/multi.h` | Packet type constants and sizes |
| `d2/main/net_udp.c` | Player join/rejoin sync, `net_udp_welcome_player`, `net_udp_read_sync_packet` |
| `d2/main/gameseq.c` | `init_player_stats_new_ship()`, `coop_write_progress_json()` callsite |
| `d2/main/state.c` | `state_restore_all_sub()` - coop restore callsign matching |

### Kotlin (lobby UI)
| File | Role |
|------|------|
| `android/.../multiplayer/LobbyScreen.kt` | `CoopSaveOffer` widget (Track A in-lobby), save selection toggle |
| `android/.../multiplayer/MultiplayerScreen.kt` | `readCoopAutosaveHistory()`, `writeCoopRestoreSlot()`, `readCoopProgress()` |
| `android/.../multiplayer/CreateGameDialog.kt` | Game creation dialog with save list + progress-based level suggestion |

---

## Work Chunks

### Chunk 0: Fix save restore bug (CRITICAL, do first) -- DONE

**Problem**: `coop_arm_auto_restore()` uses `Players[Player_num].callsign` to build the save filename, but autosaves use `COOP_AUTOSAVE_CALLSIGN` ("coopsave"). The file is never found, restore never arms.

**Fix**: Add `COOP_AUTOSAVE_CALLSIGN` fallback to `coop_arm_auto_restore()`, matching the pattern already in `multi_restore_game()`.

**Files**: `d2/main/coop_save.c` (~L556), `d1/main/coop_save.c` (same)

**Functions to modify**:
- `coop_arm_auto_restore()` -- after the initial `state_get_game_id()` fails, try again with `COOP_AUTOSAVE_CALLSIGN`. Accept `COOP_AUTOSAVE_GAME_ID` sentinel as valid game_id

**Testing**: Create coop lobby, select a save in CreateGameDialog, start game. Verify save is loaded (player positions/inventory match the save, not fresh start). Check console log for "auto-restore armed" and "triggering auto-restore" messages.

---

### Chunk 1: Enhance MULTI_SHIP_STATUS for full inventory sync -- DONE

**Problem**: Other players don't have an accurate copy of each player's full inventory. Shields and most primary ammo are missing from the periodic sync. This matters for Track B -- the host needs accurate data to cache/restore.

**Changes**:
- `multi_send_ship_status_for_frame()`: Add shields (4 bytes), score (4 bytes), all primary_ammo[0-9] (20 bytes). Current 43 bytes -> ~71 bytes
- `multi_do_ship_status()`: Update unpacking in both observer and coop branches. In coop mode, write ALL received fields into `Players[pnum]` (not just the partial subset). This ensures `coop_snapshot_player()` gets accurate data when a player disconnects
- `multi.h`: Update `MULTI_SHIP_STATUS` size constant

**Files**: `d2/main/multi.c`, `d2/main/multi.h`, `d1/main/multi.c`, `d1/main/multi.h`

**Risk**: Writing shields from the packet into `Players[pnum].shields` could conflict with the authoritative damage/repair packet system. Mitigated by the fact that in coop, the host already tracks shields via damage packets -- this just gives a periodic ground-truth correction. In practice, the ship status shields value will always be slightly behind the damage packets, but for the purpose of caching-for-restore, "slightly stale" is much better than "unknown"

**Testing**: Join 2-player coop, pick up weapons, verify buddy overlay shows correct shields/energy. Verify with introspection API.

---

### Chunk 1b: Preserve game stats in coop_player_record -- DONE

**Problem**: The `player` struct tracks cumulative game stats (kills, deaths, hostages, time played) but `coop_player_record` only stores inventory/loadout. When a player disconnects and rejoins, or when restoring from a save, these stats are lost.

**Stats to preserve** (from the `player` struct):
- `net_kills_total` (short) -- total net kills across all levels
- `net_killed_total` (short) -- total times killed
- `num_kills_total` (short) -- total robots killed
- `hostages_rescued_total` (ushort) -- total hostages rescued
- `time_total` (fix) -- total game time
- `hours_total` (sbyte) -- hours overflow for time_total

Per-level stats (`num_kills_level`, `time_level`, `hostages_level`) reset naturally on level transition and don't need to be preserved across sessions.

Session stats (`Coop_kill_stats[pnum].robots_killed`, `.score_earned`) are per-level and already shared via `MULTI_COOP_PEER_STATUS`. These could optionally be cached but are lower priority.

**Changes**:
- `coop_save.h` (d1+d2): Add stat fields to `coop_player_record`. Bump `COOP_SAVE_META_VER` to 3. New fields:
  ```c
  int16_t  net_kills_total;
  int16_t  net_killed_total;
  int16_t  num_kills_total;
  uint16_t hostages_rescued_total;
  fix      time_total;
  int8_t   hours_total;
  ```
- `coop_save.c` (d1+d2): Update `coop_snapshot_player()` to copy these from `Players[pnum]`. Update metadata read/write to handle v2 (old, no stats) and v3 (with stats) gracefully
- `multi.c` (d1+d2): In `coop_do_restore_inventory()` (Chunk 2), also apply the stat fields. In the MULTI_SHIP_STATUS coop branch, optionally cache stats too (or accept that stats are only accurate at snapshot time)

**Consideration**: Adding ~11 bytes to `coop_player_record` changes the binary trailer layout. Must bump `COOP_SAVE_META_VER` and handle old saves that lack these fields (default to 0). The existing trailer versioning already supports this

**Testing**: Player 2 kills some robots, disconnects, reconnects. Verify kill count is preserved. Save and reload -- verify stats survive.

---

### Chunk 2: Restore inventory on mid-session rejoin -- DONE

**Problem**: When a player rejoins mid-session, they get default ship stats. The host has their cached inventory in `coop_absent_list[]` but never sends it back.

**Design**: New packet `MULTI_COOP_RESTORE_INVENTORY` sent from host to the rejoining player after the sync flow completes. Client applies it to overwrite defaults.

**Changes**:

1. `multi.h` (d1+d2): New packet type `MULTI_COOP_RESTORE_INVENTORY` (~75 bytes)
   - Byte 0: packet type
   - Byte 1: target pnum
   - Remaining: energy (4), shields (4), score (4), laser_level (1), primary_weapon_flags (2), secondary_weapon_flags (2), primary_ammo[10] (20), secondary_ammo[10] (20), flags (4), level_num_when_cached (2)

2. `coop_save.c/h` (d1+d2): New `coop_find_absent_player(callsign, client_id)` returns `coop_player_record*` or NULL

3. `multi.c` (d1+d2):
   - `coop_send_restore_inventory(int pnum)`: look up in absent list, pack and send reliable to `pnum`
   - `coop_do_restore_inventory(const ubyte *buf)`: unpack, apply to `Players[Player_num]`, update object shields. Skip key flags if `level_num_when_cached != Current_level_num`. Skip CLOAKED/INVULNERABLE flags always
   - Add dispatch case in `multi_process_data()`

4. `net_udp.c` (d1+d2): After `net_udp_send_rejoin_sync()` or at the end of the welcome flow, call `coop_send_restore_inventory(pnum)` for the joining player (host only, coop only)

**Timing**: The restore packet is sent after sync, processed after `init_player_stats_new_ship()` runs on the client. It overwrites the defaults cleanly.

**Testing**: Player 2 gets weapons -> disconnects -> reconnects. Verify same weapons/shields/energy.

---

### Chunk 3: Preserve absent player data across save/load cycles -- DONE

**Problem**: The `coop_absent_list[]` is in-memory only. If the game is restarted from a save, absent player records are lost.

**Changes**:
- `coop_save.c/h` (d1+d2): Add `coop_load_absent_from_metadata(const coop_save_metadata *meta)` to repopulate `coop_absent_list[]` from save metadata
- `state.c` (d1+d2): After `state_restore_all_sub()` completes, read the save's metadata trailer and call `coop_load_absent_from_metadata()`
- Verify `coop_clear_absent_players()` is NOT called during level transitions (check all call sites)

**Scenario**: Host + P2 on level 3. P2 disconnects. Host continues to level 5. Autosave includes P2's absent record. Game restarts from save. P2 reconnects -> host finds P2 in loaded absent list -> sends restore packet -> P2 has level-3 inventory.

**Testing**: Disconnect P2, autosave, restart game from save, reconnect P2. Verify inventory preserved.

---

### Chunk 4: Periodic autosave timer -- DONE

**Problem**: Autosaves only trigger on disconnect. If the host plays for 20 minutes without anyone disconnecting, there's no recent save.

**Changes**:
- `multi.c` (d1+d2): In `multi_do_frame()`, add a periodic coop autosave (every 30 seconds, host only). Use `timer_query()` or a frame counter

**Files**: `d2/main/multi.c`, `d1/main/multi.c`

**Testing**: Play coop for 2 minutes, verify multiple autosaves in `coop_autosave_history.json`.

---

### Chunk 5: Enhance Track B -- progress.json with player inventory -- DONE

**Problem**: `coop_progress.json` only stores the level number. When resuming via Track B (start at last level, fresh level state), players get default loadout.

**Design options**:
- A) Extend `coop_progress.json` to include per-player `coop_player_record` data. Written at level end alongside the level number. On resume, lobby writes a "restore inventory" file that the engine reads at start
- B) Rely on the autosave system (Track A) for inventory and keep Track B as level-only. Users who want inventory back use Track A
- C) At level-end, snapshot all players into the absent list, so their state is available on the next session start

**Preferred: Option A** -- extend `coop_progress.json` to carry per-player inventory. This makes Track B fully functional without requiring a save file.

**Changes**:
- `coop_save.c` (d1+d2): In `coop_write_progress_json()`, add a `"players"` array with per-player inventory fields (shields, energy, laser_level, weapon flags, ammo, score, player flags). Reuse `coop_snapshot_player()` to gather the data
- `MultiplayerScreen.kt`: Extend `readCoopProgress()` to also return the player records. Or write a separate `coop_progress_players.json` sidecar
- On game start via Track B: write a file or pass data that tells the engine to apply saved inventory to matching players after level load

**Consideration**: This overlaps with Chunk 2 (rejoin restore). The mechanism to apply saved inventory on game start is the same -- the host looks up each player and sends a restore packet. The difference is the data source (progress file vs absent list vs autosave metadata).

**Testing**: Complete level 3 in coop. Exit. Start new game at level 4 via progress resume. Verify players have their level-3-end inventory.

---

### Chunk 6: Lobby UI improvements -- DONE

**Problem**: Autosaves are presented in the CreateGameDialog and LobbyScreen, but:
- There's no top-level "Resume Recent Game" shortcut
- The two UIs (CreateGameDialog save list and LobbyScreen CoopSaveOffer) are somewhat redundant
- CreateGameDialog already shows saves with good UX; LobbyScreen shows a simpler version

**Changes**:
- `MultiplayerScreen.kt`: Add a "Recent Coop Games" section on the multiplayer screen. Scan both d1/d2 dirs for `coop_autosave_history.json`. Show last 3-5 saves with mission, level, players, time. Tapping one pre-fills CreateGameDialog or goes straight to lobby creation
- Clean up the interaction between CreateGameDialog save selection and LobbyScreen CoopSaveOffer so they don't conflict (currently both can write `coop_restore_slot.txt`)

**No C engine changes needed.**

---

### Chunk 6b: Lobby metadata -- save type labels and player match count -- DONE

**Problem**: The lobby save lists don't tell the host:
1. Whether a save is a **full save** (Track A: restores level state, robot positions, door states, everything) vs a **level checkpoint** (Track B: starts at the right level with inventory, but fresh level state)
2. How many players currently in the lobby **match** the save's player list

This matters for making informed restore decisions.

**Design**:

#### Save type label
- Full saves come from `coop_autosave_history.json` (slots 5-9). These are actual `.mg` save files that `state_restore_all_sub()` will load
- Level checkpoints come from `coop_progress.json` (written at level end). These just set the starting level; no save file is loaded
- Both could appear in the same list. Tag each entry with a `type` field:
  - `"full_save"` -- from autosave history, restores everything
  - `"checkpoint"` -- from progress.json, level + inventory only
- Display in UI: prefix the label with a tag like `[Save]` or `[Checkpoint]`, or use an icon/color distinction

#### Player match count
- For autosave entries: compare `save.callsigns` (and/or `client_ids` if available) against the current lobby player list
- Show as e.g. `"2/3 players match"` or highlight matching names in the callsign list
- In CreateGameDialog: already scores saves by callsign match when sorting. Surface the match count in the display label
- In LobbyScreen CoopSaveOffer: the `bestMatch` scoring already counts matches. Show the count: `"Save found (2/3 players): L5, 3p, 4:32 played"`

**Changes**:

C engine:
- `coop_save.c` (d1+d2): Add a `"type": "full_save"` field to each entry in `coop_autosave_history.json`. This is always `"full_save"` for autosaves. The Kotlin side will tag progress entries as `"checkpoint"` when merging them into the same list
- `coop_save.c` (d1+d2): Add a `"total_score"` field to autosave history entries (sum of all connected players' scores at save time). This gives a quick indicator of game progress without needing to parse the binary save trailer
- `coop_save.c` (d1+d2): Extend `coop_write_progress_json()` to include per-player `client_ids` array (currently only has `players` callsign array). This allows the Kotlin lobby to match progress entries against lobby players the same way it matches autosave entries

Kotlin:
- `MultiplayerScreen.kt`: Extend `CoopSaveEntry` data class:
  ```kotlin
  data class CoopSaveEntry(
      val slot: Int,
      val level: Int,
      val timestamp: Long,
      val numPlayers: Int,
      val callsigns: List<String>,
      val clientIds: List<String> = emptyList(),
      val levelTimeSeconds: Int = 0,
      val type: String = "full_save",  // "full_save" or "checkpoint"
      val totalScore: Int = 0,
  )
  ```
- `MultiplayerScreen.kt`: In `readCoopAutosaveHistory()`, parse the new `type` and `total_score` fields, plus `client_ids`
- `MultiplayerScreen.kt`: New `readCoopProgressAsEntry()` function that reads `coop_progress.json` and returns a `CoopSaveEntry` with `type = "checkpoint"`, `slot = -1` (no save file). The lobby merges this into the save list
- `CreateGameDialog.kt`: In the save list display, show the save type and match count:
  ```
  [Save] L5 - 2p (2/2 match) - Alice, Bob - 3m ago - 54320 pts
  [Checkpoint] L6 - 2p (1/2 match) - Alice, Charlie - 1h ago
  ```
- `LobbyScreen.kt`: In `CoopSaveOffer`, include the save type and match count in the label. The `bestMatch` scoring already computes `matchCount` -- surface it: `"[Save] 2/3 match: L5, 3p, 4:32 played"`

**Consideration**: Progress entries (checkpoints) don't have an associated save slot. When a checkpoint is selected, `writeCoopRestoreSlot()` should write a sentinel (e.g. `slot = -1` or a separate file) that tells the C engine "start at this level but don't load a save file". The engine already handles this scenario: if no `coop_restore_slot.txt` exists, it just starts at the level specified in the lobby `GameLaunchInfo.levelNum`. So the checkpoint path doesn't need a restore slot at all -- it just needs the level number passed through `GameLaunchInfo`, which already happens via `levelNumText` in `CreateGameDialog`

**Testing**: Create a coop lobby with 2 players. Verify save list shows `[Save]` and `[Checkpoint]` labels. Verify match count is accurate (add/remove players from lobby, verify count updates). Verify checkpoints show `0 pts` and full saves show the total score.

---

## Considerations

### Protocol versioning
- MULTI_SHIP_STATUS size change and new MULTI_COOP_RESTORE_INVENTORY packet break compat with older builds. Fine pre-release

### Save file backward compatibility
- Existing trailer system is backward-compatible. No format changes needed -- `coop_player_record` already has all fields

### Host migration
- On migration, new host inherits `coop_absent_list[]` in memory. No transfer needed
- If game restarts from save on different host, absent list loaded from metadata trailer (Chunk 3)

### D1/D2 parity
- All C changes mirrored. `coop_save.h` already uses max-of-both layout (10 weapon slots)

### Player flags on restore
- Restore durable flags: QUAD, AFTERBURNER, MAP_ALL, CONVERTER, AMMO_RACK, HEADLIGHT
- Do NOT restore: CLOAKED, INVULNERABLE (time-limited), FLAG (CTF)
- Keys (BLUE/YELLOW/RED KEY in flags): level-specific, only restore if same level

### Score and game stats
- Score: already tracked in `coop_player_record.score`. Sent via `MULTI_SCORE` packets normally. Added to expanded MULTI_SHIP_STATUS for caching accuracy
- Kill/death/hostage stats: added to `coop_player_record` in Chunk 1b. These are cumulative totals from the `player` struct, not per-level deltas
- `total_score` in autosave history JSON: sum of all connected players' scores at save time. Quick progress indicator for the lobby UI without parsing binary trailers
- Per-level session stats (`Coop_kill_stats`): already synced via `MULTI_COOP_PEER_STATUS` packets (1/sec). Not saved to `coop_player_record` since they reset each level anyway

### Lobby metadata
- Save type labels: `"full_save"` vs `"checkpoint"`. Full saves restore complete level state via `state_restore_all_sub()`. Checkpoints simply set the start level + restore per-player inventory. The distinction is important for user expectations -- a full save puts you back mid-level with doors opened and robots killed, while a checkpoint starts the level fresh
- Player match count: computed by comparing save `callsigns`/`client_ids` against current lobby players. Shown as `"2/3 match"` in the save list. Already computed internally for scoring (LobbyScreen `bestMatch` logic) -- just needs to be surfaced in the display label
- Match scoring uses `client_ids` when available (UUID-based, survives callsign changes), falling back to case-insensitive callsign matching

---

## Execution Order

1. **Chunk 0** (fix restore bug) -- unblocks Track A immediately, small change
2. **Chunk 1** (ship status expansion) -- foundation for accurate caching
3. **Chunk 1b** (stats in coop_player_record) -- can be done with Chunk 1 or independently
4. **Chunk 2** (rejoin restore) -- core Track B deliverable for mid-session
5. **Chunk 3** (absent list persistence) -- makes Track B survive save/load
6. **Chunk 4** (periodic autosave) -- makes Track A more robust
7. **Chunk 5** (progress + inventory) -- makes Track B work across sessions
8. **Chunk 6** (lobby UI -- resume recent games) -- top-level shortcut
9. **Chunk 6b** (lobby metadata -- type labels + match count) -- polish, mostly Kotlin

Chunk 0 is a standalone bugfix, should be done first.
Chunks 1/1b are the data foundation.
Chunks 2-3 are the critical path for the inventory system.
Chunks 4-6 are improvements that can be done in any order.
Chunk 6b can be done anytime after Chunk 0 (it's mostly UI display logic).
