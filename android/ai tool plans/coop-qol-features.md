# Coop Quality of Life Features -- Deep Dive and Plan

## Overview

A set of features to improve the cooperative multiplayer experience:
1. Robot kill stats overlay (per-player, by score)
2. Coop progress auto-save and session resume system
3. Warp-to-player teleportation

---

## Feature 1: Robot Kill Stats Overlay

### What it does
An in-game overlay showing:
- Robots killed vs total robots: "30/130"
- % of total robot score value killed, broken down by player (who contributed what)

### Existing infrastructure
- `Players[pnum].num_robots_level` -- total robots at level start (set in `init_player_stats_level()` via `count_number_of_robots()`)
- `count_number_of_robots()` in gameseq.c -- counts current alive OBJ_ROBOT objects
- killed = `num_robots_level - count_number_of_robots()`
- `Robot_info[id].score_value` -- per-robot-type point value
- Kill attribution: `multibot.c:895` -- `add_points_to_score(Robot_info[Objects[botnum].id].score_value)` when a robot is exploded
- `Players[pnum].score` -- individual player score in coop
- Android overlay system: JNI bridge in `android_jni_overlay.c`, Kotlin overlays in `VideoInfoOverlay.kt`

### Design approach

**C side (d2/main, d1/main):**
- Add per-player robot kill tracking: a small struct array `coop_kill_stats[MAX_PLAYERS]` with fields `{robots_killed, score_earned}`. Increment when a robot is exploded and attributed to a player
- This data is already partially available: `Players[pnum].num_kills_level` tracks robot kills, `Players[pnum].score - Players[pnum].last_score` tracks score earned this level. But `score` includes hostage bonuses, so a dedicated `robots_score_earned` counter is cleaner
- Hook into `multi_explode_robot_sub()` / `collide.c` robot death path to increment per-player counters
- Total robot score needs to be computed at level start: iterate all OBJ_ROBOT objects, sum `Robot_info[obj.id].score_value`. Store as `total_robot_score_value`
- Expose via JNI: a native function like `nativeGetCoopRobotStats()` returning an int array: [killed, total, total_score, p0_kills, p0_score, p1_kills, p1_score, ...]

**Kotlin side:**
- New overlay class `CoopStatsOverlay` (similar to `VideoInfoOverlay`)
- Polls native stats at ~1 Hz (no need for high frequency)
- Renders: "Robots: 30/130" and per-player "PlayerA: 45% | PlayerB: 55%" (% of robot score earned)
- Only visible when `Game_mode & GM_MULTI_COOP`

**Multiplayer sync:**
- Robot kills are already synced via `MULTI_ROBOT_EXPLODE` packets
- Score is already synced via `MULTI_SCORE` packets
- The overlay can use local `Players[]` data since it's kept in sync -- no new network packets needed
- However, per-player robot score tracking needs to happen on all clients. Currently `add_points_to_score()` only updates the local player's score. The `MULTI_SCORE` packet broadcasts the score, but doesn't distinguish "robot points" from "other points". Options:
  - Track robot kills per player via existing `num_kills_level` (already synced) and approximate score from kill count * average robot value -- rough but zero new packets
  - Add a new `MULTI_ROBOT_SCORE` packet -- cleanest, but adds network complexity
  - Use the existing `Players[pnum].score` difference from level start -- already available, includes some non-robot points but close enough for an overlay

**Recommendation:** Use `Players[pnum].score - Players[pnum].last_score` for the per-player score contribution. It's already synced, already available, and the non-robot score components (hostage bonus) are minor. For the kill count, `num_kills_level` is already per-player and synced. For total robots, compute at level start and store globally.

The one gap: `num_kills_level` is only tracked for player 0 on remote machines (see `multibot.c:892` -- `Players[0].num_kills_level++`). This is a limitation. We'd need to track which player actually killed each robot on all clients. The `MULTI_ROBOT_EXPLODE` packet includes the killer object number, so we can resolve to player index and track locally. This means adding a small hook in `multi_do_robot_explode()`.

### Complexity: Medium
- C changes: ~50-80 lines (tracking arrays, level-start init, JNI export, hook in robot explode)
- Kotlin changes: ~100-150 lines (new overlay class)
- d1/d2 duplication: Yes, same hooks needed in both

### Risks / edge cases
- Matcen (robot generator) robots: `num_robots_level` is incremented when matcen spawns a new robot (`ai2.c:1804`, `fuelcen.c:327`, `multibot.c:1144`), so the total count grows during gameplay. The overlay denominator should use the *current* `num_robots_level` not the initial snapshot
- Boss robots may respawn or have special kill handling
- Observer mode players shouldn't contribute to stats
- Score display should handle 0 total gracefully (no divide by zero)

---

## Feature 2: Coop Progress Auto-Save and Session Resume

This is the most complex feature. Breaking it into sub-features:

