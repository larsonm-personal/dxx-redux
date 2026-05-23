# Coop Guidebot, Host Migration, and Touch Wheel Updates

## Summary

Five work areas:
1. Guidebot is deleted at level start in coop (1-line fix)
2. Guidebot ownership not persisted in coop saves (small addition)
3. Host migration for coop (significant new feature, targeting 2-player first)
4. Guidebot control messages (HUD messages for ownership events)
5. Guidebot touch wheel updates (show owner callsign, add release-control option)

---

## Issue 1: Guidebot not spawned in coop

### Root cause
`gameseq_init_network_players()` in d2/main/gameseq.c:232 unconditionally deletes
companion robots in all multiplayer modes:

```c
if ((Objects[i].type==OBJ_ROBOT) && (Robot_info[Objects[i].id].companion) && (Game_mode & GM_MULTI))
    obj_delete(i);
```

The comment on line 199 says "delete buddy bot if coop game" but the code deletes
it in ALL multiplayer. Later code in `multi_delete_extra_objects()` (multi.c:4479)
properly guards with `!(Game_mode & GM_MULTI_COOP)`, but by then the guidebot is
already gone.

### Fix
Add coop exclusion to the condition:

```c
if ((Objects[i].type==OBJ_ROBOT) && (Robot_info[Objects[i].id].companion)
    && (Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))
    obj_delete(i);
```

This pattern `(Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP)` is the
standard idiom used throughout the codebase (escort.c:419, collide.c:1073, etc).

### Other blockers found and already fixed
The following coop escort code already exists and should work once the guidebot spawns:
- escort.c:305 -- ownership assignment on first release
- escort.c:421 -- coop owner check in buddy_message()
- escort.c:1745-1755 -- do_escort_menu() coop support
- escort.c:1900-1944 -- multi_send_escort_owner / escort_transfer_ownership_on_disconnect
- ai.c:755 -- AI execution gated on Escort_owner_player

### Remaining blocker: escort hotkeys
gamecntl.c:1037 still blocks escort hotkeys for ALL multiplayer:

```c
if (!(Game_mode & GM_MULTI))
    set_escort_special_goal(key);
else
    HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
```

Needs the same coop exception, with an owner check.

### Files to edit
- d2/main/gameseq.c:232 -- add `!(Game_mode & GM_MULTI_COOP)` to deletion guard
- d2/main/gamecntl.c:1037-1040 -- allow escort hotkeys in coop for owner

---

## Issue 2: Guidebot ownership not in coop saves

### Current state
- Robot objects (including guidebot) ARE saved in save files (state.c saves all objects)
- Escort AI state IS saved: Escort_kill_object, Escort_last_path_created,
  Escort_goal_object, Escort_special_goal, Escort_goal_index, Stolen_items
  (via ai_save_state / ai_restore_state in ai.c)

### Gap
`Escort_owner_player` (escort.c:112) is NOT saved or restored. On load it resets
to -1, meaning the first player to release the guidebot re-claims it. This is
arguably acceptable behavior (the bot is re-caged on level load anyway via
`init_buddy_for_level()` which sets `Buddy_allowed_to_talk = 0`), but for
mid-level saves it means ownership is lost.

### Considerations
- Save file format compatibility: adding data to ai_save_state changes the format.
  Could use the coop_save_metadata trailer instead, which is version-tagged
- `Buddy_allowed_to_talk` is also not saved (reset on level load), meaning
  mid-level guidebot released state is lost. This is an existing issue
- `Buddy_objnum` is recomputed by `init_buddy_for_level()` via `find_escort()`

### Recommendation
Add `Escort_owner_player` to the coop_save_metadata trailer (bump version).
This keeps the standard save format untouched and only affects coop saves.
Also add `Buddy_allowed_to_talk` so mid-level saves restore release state.

### Files to edit
- d2/main/coop_save.h -- add escort_owner_player and buddy_allowed_to_talk fields
- d2/main/coop_save.c -- save/restore the new fields
- d2/main/escort.c -- extern declarations if not already present

---

## Issue 3: Coop host migration

