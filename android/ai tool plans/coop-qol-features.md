# Coop Quality of Life Features -- Plan

All features apply to both D1 and D2. All C-side changes are duplicated in d1/ and d2/.

## Decisions (resolved)

- **D1 + D2**: all features for both games - but attempt to de-duplicate new code, as usual
- **Warp policy**: prevent warping past locked doors (no key held by any player). Use BFS reachability. Stay on same side of doors as warp target in general
- **Engagement timer**: dealing damage to a robot OR being hit by a robot
- **Distance threshold**: fixed constant (tunable #define)
- **Session resume UX**: suggest both a save file and a starting level based on what's available. Actual save files take precedence over level-only checkpoints
- **Save file metadata**: store which players were in the game and match callsigns to inventories when restoring from lobby
- **Backward compatibility**: new MULTI_* packets will NOT be silently ignored by older clients (the dispatch default case calls `Int3()`, which is fatal on debug builds). However, version negotiation prevents mismatched clients from joining -- `MULTI_PROTO_VERSION` is checked at join time and mismatches are rejected with `UPID_VERSION_DENY`. So: bump `MULTI_PROTO_VERSION` when adding new packet types. Clients with our new features will only play with other clients that have them. This is acceptable since all builds come from the same APK anyway
- **Player persistence across levels**: the existing save format already stores all MAX_PLAYERS (8) slots with connected state. Disconnected players have `CONNECT_DISCONNECTED` and their objects become `OBJ_GHOST`. We can extend this to carry absent players' inventories forward across levels by keeping their player struct intact (not clearing it at level transition). On rejoin, match by callsign or client_id and restore their inventory. Limit carried-forward absent players to 16 via the metadata extension (separate from the in-game 8-player limit)
- **Player identity matching**: match on callsign OR unique client_id. The client_id is either a GPGS player_id (returned from server auth) or a persistent installation UUID stored in SharedPreferences on first launch. The `netplayer_info` struct will be extended with a `client_id` field. Save file metadata will store both callsign and client_id. On resume, try client_id match first (handles callsign changes), fall back to callsign match
- **Warp target cycling**: with >2 players, the warp button cycles through eligible targets. Each press of the button advances to the next valid target (sorted by player index, wrapping). Only players who are reachable (not behind locked doors) and distant enough are eligible targets

---

## Feature 1: Robot Kill Stats Overlay

### What it does
In-game overlay showing:
- Robots killed vs total: "30/130"
- Per-player % of robot score value earned

### Existing infrastructure
- `Players[pnum].num_robots_level` -- total robots at level start (set via `count_number_of_robots()` in gameseq.c)
- `count_number_of_robots()` -- counts alive OBJ_ROBOT objects
- `Robot_info[id].score_value` -- per-robot-type point value
- Kill attribution in multibot.c:895: `add_points_to_score(Robot_info[Objects[botnum].id].score_value)`
- `Players[pnum].score` -- individual player score in coop (synced via MULTI_SCORE)
- Android overlay: JNI bridge in `android_jni_overlay.c`, Kotlin overlays in `VideoInfoOverlay.kt`

### Design

**C side (d2/main, d1/main):**
- Add `coop_kill_stats[MAX_PLAYERS]` struct array: `{int robots_killed, int score_earned}`
- Compute `total_robot_score_value` at level start: iterate all OBJ_ROBOT, sum `Robot_info[obj.id].score_value`
- Hook in `multi_do_robot_explode()` -- the MULTI_ROBOT_EXPLODE packet includes killer objnum, resolve to player index, increment that player's counters on all clients
- Also hook single-player robot death in `collide.c` so the overlay works in solo coop testing
- Use `Players[pnum].score - Players[pnum].last_score` as the display score delta (already synced, close enough -- includes minor hostage bonus)
- Reset counters in `init_player_stats_level()`
- Expose via JNI: `nativeGetCoopRobotStats()` -> int array: [killed, total, total_score, p0_kills, p0_score, p1_kills, p1_score, ...]
- Note: `num_robots_level` grows during gameplay from matcen spawns (ai2.c:1804, fuelcen.c:327, multibot.c:1144). Use current value as denominator

**Kotlin side:**
- `CoopStatsOverlay` class, polls at ~1 Hz
- Renders kill count and per-player score %
- Only visible when `Game_mode & GM_MULTI_COOP`

**No new network packets needed** -- MULTI_ROBOT_EXPLODE already carries killer info, MULTI_SCORE syncs scores.

### Edge cases
- Matcen robots increase the total -- denominator must be live `num_robots_level`
- Boss robots may respawn
- Observer players excluded from stats
- Handle 0 total (no div by zero)

---

## Feature 2: Shared Teammate Overlay

### What it does
Shows teammate status: shields, energy (or, if primary is gatling, ammo), current secondary weapon + ammo count. Displayed alongside the robot stats overlay in coop.

### Design
- Data is already in `Players[]` array which is synced across clients
- Expose via JNI alongside coop stats: `nativeGetTeammateStatus()` -> per-player [shields, energy, secondary_weapon, secondary_ammo[weapon]]
- Kotlin overlay renders compact teammate bars
- Only meaningful fields: shields (fix -> %), energy (fix -> %), secondary weapon name, ammo count

### Edge cases
- Observer players excluded
- Disconnected players shown as offline/greyed
- Shield/energy can exceed 100% with powerups -- clamp display at 200%

---

## Feature 3: Coop Progress Auto-Save and Session Resume

### 3A: Save File Metadata Extension

**What it does:** Add metadata to multiplayer save files so the resume system can identify which players were in the game and match them to their inventories.

**Current save format:**
- Header: "DGSS", version, (coop: state_game_id + callsign), description, thumbnail, palette
- Body: between_levels, mission, level, gametime, player struct (all fields including energy/shields/weapons/ammo/score), objects, walls, triggers, etc.
- Coop saves already store all player structs (currently N_players worth)
- if there was a save with three players, then resumed with only two, the third player's inventory should be maintained in the save, and carried forward across levels too. limit to 16 players saved this way (discard the oldest). on resume, the third player may no longer have a location - they can spawn at the mine entrance as needed

**Metadata extension -- appended after existing save data:**
```c
#define COOP_SAVE_META_TAG  0x434F4F50  // "COOP"
#define COOP_SAVE_META_VER  1
#define COOP_MAX_REMEMBERED_PLAYERS  16  // absent players carried forward

struct coop_player_record {
    char     callsign[CALLSIGN_LEN+1];
    char     client_id[37];            // UUID string (36 chars + null), or empty
    int32_t  score;
    uint8_t  was_connected;            // 1 if playing when saved, 0 if absent/carried
    // full inventory snapshot for absent players:
    fix      energy;
    fix      shields;
    ubyte    laser_level;
    ushort   primary_weapon_flags;
    ushort   secondary_weapon_flags;
    ushort   primary_ammo[MAX_PRIMARY_WEAPONS];
    ushort   secondary_ammo[MAX_SECONDARY_WEAPONS];
    uint     flags;                    // keys, powerup flags
};

struct coop_save_metadata {
    uint32_t tag;                      // COOP_SAVE_META_TAG
    uint16_t version;                  // COOP_SAVE_META_VER
    uint32_t wall_clock_timestamp;     // Unix epoch seconds
    int16_t  level_num;
    char     mission_name[9];
    uint8_t  difficulty;
    uint8_t  num_active_players;       // players who were connected at save time
    uint8_t  num_absent_players;       // players carried forward from previous sessions
    struct coop_player_record active_players[MAX_PLAYERS];
    struct coop_player_record absent_players[COOP_MAX_REMEMBERED_PLAYERS];
};
```

- Written by `state_save_all_sub()` at end of save data
- Read by a new `state_read_coop_metadata()` that seeks to end, reads tag, validates
- Old save files without the tag are gracefully handled (no metadata available)
- The tag-based approach means old game versions can still read the save (they stop before the trailer)

**Player persistence across levels and sessions:**
- The save already stores all MAX_PLAYERS (8) player structs. The existing restore code maps them by callsign (`strcmp(Players[i].callsign, restore_players[j].callsign)`)
- The metadata extension adds an `absent_players[]` array for players who disconnected in previous levels but whose inventories should be preserved
- When a player disconnects, their full inventory is snapshotted into the absent list before their slot is freed
- When a save is written (auto or manual), both active and absent players are stored
- At level transitions, absent player records are carried forward
- Limit: 16 absent player records. If full, discard the oldest (by timestamp or FIFO order)
- When an absent player rejoins (matched by client_id or callsign), their inventory is restored and they're removed from the absent list
- Players absent at save time have no position -- on rejoin they spawn at the mine entrance (Player_init[0])

**Callsign + client_id matching:**
- On restore, try client_id match first (handles callsign changes between sessions)
- Fall back to callsign match if client_id is empty or not found
- The match function:
  ```c
  // Try client_id first, fall back to callsign
  int find_player_in_metadata(const char *callsign, const char *client_id,
                              const coop_save_metadata *meta) {
      // Check active players by client_id
      if (client_id[0]) {
          for (int i = 0; i < meta->num_active_players; i++)
              if (!strcmp(client_id, meta->active_players[i].client_id))
                  return i;
      }
      // Check absent players by client_id
      if (client_id[0]) {
          for (int i = 0; i < meta->num_absent_players; i++)
              if (!strcmp(client_id, meta->absent_players[i].client_id))
                  return MAX_PLAYERS + i;  // offset to indicate absent
      }
      // Fall back to callsign matching
      for (int i = 0; i < meta->num_active_players; i++)
          if (!d_stricmp(callsign, meta->active_players[i].callsign))
              return i;
      for (int i = 0; i < meta->num_absent_players; i++)
          if (!d_stricmp(callsign, meta->absent_players[i].callsign))
              return MAX_PLAYERS + i;
      return -1;  // new player
  }
  ```
- If not found in either list (new player), give them default starting inventory

**File: `coop_progress.json` in the player's save directory**
```json
{
  "sessions": [
    {
      "mission": "d2",
      "players": ["PlayerA", "PlayerB"],
      "last_completed_level": 5,
      "timestamp": 1712150400,
      "difficulty": 2
    }
  ]
}
```

- Written by C side at level-end in coop (hook in `DoEndLevelScoreGlitz()` or level transition)
- Player set = sorted list of callsigns (order-independent matching)
- Read by Kotlin lobby code when coop game is being set up

### 3C: Auto-Save on Player Disconnect

- Hook into `multi_do_quit()` / `multi_disconnect_player()` in multi.c
- Before cleanup, each remaining player auto-saves to slot 9 (`.mg9`)
- The disconnecting player's client also auto-saves before exit
- Bypass the "all players alive" and "host initiates" constraints -- this is an automatic save
- Save description: "Auto L5 2p 12345pts"
- Include the coop metadata extension (3A) so the save can be matched later

### 3D: "Last in Mine" Save

- Special case of 3C: when `count_connected_players() == 1` in coop
- Auto-save + HUD message "All other players left -- game saved"

### 3E: Session Resume from Lobby

**When a coop lobby forms:**
1. Host scans for `.mg9` (auto-save) and `coop_progress.json` for the selected mission
2. Each joining client sends their save file metadata to host (new `MULTI_COOP_SAVE_INFO` packet: level, timestamp, callsigns hash)
3. Host compares all available saves:
   - Saves with matching player sets (by callsign) are candidates
   - Pick the newest by wall_clock_timestamp
   - If a save file matches, suggest "Resume from Level 5 save? [Yes/No]"
   - If only a progress.json match (no save file), suggest "Start from Level 6? [Yes/No]"
   - Save file takes precedence over level-only suggestion
4. If Yes: host sets the starting level, and once all players are in-game, host triggers `MULTI_RESTORE_GAME` from the best save
5. Callsign-to-inventory matching (3A) ensures each player gets their correct loadout

**Network: 1 new packet type** (`MULTI_COOP_SAVE_INFO`) sent during lobby phase.

### Edge cases
- Race condition on simultaneous disconnect: each client saves independently, timestamps disambiguate
- New player joining who wasn't in original session: gets default inventory
- Callsign changes between sessions: client_id match handles this; callsign-only match is fallback
- Save file corruption: check tag/version, skip gracefully
- Absent player limit: 16 max carried forward; oldest discarded when full
- Absent player rejoins mid-level: inventory restored, spawns at mine entrance
- Absent player rejoins on a different level than when they left: their inventory carries but level state is current

---

## Feature 4: Warp to Player

### What it does
Teleport to a teammate in coop, with a popup button or menu option. Prevents warping past locked doors.

### Trigger conditions (all must be true)
1. `Game_mode & GM_MULTI_COOP`
2. >= 2 players connected
3. Euclidean distance to nearest teammate > `COOP_WARP_DISTANCE_THRESHOLD` (fixed constant, e.g. F1_0 * 200)
4. Local player hasn't dealt damage to a robot AND hasn't been hit by a robot for `COOP_WARP_ENGAGEMENT_TIMEOUT` seconds (e.g. 20s)
5. Local player is alive (not dead/respawning)
6. Cooldown timer expired (e.g. 60s after last warp)

### Engagement tracking
- New global: `fix64 last_robot_engagement_time`
- Incremented in two places:
  - `apply_damage_to_robot()` or equivalent in collide.c -- local player dealt damage
  - `apply_damage_to_player()` or equivalent -- local player took damage from a robot (check source is OBJ_ROBOT)
- Reset on level start

### Locked door constraint
- Use `create_bfs_list()` from escort.c to find all segments reachable from player's current segment without crossing locked doors
- `segment_is_reachable()` (escort.c:136) uses `ai_door_is_openable()` to check key requirements
- Before offering warp: BFS from local player's segment, check if target's segment is in the reachable set
- If not reachable (locked door between them): suppress the warp button, show "Locked door between you" tooltip if attempted from menu
- The BFS uses `wall.keys` (KEY_BLUE/RED/GOLD) and `WALL_DOOR_LOCKED` flag to determine passability
- Check against keys held by *any* player (coop keys are shared: `Players[pnum].flags & KEY_*`)
- Note: `ai_door_is_openable()` checks the local player's keys. For coop warp, check the union of all players' keys. May need a wrapper that tests keys from all connected players
- Performance: BFS over segments is fast (MAX_SEGMENTS ~900). Run at warp-check time (not every frame). Cache result and invalidate when a key is picked up or a door opens

### Warp target selection
- With 2 players: target is always the other player
- With >2 players: the warp button cycles through eligible targets on each press
  - Eligible = connected, alive, reachable (BFS), distant enough
  - Cycle order: ascending player index, wrapping around
  - Track `coop_warp_target_idx` -- current target in the cycle
  - Display changes to "Warp to [next callsign]" as target cycles
  - If current target becomes ineligible (dies, gets close, etc.), auto-advance to next eligible
  - If no targets eligible, hide the button

### Warp mechanics
1. Target = selected teammate (nearest by default, cycles with button presses if >2 players)
2. Target position = `Objects[Players[target_pnum].objnum].pos`
3. Spawn offset = random unit vector * `ConsoleObject->size * 4` (2 ship diameters)
4. Candidate = target_pos + offset
5. Validate: `find_point_seg(&candidate, target_segment) != -1`
6. Validate: no object intersection at candidate position (check against robots, players in segment)
7. Up to 30 attempts with different random directions
8. If all fail: HUD message "Warp failed -- no clear space near [callsign]"
9. On success:
   - Move ConsoleObject to new position + segment
   - `obj_relink(ConsoleObject - Objects, new_segment)`
   - Send `MULTI_WARP_TO_PLAYER` packet: `{warping_pnum, target_pnum, new_pos(12 bytes), new_segment(2 bytes)}`
   - All clients update the warping player's object position
   - Set cooldown timer

### UI
- **Popup button**: Android touch overlay, appears when conditions met. "Warp to [callsign]"
- **Menu entry**: in the F1/options popup menu, only for coop. Performs warp immediately
- Button disappears when: player engages robot, gets close enough, or cooldown active
- After respawn far from action: use shorter engagement timeout (e.g. 5s instead of 20s)

### Network
- 1 new packet: `MULTI_WARP_TO_PLAYER` (1 type + 1 warper + 1 target + 12 pos + 2 seg = 17 bytes)

### Edge cases
- Target moving: use position at time of warp execution, not button press
- Multiple players warp simultaneously: each gets own random offset, unlikely to collide
- Tiny segments: 30 retries should find something; if not, fail gracefully
- Target in secret area: if reachable by BFS, allow it. If behind a locked secret door, deny it
- >2 players: target cycling wraps around, skips ineligible targets
- All targets behind locked doors: hide warp button, don't allow from menu either

---

## Feature 5: End-of-Level Score Breakdown

### What it does
Enhanced end-of-level screen showing per-player robot kill contributions.

### Design
- Hook into `DoEndLevelScoreGlitz()` in gameseq.c
- Add rows showing each player: callsign, robots killed, score earned, % of total
- Data source: `coop_kill_stats[]` from Feature 1
- Pure display change, no networking needed
- Only in coop mode

---

## Feature 6: 3D Player Locator HUD (future)

### What it does
- 3D directional indicator on the HUD pointing toward teammates
- "Follow me" line rendered in 3D space between players
- Visible through walls as a compass/arrow indicator

### Design (deferred -- placeholder for planning)
- Render a small arrow/icon at the screen-space projection of teammate position
- If off-screen, render at screen edge pointing in the direction
- "Follow me" line: render a 3D line strip through the mine path (using AI pathfinding points)
- Requires 3D rendering hooks in gamerend.c
- The automap already renders player positions -- some of that code can be reused

---

## Implementation Phases

### Phase 1: Robot Kill Stats + Teammate Status Overlay
- [x] Add `coop_kill_stats[MAX_PLAYERS]` tracking in d2/main (multi.c, multi.h)
- [x] Add `coop_record_robot_kill()`, `coop_reset_kill_stats()`, `coop_compute_total_robot_score()`, `coop_killer_to_pnum()` in multi.c
- [x] Hook `multi_do_robot_explode()` for per-player attribution on all clients (multibot.c)
- [x] Hook 3 `add_points_to_score` calls in collide.c for local player kills
- [x] Hook `add_points_to_score` in fireball.c for explosion splash kills
- [x] Compute `Coop_total_robot_score` and reset stats in `init_player_stats_level()` (gameseq.c)
- [x] JNI export: `nativeGetCoopRobotStats()`, `nativeGetTeammateStatus()` (jni_main.c)
- [x] Kotlin `CoopStatsOverlay` class (auto-shows in coop, polls at 1Hz)
- [x] Integrate overlay in MainActivity.kt (creation, polling start/stop)
- [x] Duplicate all C hooks in d1/main (multi.h, multi.c, multibot.c, collide.c, fireball.c, gameseq.c)
- [x] Build passes (Android debug APK, both d1 and d2)
- [x] Test with 2 emulators in coop

### Phase 2: Client Identity + Save File Metadata Extension
- [x] Generate persistent installation UUID in SharedPreferences on first app launch (`ClientIdentity.kt`)
- [ ] Return GPGS player_id from server to client (or use installation UUID as fallback)
- [x] Extend `netplayer_info` with `client_id` field + bump `MULTI_PROTO_VERSION`
- [x] Pass client_id through join handshake (extend UPID_REQUEST/UPID_SYNC)
- [x] Fix `UPID_GAME_INFO_SIZE` buffer overflow: add (MAX_PLAYERS+4)*37 for client_id; bump `UPID_MAX_SIZE` 1024->2048
- [x] Define `coop_save_metadata` and `coop_player_record` structs
- [x] Write metadata trailer in `state_save_all_sub()`, including absent player records
- [x] Read metadata in new `state_read_coop_metadata()`, handle missing tag gracefully
- [x] Implement `find_player_in_metadata()` with client_id-first, callsign-fallback matching
- [ ] Callsign/client_id-to-inventory remapping in `multi_restore_game()`
- [x] Absent player tracking: snapshot inventory on disconnect, carry forward at level transition
- [ ] Absent player rejoin: restore inventory, spawn at mine entrance
- [x] Duplicate in d1
- [ ] Test: save with 2 players, restore with swapped join order -- verify correct inventories
- [ ] Test: player disconnects, reconnects next session -- verify inventory preserved

### Phase 3: Auto-Save on Disconnect / Last in Mine
- [x] Hook `multi_do_quit()` / disconnect path to trigger auto-save to slot 9
- [x] Bypass "all alive" / "host only" constraints for auto-save
- [x] Detect "last in mine" (`count_connected_players() == 1`) and save + notify
- [x] Include coop metadata in auto-saves
- [x] Duplicate in d1
- [ ] Test: player disconnect mid-level, verify save file created with correct metadata

### Phase 4: Level Completion Checkpoint + Session Resume
- [x] Write `coop_progress.json` at level-end in coop
- [x] Kotlin lobby: read coop_progress.json, auto-suggest resume level in CreateLobbyDialog
- [x] Write `coop_autosave_info.json` sidecar alongside auto-save
- [x] Kotlin lobby: read autosave info, show "Will restore save from Level X" hint
- [x] Auto-restore framework: `coop_arm_auto_restore()` / `coop_try_auto_restore()` / `coop_disarm_auto_restore()`
- [x] Hook auto-restore in `multi_do_frame()` and `multi_new_game()` (d1+d2)
- [x] ~~MULTI_COOP_SAVE_INFO packet~~ -- not needed; host matches locally from coop_autosave_history.json
- [x] Host-side logic: scan saves, match player sets by callsign, auto-select best match in LobbyScreen
- [x] On accept: host triggers MULTI_RESTORE_GAME with callsign remapping (already works via coop_restore_slot.txt -> coop_arm_auto_restore)
- [x] Added `game` field to lobby gameInfo so LobbyScreen knows d1/d2
- [x] CoopSaveOffer composable in LobbyScreen (auto-selects best match, host can toggle restore/fresh)
- [x] Duplicate in d1
- [x] Test: coop multiplayer test passes (ALL CHECKS PASSED)
- [ ] Test: full flow -- play 2 levels, disconnect, reconnect, verify resume suggestion

### Phase 5: Warp to Player
- [x] Add engagement tracking (`last_robot_engagement_time`) in collide.c (d1+d2)
- [x] Custom BFS reachability in `coop_warp.c` (works for both d1 and d2, d1 has no escort.c)
- [x] Coop key check (keys are shared in coop, local player flags already have all keys)
- [x] Warp target cycling for >2 players (`coop_warp_target_idx`, `coop_warp_cycle_target()`)
- [x] Warp spawn point finder with 30 retries (random offset from target position)
- [x] `MULTI_WARP_TO_PLAYER` network packet (17 bytes)
- [x] Cooldown timer (60s), engagement timeout (20s normal, 5s after respawn)
- [x] Android popup button via `WarpButtonOverlay.kt` (long-press cycles targets)
- [ ] F1 menu entry for coop (deferred)
- [x] JNI exports: `nativeGetCoopWarpStatus`, `nativeGetCoopWarpTargetName`, `nativeCoopWarpExecute`, `nativeCoopWarpCycleTarget`
- [x] Respawn tracking with shorter engagement timeout
- [x] Duplicate in d1
- [x] Build passes, lint passes
- [x] Test: coop multiplayer test passes (ALL CHECKS PASSED)
- [ ] Test: 2 emulators, verify warp works when far from teammate
- [ ] Test: verify locked door constraint blocks warp

### Phase 6: End-of-Level Score Breakdown
- [x] Modify `DoEndLevelScoreGlitz()` to show per-player stats
- [x] Duplicate in d1
- [x] Build passes, lint passes
- [ ] Test: finish a coop level, verify breakdown display

### Phase 7: Multi-Slot Autosave + Lobby Resume (completed)
- [x] Rotating autosave slots 5-9 (`coop_autosave_next_slot` counter)
- [x] `coop_autosave_history.json` with slot/mission/level/timestamp/callsigns/client_ids
- [x] Kotlin lobby save picker in `CreateLobbyDialog` (filtered by mission + client_id)
- [x] `coop_restore_slot.txt` mechanism: Kotlin writes slot, C reads and deletes
- [x] Removed C engine fallback slot scanning -- lobby is sole driver of restore
- [x] Auto-set level field in lobby when save selected/deselected
- [x] Fix: shield/energy overlay always showing full -- JNI had `* 100 / F1_0` instead of `/ F1_0`
- [x] Build passes, lint passes (both d1 and d2)

### Phase 8: 3D Player Locator HUD + Follow Line (future)
- [ ] 3D arrow/icon rendering in gamerend.c
- [ ] Off-screen edge indicator
- [ ] "Follow me" path line using AI pathfinding
- [ ] Duplicate in d1

---

## Backward Compatibility Notes

**Network packets:** Adding new `MULTI_*` packet types will cause `Int3()` (fatal assertion) on older clients that receive them. This is a non-issue because:
1. `MULTI_PROTO_VERSION` is checked at join time via `net_udp_check_game_info_request()`
2. Mismatched versions get `UPID_VERSION_DENY` and cannot join
3. Bump `MULTI_PROTO_VERSION` whenever new packet types are added
4. All android builds come from the same APK, so version mixing is unlikely
5. For desktop redux clients: they'll simply see a version mismatch and be told to update

**Save files:** The metadata trailer (appended after existing data with a tag) is backward-compatible:
- Old game versions read the save normally and stop before the trailer
- The trailer's `COOP_SAVE_META_TAG` lets new versions detect and parse it
- If the tag is missing, the metadata is simply unavailable -- graceful degradation

**UPID packets:** The UDP-level packet dispatch silently drops unknown types (`con_printf(CON_DEBUG, ...)` + return). So adding new UPID types is safer -- but still gate behind version check for correctness.

---

## Key files to modify

### C side (each change in both d1/ and d2/)
| File | Changes |
|------|---------|
| `main/multi.c` | auto-save hooks, MULTI_WARP_TO_PLAYER packet, MULTI_COOP_SAVE_INFO packet, disconnect save, absent player tracking |
| `main/multi.h` | new packet type definitions, coop_kill_stats struct, MULTI_PROTO_VERSION bump, netplayer_info client_id field |
| `main/multibot.c` | per-player kill attribution in multi_do_robot_explode() |
| `main/collide.c` | engagement time tracking (damage dealt/received), solo kill tracking |
| `main/gameseq.c` | total_robot_score_value computation at level start, coop_progress.json write, stats reset |
| `main/state.c` | coop_save_metadata write/read, callsign remapping on restore |
| `main/gameseg.c` | (read only -- find_point_seg for warp validation) |
| `main/escort.c` | (read only -- create_bfs_list for reachability, may need to expose) |
| `main/wall.h` | (read only -- wall/key structs for understanding) |

### Android / JNI
| File | Changes |
|------|---------|
| `android/app/src/main/cpp/shared/android_jni_overlay.c` | new JNI exports for coop stats, teammate status, warp trigger |
| `android/app/src/main/java/.../CoopStatsOverlay.kt` | new overlay class |
| `android/app/src/main/java/.../MainActivity.kt` | overlay integration, warp button |
| `android/app/src/main/java/.../net_udp setup` | lobby resume suggestions |

### Shared new files
| File | Purpose |
|------|---------|
| `main/coop_save.c` / `.h` | coop_progress.json read/write, metadata helpers, warp logic, absent player management |
| `android/.../multiplayer/ClientIdentity.kt` | persistent installation UUID generation/storage, GPGS player_id retrieval |

---

## Constants (shared C + potential Kotlin use)

```c
#define COOP_WARP_DISTANCE_THRESHOLD  (F1_0 * 200)   // distance to trigger warp offer
#define COOP_WARP_ENGAGEMENT_TIMEOUT  (F1_0 * 20)     // 20s no engagement before warp available
#define COOP_WARP_RESPAWN_TIMEOUT     (F1_0 * 5)      // shorter timeout after respawn
#define COOP_WARP_COOLDOWN            (F1_0 * 60)     // 60s cooldown after warp
#define COOP_WARP_MAX_RETRIES         30               // spawn point attempts
#define COOP_WARP_OFFSET_SCALE        4                // ship_size * this = spawn offset distance
#define COOP_AUTOSAVE_SLOT            9                // .mg9 = auto-save slot
#define COOP_SAVE_META_TAG            0x434F4F50       // "COOP"
#define COOP_SAVE_META_VER            1
#define COOP_MAX_REMEMBERED_PLAYERS   16               // absent players carried in metadata
#define COOP_CLIENT_ID_LEN            36               // UUID string length (no null)
```
