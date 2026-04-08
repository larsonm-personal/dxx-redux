# UI Cleanup Batch 2

## Status: Complete (build verified)

## Issues

### 1. Admin tray: view buttons cleanup
**Priority: Low | Effort: Small**

Remove the `ADMIN_DECREASE_VIEW` ("View -") button and rename `ADMIN_INCREASE_VIEW` from "View +[Cockpit|Status|Full]" to "Cycle View[Cockpit|Status|Full]".

Files:
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
  - Remove `ADMIN_DECREASE_VIEW` constant (line ~387) and shift subsequent constants
  - Update `adminTrayLabel()` (line ~2722): change "View +" prefix to "Cycle View"
  - Remove the grid entry for view- from the tray drawing code
  - Update button count / grid layout
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
  - Remove `ADMIN_DECREASE_VIEW` handler from adminTrayCallback (line ~536)

### 2. Admin tray: make "exit" actually exit
**Priority: Low | Effort: Small**

Currently both "Exit" (`META_RETURN_TO_LAUNCHER`) and "Game Menu" (`ESC`) exist. META_RETURN_TO_LAUNCHER already does ESC + SDL_QUIT (bypasses "Abort Game?" dialog) and exits back to the launcher/setup screen.

Investigate: user reports they "do the same thing". Check if `android_force_quit` flag in `android_meta_actions.c` (line ~117) is properly skipping the abort confirmation. If the flag is working, the buttons should have different behavior. Possibly the SDL_QUIT isn't being processed quickly enough and the ESC opens the game menu first.

If "exit" is already correctly exiting to launcher, the issue may be user confusion. Otherwise, fix the flow so the exit is immediate.

### 3. Host LAN game dialog: scroll indicators
**Priority: Low | Effort: Small**

`CreateGameDialog.kt` uses `Modifier.verticalScroll(rememberScrollState())` but lacks scroll indicator arrows. Other pages (AdvancedSettingsPage, GraphicsSettingsPage, etc.) use a pattern with `KeyboardArrowUp`/`KeyboardArrowDown` in circular surfaces.

Add the same scroll arrow pattern:
- Save the `rememberScrollState()` to a val
- Add top/bottom arrow indicators conditionally based on `scrollState.canScrollBackward`/`canScrollForward`
- Use the established `Surface(shape = CircleShape, ...)` style from AdvancedSettingsPage

### 4. LAN games view: full-page scrolling
**Priority: Medium | Effort: Medium**

Currently the LAN tab (`LanDiscoveryTab.kt`) uses a non-scrollable outer `Column` with a `LazyColumn(Modifier.weight(1f))` for just the game list. In landscape, the fixed header content (permission card, buttons, IP display, hosted lobby info) takes most of the screen.

Approach: Convert the entire page to a single `LazyColumn` so everything scrolls together:
- Permission card -> `item { ... }`
- Action buttons -> `item { ... }`
- Local IP / hosted lobby info -> `item { ... }`
- Discovered lobbies -> `items(...)` (existing)
- Diagnostics -> `item { ... }`

This makes the full page scrollable while keeping the lobby list virtualized. The "Start/Stop Scanning" and "Host LAN Game" buttons scroll with the content, which is acceptable since they're needed once then the user scrolls to games.

### 5. LAN rejoin: "missing required packets" failure
**Priority: High | Effort: Large**

#### Symptom
Client joins host's LAN lobby successfully at the Kotlin layer (JOIN -> JOIN_ACK -> START). C engine auto_join gets GAME_INFO and does `do_join_game`. Host receives UPID_REQUEST with `Network_status=PLAYING` and triggers `net_udp_do_refuse_stuff` (because `RefusePlayers=1`). Host shows accept prompt. User presses accept. Client eventually gets "Failed to join the netgame. You are missing packets." from `net_udp_verify_objects()`.

#### Analysis of the join flow

1. Kotlin START launches the game on both sides
2. Host's C engine boots faster, enters NETSTAT_PLAYING
3. Client's C engine boots, auto_join gets GAME_INFO showing NETSTAT_PLAYING
4. `net_udp_can_join_netgame` returns 3 (RefusePlayers path) -- join proceeds
5. Client calls `StartNewLevel` -> `net_udp_level_sync` -> `net_udp_wait_for_sync`
6. Client sets `Network_status=NETSTAT_WAITING`, sends UPID_REQUEST
7. Host receives UPID_REQUEST in NETSTAT_PLAYING, calls `net_udp_do_refuse_stuff`
8. In `do_refuse_stuff`: checks if callsign+address matches existing player -- this is a fresh game so no match
9. Host shows HUD message "wants to join (accept: F6)", sets `WaitForRefuseAnswer=1`
10. Subsequent UPID_REQUESTs check `WaitForRefuseAnswer==1` and compare callsigns
11. If REFUSE_INTERVAL timer expires before F6 press -> player gets DUMP_DORK and silently rejected
12. If F6 pressed in time -> `net_udp_welcome_player` is called