### Historical context
The original D2 IPX netcode had infrastructure for host migration. Traces remain:
- `change_playernum_to()` (multi.c:5005) -- copies callsign between player slots
  and reassigns Player_num. Has commented-out original code. Used during join/rejoin
  flows at 12+ call sites in net_udp.c
- `MULTI_KILL_HOST` / `MULTI_KILL_CLIENT` packet types (multi.h:117) -- separate
  kill authority packets for host vs client, implying the host role could change
- Demo recording (newdemo.c:3714) forces `change_playernum_to(0)` with comment
  "this is reality" -- suggesting player numbers were once dynamic
- `is_master_ip()` (net_udp.c:894) validates sender is the recognized master,
  implying master identity was designed to be checkable rather than hardcoded

This was lost during the Rebirth/Redux migration to UDP. The UDP protocol is
centralized hub-and-spoke (host relays all packets), unlike IPX which was P2P.

### Current state
- `multi_who_is_master()` returns hardcoded 0
- `multi_i_am_master()` returns `(Player_num == 0)`
- When player 0 disconnects, multi_disconnect_player() force-quits all clients

### Architecture: host-relay (not P2P)
All game packets route through the host (net_udp.c:7465 relay logic). Clients
send to host, host re-broadcasts to all. This means:
- Clients don't know each other's IP addresses
- When host drops suddenly, clients can't communicate at all
- This is fundamentally different from IPX P2P where any node could talk to any other

### Targeting 2-player coop first
For 2-player coop, the relay problem disappears -- when the host drops, only one
client remains, and that client already knows the host's address (which is now
irrelevant since they're alone). The client just needs to:
1. Detect the host is gone (timeout)
2. Promote itself to master
3. Continue playing solo (or accept new joiners)

For 3+ player games, the remaining clients would need to discover each other's
addresses. This could be solved with a broadcast peer address list during
migration, but is out of scope for the initial implementation.

### All callers of multi_i_am_master() / multi_who_is_master()

**Master-only responsibilities (things new master must do):**
- Robot position updates (multibot.c:914)
- Robot respawn decisions (multibot.c:1015)
- Matcen spawning (fuelcen.c:391)
- Powerup cap enforcement (multi.c:5580+)
- Kill computation and relay (multi.c:2507-2542)
- Game mode broadcast every 2 sec (multi.c:1442)
- Player rejoin sync (multi.c:3850)
- Wall status sync for rejoiners (multi.c:6532)
- Netgame state broadcast (net_udp.c:1968)
- Coop autosave (coop_save.c:534)
- Game presence broadcast every 10 sec (net_udp.c:6317)

**Client-to-master messages (must reroute to new master):**
- Player move requests (multi.c:1887)
- Kick requests (multi.c:1940)
- Kill reactor requests (multi.c:2008)
- Kill credit (multi.c:3952 -- MULTI_KILL_CLIENT)
- Bounty updates (multi.c:6250)
- Goal counts (multi.c:6319)
- Score updates (multi.c:6406)

### State that only the host tracks
- `object_owner[MAX_OBJECTS]` -- who created each object (-1 = level-loaded)
- `respawnable_bots[]`, `robo_death_time[]`, `NextRespawnWave` -- robot respawn
- `PowerupsInMine[]`, `MaxPowerupsAllowed[]` -- powerup caps
- `Control_center_destroyed`, `Countdown_seconds_left` -- reactor countdown
- Routing: peer address table `Netgame.players[i].protocol.udp.addr`

### Implementation approach for 2-player coop

**Step 1: Dynamic master variable**
Replace hardcoded functions with dynamic lookup:
```c
int Multi_master_playernum = 0;  // initialized to 0 on game start

int multi_i_am_master(void) { return (Player_num == Multi_master_playernum); }
int multi_who_is_master(void) { return Multi_master_playernum; }
```
This is the one change that makes everything else work -- all 30+ call sites
automatically use the new master.