### 2A: Level Completion Checkpoint

**What it does:** When players finish a level together, record that "Player A and Player B completed level N of mission M". When those players are next in a lobby together, suggest the next level.

**Design:**
- New file format: `coop_progress.json` in the player's save directory
- Written by the game engine (C side) at level-end in coop
- Read by the launcher/lobby (Kotlin side) when players assemble

**File structure:**
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

**C side:**
- Hook into the level-end path in coop (when players advance to next level)
- Write/update `coop_progress.json` with the completed level info
- Key locations: `DoEndLevelScoreGlitz()` in `gameseq.c`, or the level transition in `multi.c`
- Player set identification: sorted list of callsigns (order-independent matching)

**Kotlin side:**
- When a coop lobby forms with 2+ players, read each player's `coop_progress.json`
- Find matching sessions (same player set, same mission)
- Suggest "Resume from level 6?" in the lobby UI
- This requires knowing who's in the lobby before the game starts -- the lobby screen already shows connected players

**Key question:** How is "same players" defined? Callsigns are only 8 chars and not guaranteed unique across the internet. For the matchmaking server, players might have server-assigned IDs. For now, callsign matching is good enough for a QoL feature.

### 2B: Auto-Save on Player Disconnect

**What it does:** When a player leaves a coop game (disconnect or quit), automatically create a save file.

**Design:**
- Hook into `multi_disconnect_player()` / `multi_do_quit()` in `multi.c`
- Before cleanup, trigger `state_save_all_sub()` to a special auto-save slot
- File naming: `{callsign}.ma{level}` (ma = multiplayer auto-save) or re-use an existing `.mg` slot (slot 9 as "auto" slot)
- Include metadata: level number, player scores, timestamp, player list

**Current save system constraints:**
- `multi_initiate_save_game()` requires: host is not observer, host initiates, all players alive, unique callsigns
- Auto-save should bypass the "all players alive" check -- if someone disconnects while dead, still save
- The save should happen on each remaining client independently (each saves their own state)

**Approach:**
- Use save slot 9 (last of NUM_SAVES=10) as the dedicated auto-save slot
- When any player disconnects in coop: each remaining player auto-saves to slot 9
- When the disconnecting player's client processes the quit, it also auto-saves to slot 9 before exiting
- Store extra metadata in the save description: "Auto L5 2p 12345pts"

### 2C: "Last in Mine" Save

**What it does:** When a player becomes the last remaining player (all others disconnected), create the same auto-save.

**Design:**
- This is a special case of 2B -- when `N_players` drops to 1 in coop
- After the last disconnect event is processed, check if local player is the only one left
- Trigger auto-save
- Optionally show a message: "All other players left. Game saved"

**Implementation note:** The existing disconnect flow in `multi.c` already handles N_players tracking. Add a check after disconnect processing:
```c
if ((Game_mode & GM_MULTI_COOP) && count_connected_players() == 1) {
    // auto-save and notify
}
```

### 2D: Session Resume from Lobby

**What it does:** When the same players are in a lobby again, find the best save file to resume from.

**Design -- this is the tricky part:**

**Problem:** Save files are per-player (each player has their own `.mg9`). When players reconnect, we need to:
1. Identify that these are the "same" players from a previous session
2. Find each player's auto-save file
3. Compare timestamps to find the newest/best one
4. Load that save once all players have joined

**Approach:**
- When a coop game is being set up (lobby phase), after players join:
  - Each client scans for `.ma*` or `.mg9` files matching the current mission
  - Each client sends metadata about their save files to the host (new packet type or extend existing handshake)
  - Host compares and picks the newest save with matching player sets
  - Host presents option: "Resume from Level 5 auto-save? [Yes/No]"
  - If Yes, host sends `MULTI_RESTORE_GAME` once all players are in-game

**New network protocol needed:**
- `MULTI_COOP_SAVE_INFO` packet: player sends save file metadata (level, timestamp, player list hash) to host during lobby
- Or: extend the join handshake to include this data

**Alternative simpler approach:**
- Don't try to match across network. Instead:
  - The `coop_progress.json` file tracks which level to start on
  - The lobby suggests starting from the saved level number
  - Each player loads their own auto-save independently once the level loads
  - This avoids save file transfer but means each player resumes with their own inventory state

**Recommendation:** Start with the simpler approach (level suggestion + individual auto-saves). The complex save-comparison approach can come later.

### 2E: Save File Metadata

Save files need additional metadata for the resume system to work:
- Player list (all callsigns in the session, sorted)
- Timestamp (already have `GameTime64` but need wall-clock time)
- Level number (already saved)
- Mission name (already saved)
- Per-player scores (already saved in player struct)