In `net_udp_welcome_player` (line ~1991), several checks can cause rejection:
- `Endlevel_sequence || Control_center_destroyed` -> DUMP_ENDLEVEL
- `Network_send_objects || Network_sending_extras` -> silently ignored (already sending to someone)
- **`their->player.connected != Current_level_num`** -> DUMP_LEVEL -- this is the most likely failure point

The `connected` field in UPID_REQUEST carries the client's `Current_level_num`. If the client's level state is stale from a previous session or not properly initialized during auto_join, this check fails.

If welcome_player succeeds, it calls `net_udp_send_objects()` which sends all game objects to the client. Client's `net_udp_read_object_packet()` then calls `net_udp_verify_objects(remote_objnum, object_count)`:
- Returns 2 if `(remote - local) > 10` (packet loss)
- Returns 1 if `Netgame.max_numplayers > nplayers` (not enough player/ghost objects)
- Returns 0 on success

For a mid-game join, the second check is the likely failure: `max_numplayers` (e.g. 4) may exceed the actual player+ghost objects in the mine if objects weren't properly created/maintained.

#### Investigation plan

1. Add MPDIAG logging in `net_udp_welcome_player` at each rejection point:
   - Log `their->player.connected` vs `Current_level_num` for the level check
   - Log `Network_send_objects` and `Network_sending_extras` state
   - Log whether endlevel check fires

2. Add MPDIAG logging in `net_udp_verify_objects`:
   - Log `remote_objnum`, `object_count`, `nplayers`, `Netgame.max_numplayers`

3. Add MPDIAG logging in `net_udp_read_object_packet` at the verification call point

4. Add MPDIAG logging in `net_udp_do_refuse_stuff` for the accept/reject flow:
   - Log when WaitForRefuseAnswer transitions
   - Log when RefuseThisPlayer is set (F6 pressed)
   - Log the REFUSE_INTERVAL timeout path

5. Add MPDIAG logging on the client side in `net_udp_wait_for_sync`:
   - Log when SYNC is received
   - Log N_players and Network_status after sync processing

6. Check that `UDP_Seq.player.connected` is set to `Current_level_num` before sending UPID_REQUEST in the auto_join path. Trace where `UDP_Seq` is initialized.

7. Reproduce with enhanced logging, capture both host and client logs

8. Fix the root cause based on which check fails