**Step 2: Detection and election (2-player)**
In `multi_disconnect_player()`, when the disconnected player is the current
master, instead of force-quitting:
```c
if (pnum == multi_who_is_master()) {
    // Find next connected player
    int new_master = -1;
    for (int i = 0; i < N_players; i++) {
        if (i != pnum && Players[i].connected == CONNECT_PLAYING) {
            new_master = i;
            break;
        }
    }
    if (new_master >= 0) {
        Multi_master_playernum = new_master;
        HUD_init_message(HM_MULTI, "You are now the game host");
        // Begin master responsibilities
        return;
    }
    // No other players -- end game
}
```

**Step 3: Host state bootstrap**
When a client becomes master, it already has most state from normal sync:
- Object positions, types, segments (received every frame)
- Door/wall states (received via MULTI_DOOR_OPEN packets)
- Reactor state (received, tracked locally in Countdown_seconds_left)
- Player states (received via MULTI_POSITION packets)

State the new master needs to initialize:
- `object_owner[]`: set all to -1 (level-loaded) -- safe default
- Robot respawn: reset respawn timers -- robots already dead stay dead
- Powerup caps: recount from live objects via `multi_powcap_count_powerups_in_mine()`
- Start broadcasting game presence (net_udp.c periodic sends)

**Step 4: No new packet type needed for 2-player**
With only 2 players, when one drops the other detects it via timeout. No
coordination packet is needed. The surviving player self-promotes. For future
3+ player support, a MULTI_HOST_MIGRATION packet would be added.

**Step 5: Kotlin-layer LAN broadcast resumption**
When the new host takes over, it must start broadcasting LAN game availability
so the disconnected original host can find the game and rejoin.

**Architecture context:**
- MainActivity (game engine) runs in `:game` process
- SetupActivity and LobbyService run in the main process
- These are separate OS processes -- LobbyService is an `object` singleton in the
  main process, not accessible from `:game`
- The original host's LobbyService was broadcasting ANNOUNCE packets with
  `status: "in_game"` every 3 seconds on port 42400 via subnet-directed broadcast
- The joiner's LobbyService was stopped at game launch (`stopDiscovery()`)

**Cross-process communication:**
Since the C-layer host migration detection happens in `:game` process but
LobbyService lives in the main process, we need cross-process signaling.

The established pattern is:
1. C layer calls a JNI method on MainActivity (g_activity reference)
2. MainActivity sends an Android broadcast Intent
3. SetupActivity's BroadcastReceiver (in main process) handles it

**Recommended flow:**

```
C: multi_disconnect_player() detects host left
C: Multi_master_playernum = Player_num (self-promote)
C: android_notify_host_migration()  // new JNI callback
    |
    v
Kotlin (game process): MainActivity.onHostMigration()
    |-- sendBroadcast(Intent("com.dxxredux.HOST_MIGRATION")
    |     .putExtra("callsign", myCallsign)
    |     .putExtra("game", gameVariant)   // "d1" or "d2"
    |     .putExtra("mission", missionName)
    |     .putExtra("mode", "coop")
    |     .putExtra("difficulty", difficulty)
    |     .putExtra("level_num", levelNum))
    |
    v
Kotlin (main process): SetupActivity.hostMigrationReceiver
    |-- LobbyService.startDiscovery(context, callsign)
    |-- LobbyService.hostLobby(callsign, game, mission, "coop", 2)
    |-- LobbyService.startGame(difficulty, levelNum)
    |     (this sets gameStarted=true and starts broadcasting
    |      ANNOUNCE with status="in_game")
```

**What the broadcast contains:**
The LAN ANNOUNCE packet (already defined in LobbyProtocol.kt) includes:
- type: "ANNOUNCE"
- lobby_id: new UUID (fresh lobby for the migrated game)
- callsign: new host's name
- game: "d1" or "d2"
- mission, mode, difficulty, level_num
- player_count: 1
- max_players: 2
- status: "in_game"
- build: git commit count

**What the disconnected player sees:**
When the original host's game exits ("Host left" or similar), it returns to
SetupActivity. If they go to LAN join, they'll see the game listed with
status "in_game" and can rejoin via the normal join flow.