**Add a small trailer or header extension to save files** with:
```c
struct coop_save_metadata {
    uint32_t wall_clock_timestamp;  // Unix epoch seconds
    uint8_t num_players;
    char player_callsigns[MAX_PLAYERS][CALLSIGN_LEN+1];
    int32_t player_scores[MAX_PLAYERS];
};
```

This is written after the existing save data (as a tagged extension block so old readers skip it).

### Complexity: High
- C changes: ~200-400 lines across multi.c, state.c, gameseq.c, new coop_progress code
- Kotlin changes: ~150-300 lines for lobby integration
- Network protocol: possibly 1-2 new packet types
- d1/d2 duplication: Yes
- Testing: complex due to multi-player disconnect scenarios

### Risks / edge cases
- Race condition: if two players disconnect simultaneously, who saves what?
- Save file corruption if game crashes during auto-save
- Player callsign changes between sessions break matching
- Different game versions/mods between sessions
- Save file compatibility across game updates
- What happens if a player joins a "resumed" game who wasn't in the original session?
- Coop saves require unique callsigns (existing check in `multi_initiate_save_game`)

---

## Feature 3: Warp to Player

### What it does
In coop games, when players are far apart and one player hasn't engaged a robot recently, offer a popup button to teleport to the other player.

### Existing infrastructure
- `find_point_seg(pos, segnum)` in `gameseg.c` -- checks if a position is inside the mine, returns segment number or -1
- `obj_create()` validates positions before placing objects
- `compute_segment_center()` -- gets center of a segment
- `ConsoleObject->pos` / `Objects[Players[pnum].objnum].pos` -- player positions
- `vm_vec_dist()` -- distance between two points
- Android popup button system: the touch overlay can add buttons dynamically

### Design

**Trigger conditions (all must be true):**
1. Game mode is coop (`Game_mode & GM_MULTI_COOP`)
2. At least 2 players connected
3. Distance between local player and nearest other player exceeds threshold (e.g. some multiple of segment size, or N segments of path distance). Using Euclidean distance is simpler; path distance is more accurate but expensive to compute
4. Local player hasn't damaged a robot in the last N seconds (e.g. 15-30 seconds). Track via a `last_robot_engagement_time` variable
5. Local player is alive (not dead/respawning)

**Warp mechanics:**
1. Target = nearest connected teammate
2. Target position = `Objects[Players[target_pnum].objnum].pos`
3. Spawn offset = random direction vector * (2 * ship_radius) -- "a couple ship lengths"
4. Candidate spawn position = target_pos + spawn_offset
5. Validate: `find_point_seg(&candidate_pos, target_segment)` must return valid segment (not -1)
6. Also check no intersection with robots or other players within ship_radius
7. If invalid, try another random direction. Up to 30 attempts
8. If all fail, show "Warp failed -- no clear space" message
9. If success: teleport local player, notify other players via new `MULTI_WARP_TO_PLAYER` packet

**Network sync:**
- New packet `MULTI_WARP_TO_PLAYER`: `{warping_player, target_player, new_pos, new_segment}`
- All clients update the warping player's position
- This is similar to how respawning works -- position is updated and broadcast

**UI:**
- Popup button appears on screen (Android touch overlay) when conditions are met
- Labeled "Warp to [callsign]" with the target player name
- Button disappears when conditions no longer met (player engages robot, gets close enough, etc.)
- Also available in the F1/options popup menu during coop games
- Cooldown after use: e.g. 60 seconds before it can be used again

**Collision avoidance for spawn point:**
```c
int try_warp_spawn(vms_vector *target_pos, int target_seg, vms_vector *result_pos) {
    for (int attempt = 0; attempt < 30; attempt++) {
        vms_vector offset;
        // Random unit vector * 2 ship lengths
        vm_vec_make(&offset, (d_rand()-16384)*2, (d_rand()-16384)*2, (d_rand()-16384)*2);
        vm_vec_normalize(&offset);
        vm_vec_scale(&offset, ConsoleObject->size * 4); // 2 ship diameters

        vms_vector candidate;
        vm_vec_add(&candidate, target_pos, &offset);

        int seg = find_point_seg(&candidate, target_seg);
        if (seg == -1) continue; // outside mine

        // Check for intersecting objects
        if (!check_object_object_intersection(&candidate, ConsoleObject->size, seg))
            continue;

        *result_pos = candidate;
        return seg; // success
    }
    return -1; // failed
}
```

**Menu integration:**
- Add "Warp to Player" option in the F1 game menu, visible only when `Game_mode & GM_MULTI_COOP`
- When selected from menu, perform the warp immediately (same logic as the popup button)

### Complexity: Medium-High
- C changes: ~150-250 lines (warp logic, network packet, engagement tracking, spawn validation)
- Kotlin changes: ~80-120 lines (popup button, menu entry)
- Network protocol: 1 new packet type
- d1/d2 duplication: Yes