#### Possible fixes (depending on root cause)
- If level number mismatch: ensure `Current_level_num` is set from `Netgame.levelnum` before `net_udp_send_request` in the auto_join path
- If verify_objects fails on player count: relax the `max_numplayers <= nplayers` check for mid-game joins (the missing player objects are expected -- they'll be created when the player joins)
- If REFUSE_INTERVAL timeout: increase the timer or show the accept prompt more prominently on Android (admin tray "Accept" button already exists)

**Same investigation needed for d1/** -- the code is nearly identical.

### 6. Scanning doesn't resume after failed join
**Priority: Medium | Effort: Small**

When a non-host joiner's game launches (`SetupActivity.kt` line ~1310), `LobbyService.stopDiscovery()` is called. This closes the socket and sets `_isDiscovering = false`. If the game fails to join and exits back to the setup screen, the user must manually click "Start Scanning" again.

Fix approach:
- In `SetupActivity.kt`, when the game exits back (detected in `onResume` or wherever the game process completion is handled), check if discovery was previously active and auto-restart it
- Store a `wasDiscoveringBeforeLaunch` flag before calling `stopDiscovery()`
- In the onResume handler (or game-exit callback), if `wasDiscoveringBeforeLaunch && !isDiscovering`, call `startDiscovery()` automatically

Files:
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- add flag and auto-resume logic
- Possibly `LobbyService.kt` -- add a `wasActive` flag

### 7. Broadcast stops on 2nd hosting round
**Priority: High | Effort: Medium**

User reports: after hosting, stopping, then re-hosting, broadcast packets aren't being sent. Direct IP join still works.

#### Analysis

`hostLobby()` (LobbyService.kt line ~190):
- Cancels old `announceJob`
- Resets `gameStarted = false`
- Launches new announce coroutine on `scope`

The announce coroutine uses `scope?.launch(...)` but `scope` is created in `openSocket()`. If `stopHosting()` was called without `stopDiscovery()`, the scope and socket remain. But if `stopDiscovery()` was called (which cancels the scope), then `hostLobby()` checks `if (!_isDiscovering.value) return` and would bail out.

Possible failure modes:
1. **Discovery was stopped between hosting rounds**: `hostLobby()` returns early at the `_isDiscovering` guard. User needs to re-start scanning before hosting again
2. **Socket closed/reopened**: `openSocket()` creates a new scope, but `hostLobby()` launches on the old scope reference if there's a stale `scope` variable
3. **Broadcast addresses changed**: `getBroadcastAddresses()` might return empty on 2nd call (Wi-Fi state change, network interface flapping)
4. **`announceJob` coroutine exits silently**: The `while (isActive && _isHosting.value)` loop might exit if `_isHosting` was momentarily false

#### Investigation plan

1. Add NetLog entries in `hostLobby()` at each stage: entering, discovery check, creating job
2. Add NetLog to `broadcastAnnounce()` at entry: log `hostedLobbyId`, `_isHosting.value`
3. Add NetLog to `sendBroadcast()` showing broadcast address count and success/fail per address
4. Check if `stopHosting()` is being called implicitly during any UI transitions
5. Test with explicit logging of announce job lifecycle (started, completed, cancelled)
6. Reproduce and check logcat

#### Likely fix
The most probable cause is that `stopDiscovery()` is called during game exit (issue 6), which cancels the scope. Then re-hosting fails because `_isDiscovering` is false. The fix for issue 6 (auto-resume discovery) would also fix this.

Alternatively, if the issue is that `sendBroadcast` silently fails because broadcast addresses aren't being found, emit a diagnostic and fall back to 255.255.255.255.

### 8. "Join by IP" rejects in-progress games
**Priority: Medium | Effort: Medium**

When using "Join by IP" for a game that's already in progress, the flow is:
1. `tryJoinLobbyByIp()` sends QUERY to host, waits 1s for ANNOUNCE
2. If host IS still broadcasting (status="in_game"), an ANNOUNCE is received with the lobby
3. Client calls `joinLobby()` which sends JOIN to host
4. Host's `handleJoin()` checks `if (gameStarted)` -> rejects with "game already started"

The Kotlin lobby layer always blocks joins after `gameStarted=true`. But the C engine handles mid-game joins via UPID_REQUEST/NETSTAT_PLAYING with the RefusePlayers approval flow.

#### Fix approach

Option A (preferred): Allow mid-game joins in the Kotlin lobby layer
- In `handleJoin()`, when `gameStarted` is true AND status is "in_game", don't reject -- instead send JOIN_ACK and emit a launch event with an `isInGame=true` flag
- The C engine will handle the actual join negotiation
- The lobby acts as a facilitator to get the joiner's C engine pointed at the host's IP

Option B: Bypass the Kotlin lobby for mid-game joins
- In `tryJoinLobbyByIp()`, if the discovered lobby has `status="in_game"`, skip the JOIN handshake entirely and fall through to direct engine join
- This avoids the `handleJoin` rejection but loses the lobby's player list and metadata

Option A is better because it gives the joiner the correct game/mission/level info from the ANNOUNCE packet, which the C engine needs for `do_join_game`.

Also: in `LanLobbyCard`, there's already a "Join In-Game" button that appears for in-game lobbies. Make sure this button's handler uses the same mid-game join path.

Files:
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` -- modify `handleJoin()` to allow in-game joins
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt` -- ensure "Join In-Game" and "Join by IP" both handle in-progress games

## Execution order

1. **Issue 5 (rejoin diagnosis)** -- highest priority, most complex, needs diagnostic logging first
2. **Issue 7 (broadcast 2nd hosting)** -- related to discovery lifecycle
3. **Issue 6 (scan resume)** -- related fix, likely helps issue 7
4. **Issue 8 (join by IP in-progress)** -- Kotlin lobby change
5. **Issue 4 (full-page scroll)** -- UI improvement
6. **Issue 3 (scroll indicators)** -- small UI fix
7. **Issue 1 (view buttons)** -- trivial UI change
8. **Issue 2 (exit button)** -- investigate first, may be non-issue

## Phase 1: Diagnostic logging for rejoin (Issue 5)

Add MPDIAG logging to both d1/ and d2/ in:
- `net_udp_welcome_player`: each rejection branch
- `net_udp_verify_objects`: input values and result
- `net_udp_do_refuse_stuff`: WaitForRefuseAnswer transitions, timeout, accept
- `net_udp_wait_for_sync`: sync receipt, N_players after
- `net_udp_read_object_packet`: at verify call
- Check `UDP_Seq.player.connected` initialization in auto_join path

Build, deploy to both emulators, reproduce the rejoin failure, capture logs from both sides.

## Phase 2: Network fixes (Issues 5-8)
Based on Phase 1 diagnostics, fix the rejoin root cause. Then:
- Fix discovery auto-resume (Issue 6)
- Fix broadcast lifecycle (Issue 7)
- Allow mid-game lobby joins (Issue 8)

## Phase 3: UI fixes (Issues 1-4)
- Full-page scroll for LAN tab
- Scroll indicators for host dialog
- Admin tray button cleanup
- Exit button investigation

## Phase 4: Testing
- Run multiplayer test suite
- Manual verification of rejoin flow
- Code quality pass (`run-code-quality.ps1 --fix`)