**Data the C layer needs to expose via JNI:**
Most of these are already accessible or trivially readable:
- `Players[Player_num].callsign` -- already exposed via nativeSetCallsign (stored)
- `gameVariant` -- already known by MainActivity from launch intent
- `Current_mission_filename` or equivalent -- need new JNI getter
- `Difficulty_level` -- need new JNI getter
- `Current_level_num` -- need new JNI getter
- Alternatively: write the migration info to a file and have the broadcast
  receiver read it, avoiding new JNI methods

**Simpler alternative: file-based signaling**
Instead of a broadcast Intent with extras, the C layer could:
1. Write `files/host_migration.json` with all game state
2. Call `android_notify_host_migration()` which just sends a bare Intent
3. SetupActivity reads the file for details

This avoids plumbing all the game state through JNI parameters and is consistent
with the existing introspection file-based pattern. However, since the file is
written by `:game` process and read by the main process, it must go through the
app's shared files directory (both processes have access via `Context.filesDir`).

**Handling the rejoin:**
When the original host joins the migrated game:
1. They see the game on LAN (ANNOUNCE with status="in_game")
2. The ANNOUNCE has a new lobby_id (different from original)
3. They join via normal LAN join flow
4. The C engine on the new host accepts the join as a normal mid-game rejoin
5. Coop rejoin logic (already implemented) restores the rejoining player's state

**Edge cases:**
- If both players disconnect simultaneously, neither needs to broadcast
- If the new host also exits before the old host rejoins, both end up at
  SetupActivity -- stale broadcast stops via stopInGameBroadcast()
- LobbyService.startDiscovery() requires a Context (application context from
  SetupActivity) -- the receiver has access to this via its ctx parameter
- The new lobby_id means old JOIN_ACK state is irrelevant, preventing confusion

**Files to edit (in addition to the C-layer migration files):**

C/JNI layer:
- android/app/src/main/cpp/jni_main.c -- android_notify_host_migration() function
  that calls MainActivity.onHostMigration()
- d2/main/multi.c -- call android_notify_host_migration() after self-promotion

Kotlin game process:
- MainActivity.kt -- onHostMigration() method, sends broadcast Intent
  Also needs to expose game metadata for the broadcast extras (or write file)

Kotlin main process:
- SetupActivity.kt -- register hostMigrationReceiver BroadcastReceiver
  On receive: start LobbyService discovery and hosting in in-game mode

### Risk assessment for 2-player approach

| Risk | Severity | Mitigation |
|------|----------|------------|
| Robot respawn state lost | Low | Robots already spawned are fine; dead ones don't respawn (acceptable) |
| Powerup caps wrong | Low | Recount from live objects |
| object_owner confusion | Low | Reset to -1; only affects duplicate prevention |
| In-flight packets | None | 2-player: no relay needed when alone |
| Coop save after migration | Medium | New master can save; old saves on old host's disk are lost |
| Countdown timer | Low | Client already tracks locally; just continues |
| Boss teleport state | Low | Boss positions synced; teleport timers reset |

### Files to edit
- d2/main/multi.c -- Multi_master_playernum global, dynamic functions, election
- d2/main/multi.h -- extern for Multi_master_playernum
- d2/main/net_udp.c -- periodic broadcast now checks dynamic master
- d1/main/multi.c -- same changes for D1
- d1/main/multi.h -- same

---

## Issue 4: Guidebot control messages

### Message list
1. "Guide-Bot: you have control" -- first player to release guidebot in coop
2. "Guide-Bot control has migrated to you" -- ownership transfer on disconnect
3. "You have Guide-Bot control" -- on coop save load when restored as owner

### Implementation details

**Message 1: First release**
- Location: `ok_for_buddy_to_talk()` in escort.c:305-307
- Currently sets `Escort_owner_player = Player_num` and sends network packet
- Add: `HUD_init_message_literal(HM_DEFAULT, "Guide-Bot: you have control")`
  right after the assignment
- Only fires on the owner's client (already guarded by `Escort_owner_player == -1`)

**Message 2: Ownership transfer on disconnect**
- Location: `multi_do_escort_owner()` in escort.c:1910-1917
- This is the RECEIVER side -- called on remote clients when they get the
  MULTI_ESCORT_OWNER packet