### Risks / edge cases
- Player warps into a segment that's about to be destroyed or is behind a locked door
- Target player is in a secret area that hasn't been discovered
- Target player is moving at high speed -- warp destination is stale by the time it executes
- Multiple players trying to warp to same target simultaneously
- Warping through walls could be used to skip puzzle sections (doors, keys) -- is this desired in coop?
- What if the target player is in a tiny segment where no offset fits?
- Network latency: position may be slightly outdated
- Prevent warp spam: cooldown timer or single-use per "separation event"

---

## Additional Related Ideas and Implications

### A. Coop lobby "party" system
Currently players are identified only by callsign. For reliable session resume, consider a lightweight party system:
- When players finish a session, generate a "party token" (hash of sorted callsigns + mission)
- Store in `coop_progress.json`
- On reconnect, match by party token rather than individual callsign comparison
- This handles cases where a third player joins -- it's a new party

### B. Coop score breakdown end-of-level screen
The existing end-of-level score display (`DoEndLevelScoreGlitz`) could show per-player robot kill contributions. This pairs naturally with Feature 1.

### C. "Follow me" marker
Related to warp-to-player: a "follow me" ping that places a visible marker on the automap and HUD compass. Lower-impact than teleporting, useful for coordination.

### D. Shared inventory visibility
The overlay could also show teammate inventory (keys held, weapons) so players know what the team has. This helps coordinate who picks up what.

### E. Robot difficulty scaling
With reliable session tracking, could adjust robot difficulty based on the number of players (more HP in 2p coop). This is a much bigger change but the per-player tracking enables it.

### F. Disconnect grace period
Before creating the auto-save on disconnect, give a 30-60 second window for reconnection. If the player reconnects within the window, no save is needed. This prevents save churn from brief network drops.

### G. Server-side session tracking
The matchmaking server could track coop sessions. When both players connect to the server, it could automatically suggest "resume your D2 coop session with PlayerB? (Level 5)". This requires the server to store session data, but it's a natural extension.

### H. Coop chat/ping improvements
If players are separated (the same trigger as warp-to-player), a "ping my location" feature that shows a directional arrow on the other player's HUD pointing toward the pinger.

### I. Mid-level join considerations
Currently joining a coop game mid-level may not be fully supported. The session resume system implies players might drop and rejoin at different times. Ensuring mid-level join works smoothly (player gets appropriate loadout, robots are synced, etc.) is important for the overall experience.

### J. Spectator/observer integration
If a player dies and is waiting to respawn, the warp-to-player mechanic should be suppressed. But after respawning, if the respawn point is far from the action, the warp button should appear quickly (maybe with a shorter engagement timer).

### K. Anti-exploit: warp should not bypass key gates
If the target player is in a segment behind a locked door that the warping player hasn't unlocked, the warp could bypass key requirements. Options:
- Allow it (coop is cooperative, resources are shared anyway)
- Block it (check if any locked doors separate the players)
- Allow with warning
For coop, **allowing it is probably fine** since keys are shared team resources

### L. Save file storage on Android
Android file storage is sandboxed. Save files live in `files/` within the app's data directory. The `coop_progress.json` and auto-save files should go in the same location. No special handling needed beyond using PHYSFS paths that map to the app's internal storage.

---

## Implementation Order (Suggested Phases)

### Phase 1: Robot Kill Stats Overlay
- Lowest risk, self-contained
- Pure additive (no changes to save system or networking)
- Establishes the overlay pattern for coop
- ~2-3 sessions of work

### Phase 2: Warp to Player
- Medium complexity, high gameplay impact
- Requires 1 new network packet
- Can be tested with 2 emulators
- ~3-4 sessions

### Phase 3: Auto-Save on Disconnect / Last in Mine
- Core save system hooks
- Needs careful testing of disconnect scenarios
- ~2-3 sessions

### Phase 4: Level Completion Tracking + Session Resume
- Builds on Phase 3
- Requires lobby UI changes
- Most complex integration
- ~4-5 sessions

### Phase 5: Polish and Related Features
- End-of-level score breakdown
- Follow-me markers
- Server-side session tracking
- Grace period on disconnect
- Ongoing

---

## Open Questions

1. **Scope of d1 support:** Should all features apply to both D1 and D2 coop, or D2 first?
2. **Warp exploit policy:** Is warping past key gates acceptable in coop?
3. **Save slot allocation:** Use slot 9 for auto-saves, or add new slots beyond the existing 10?
4. **Session resume UX:** Should it auto-load the save, or just suggest the level number?
5. **Player identity:** Callsign-only matching, or add some persistent player ID?
6. **Distance threshold for warp:** Fixed constant, or scale with level size?
7. **Engagement timer:** What counts as "engaging" a robot? Firing at one, being fired at, taking damage?
8. **Max coop players:** The UI currently limits coop to 2 players. Should these features support 3-4 player coop?