- Currently just sets `Escort_owner_player = new_owner` and shows
  "Guide-Bot is now following you"
- Change message to: "Guide-Bot control has migrated to you"

**Message 3: Save load**
- Location: after coop_save_metadata is restored in coop_save.c
- When `Escort_owner_player` is restored and equals `Player_num`, show message
- Need to check that `Buddy_objnum >= 0` (guidebot exists in saved level)
- Add: `HUD_init_message_literal(HM_DEFAULT, "You have Guide-Bot control")`

### HUD message system
- `HUD_init_message_literal(HM_DEFAULT, "text")` -- plain text, no format args
- `HUD_init_message(HM_DEFAULT, "format %s", arg)` -- printf-style
- Messages display for ~3 seconds, max 4 visible in stack
- HM_DEFAULT is correct for these (not HM_MULTI which is for kill feed)

### Files to edit
- d2/main/escort.c -- messages 1 and 2 (ok_for_buddy_to_talk, multi_do_escort_owner)
- d2/main/coop_save.c -- message 3 (after restore)

---

## Issue 5: Guidebot touch wheel updates

### Current behavior
- Guide wheel is completely hidden when player is not escort owner
  (TouchOverlayView.kt:722-725, `continue` skips drawing)
- Owner gets full wheel with 9 segments + "Clear" center button
- Non-owner sees nothing -- no indication guidebot exists

### Change 1: Show "Controlled by [callsign]" for non-owners

**Current code** (TouchOverlayView.kt:722-725):
```kotlin
if (rm.control.id == "Guide" &&
    (gameVariant == "d1" || isEscortOwnerProvider?.invoke() == false)) {
    continue  // completely hidden
}
```

**New behavior**: Don't skip drawing. Instead, when the wheel is pressed by a
non-owner, show the owner's callsign as center text instead of the normal segments.

**Implementation**:
1. New JNI function: `nativeGetEscortOwnerCallsign()` in jni_main.c
   - Returns `Players[Escort_owner_player].callsign` as a string
   - Returns empty string if no owner or not coop
2. New Kotlin callback: `escortOwnerCallsignProvider` in TouchOverlayView
3. In draw path: if Guide wheel and not owner, draw wheel trigger normally but
   when opened, show single-segment overlay with "Controlled by [callsign]"
   instead of the normal command segments
4. Don't fire any action on release for non-owners

### Change 2: Add "Release" option to owner's wheel

**Add new meta action**: `META_GUIDE_RELEASE_CONTROL = 1014`
- Add constant in TouchBindings.kt
- Add to meta action name map
- Add segment to Guide wheel: label "Release", binding META_GUIDE_RELEASE_CONTROL

**C-side handler** (android_meta_actions.c):
- On META_GUIDE_RELEASE_CONTROL, call a new function `escort_release_control()`
- In escort.c: `escort_release_control()` picks a random other connected player
  and transfers ownership via `multi_send_escort_owner(new_owner)`
- Show HUD message: "Guide-Bot control released"
- On receiving end: "Guide-Bot control has migrated to you" (same as disconnect transfer)

**Random assignment** (not lowest-numbered, to be fair in 3+ player games):
```c
void escort_release_control(void) {
    if (Escort_owner_player != Player_num) return;
    if (!(Game_mode & GM_MULTI_COOP)) return;

    // Collect connected players other than self
    int candidates[MAX_PLAYERS];
    int n = 0;
    for (int i = 0; i < N_players; i++) {
        if (i != Player_num && Players[i].connected == CONNECT_PLAYING)
            candidates[n++] = i;
    }
    if (n == 0) return;  // no one to give it to

    int new_owner = candidates[d_rand() % n];
    Escort_owner_player = new_owner;
    multi_send_escort_owner(new_owner);
    HUD_init_message_literal(HM_DEFAULT, "Guide-Bot control released");
}
```

### Where center button goes
The Guide wheel currently has "Clear" as the center button (META_GUIDE_CLEAR_GOAL).
The "Release" option should be a regular segment (10th segment), not replacing
the center. The wheel handles 9+ segments fine.

### Files to edit

**Kotlin/Java (android layer)**:
- TouchBindings.kt -- add META_GUIDE_RELEASE_CONTROL constant, name, segment
- TouchOverlayView.kt -- change Guide wheel non-owner behavior to show callsign
- MainActivity.kt -- add escortOwnerCallsignProvider callback

**C/JNI**:
- jni_main.c -- add nativeGetEscortOwnerCallsign() JNI function
- android_meta_actions.c -- handle META_GUIDE_RELEASE_CONTROL
- d2/main/escort.c -- add escort_release_control() function
- d2/main/escort.h -- declare escort_release_control()

---

## Phased work plan

### Phase 1: Guidebot spawn fix (small, safe)
- [x] Edit d2/main/gameseq.c:232 -- add coop exclusion to companion deletion
- [x] Edit d2/main/gamecntl.c:1037-1040 -- allow escort hotkeys in coop for owner
- [x] Build and verify guidebot appears in coop level start
- [ ] Test: guidebot is in cage, can be released, follows owner
- [ ] Test: second player cannot use escort menu or hotkeys
- [ ] Test: single-player still works normally

### Phase 2: Guidebot control messages
- [x] Add "you have control" message in ok_for_buddy_to_talk() on first release
- [x] Change "is now following you" to "control has migrated" in multi_do_escort_owner()
- [x] Build and test message display

### Phase 3: Guidebot save persistence (small)
- [x] Add Escort_owner_player to coop_save_metadata (bump version)
- [x] Add Buddy_allowed_to_talk to coop_save_metadata
- [x] Save/restore in coop_save.c
- [x] Add "You have Guide-Bot control" message on restore when owner
- [ ] Test: save mid-level with released guidebot, reload, verify ownership + message

### Phase 4: Touch wheel updates
- [x] Add nativeGetEscortOwnerCallsign() JNI function
- [x] Add escortOwnerCallsignProvider callback plumbing (MainActivity -> TouchOverlayView)
- [x] Change Guide wheel to show "Controlled by [callsign]" for non-owners instead of hiding
- [x] Add META_GUIDE_RELEASE_CONTROL constant and wheel segment
- [x] Add escort_release_control() in escort.c with random assignment
- [x] Handle META_GUIDE_RELEASE_CONTROL in android_meta_actions.c
- [x] Build and test wheel behavior for owner and non-owner

### Phase 5: Host migration for 2-player coop
- [x] Add Multi_master_playernum global in multi.c, initialize to 0
- [x] Change multi_i_am_master() and multi_who_is_master() to use it
- [x] In multi_disconnect_player(): elect new master instead of force-quit
- [x] New master bootstraps: recount powerups, reset object_owner, start broadcasts
- [x] Show "You are now the game host" HUD message
- [x] Add android_notify_host_migration() in jni_main.c (C->Kotlin callback)
- [x] Add onHostMigration() in MainActivity.kt (sends cross-process broadcast)
- [x] Add hostMigrationReceiver in SetupActivity.kt (starts LobbyService hosting)
- [x] Write host_migration.json from C layer with game/mission/level/difficulty
- [x] LobbyService starts in-game broadcast so old host can find and rejoin
- [ ] Test with 2-player coop: host leaves, client takes over, old host sees game
- [ ] Test rejoin: old host joins the migrated game via LAN
- [ ] Verify robot AI, powerups, endlevel still work under new master
- [x] Mirror changes to D1/main/multi.c and multi.h
- [ ] Integration test

### Phase 6: Testing and polish
- [ ] Automated test: launch coop, verify guidebot present via introspection
- [ ] Manual test: host disconnect, verify game continues for remaining player
- [ ] Manual test: after host migration, verify LAN ANNOUNCE broadcasts resume
- [ ] Manual test: disconnected host sees game in LAN browser and can rejoin
- [ ] Manual test: guidebot ownership transfer and release
- [ ] Manual test: touch wheel shows callsign for non-owner
- [ ] Run existing test suite to verify no regressions
- [x] Code quality: run run-code-quality.ps1 --fix
- [x] Full assembleDebug build passes with no new warnings
