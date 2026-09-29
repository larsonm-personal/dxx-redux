# Co-op session review, 2026-09-28

Review `C:/Users/first last/Downloads/debuglog_20260928_102153.txt` for initial LAN discovery, rejoining after host migration, the lobby-to-game menu flash, and classic guidebot retaining the blue-key goal

1. Reconstruct the session timeline and distinguish observations from inferred causes
2. Trace lobby role/launch state, engine window transitions, and classic guidebot key completion
3. Record confirmed defects and evidence gaps; apply only evidence-backed fixes within the requested scope
4. If code changes are needed, run scoped formatting and relevant build/tests

Existing unrelated working-tree changes and `android/outstanding_bugs.md` must be preserved

## Review results

Review completed against the supplied log and current source. No engine or launcher code changed; no build or device reproduction was performed for this review

### Initial LAN discovery: failure not established

- Log lines 222-235: discovery starts at 10:22:09 on 192.168.88.159, UDP 42400, broadcast destination 192.168.88.255, multicast lock held
- Until 10:23:31 the receive and transmit counts are equal, consistent with receiving this device's own broadcasts; these counters do not establish remote delivery
- No host announcement is recorded during this interval and no broadcast send error is recorded
- Lines 237-240: this device hosts at 10:23:36; Player68 at 192.168.88.21 joins at 10:23:51 and becomes ready
- Line 2297: a broadcast from Player68's migrated lobby is discovered at 10:27:02
- The other device's launcher/network log covering 10:22-10:24 is needed to distinguish an initial advertisement problem from the absence of an active remote lobby

### Rejoin after migration: two state defects

- Lines 2417-2427: join at 10:27:18 receives acknowledgements repeatedly through 10:27:40; transport health nevertheless reports role=host with both announcement and heartbeat jobs active
- `LobbyService.joinLobby()` does not stop existing hosting, and `handlePlayerList()` rejects player lists while `_isHosting` remains true
- At 10:27:43 the old lobby finally stops hosting. A new join immediately receives player lists, but still produces no joining-engine launch
- `SetupActivity.hostMigrationReceiver` calls `hostLobby()` followed by `startGame()`
- `hostLobby()` installs a one-player roster and sets `gameStarted=false`; `startGame()` requires at least two players, so migration cannot mark the already-running game as in progress
- Announcements therefore retain status `lobby`; `joinDiscoveredLobby()` calls ordinary lobby join rather than `emitInGameJoinLaunch()`
- This explains acknowledgements without an in-engine join notification: the joining engine never starts. The remote host's log is absent, so the specific remote return is inferred from this deterministic code path and observed client behavior
- Fix direction: a dedicated adoption path for an already-running migrated host, including proxy port, mission requirements, session options, and in-game announcement state, without new-game readiness checks or host launch events. Joining another lobby must clear stale hosting state

### Main menu flash: draw-time lobby closure

- `d2/main/net_udp.c:4925` handles the start request only during `EVENT_WINDOW_DRAW` and closes the player-selection window immediately
- `d2/arch/sdl/event.c:258` draws windows back-to-front, then calls `gr_flip()` after draw dispatch
- The underlying menu has already drawn when the lobby closes without drawing itself. The event loop presents that underlying menu frame
- `StartNewLevel()` hides menus only after `newmenu_do1()` and player selection return, too late to prevent that frame
- D1 contains the same sequence
- The log brackets level startup at 10:24:09.665 and 10:28:52.404, but contains no frame images. The source establishes a presentation path matching the reported symptom; the introducing commit was not established
- Fix direction: consume successful lobby start outside draw dispatch or suppress presentation of the interrupted frame, while preserving cancellation and error handling

### Classic guidebot blue key: wrong flag source

- Lines 3432-3872, 10:29:07-10:29:20: guidebot repeatedly retains goal=1 (blue key), special=-1, goal_index=41, route_active=0, including periodic goal refreshes
- Path log `goal=60` is a destination segment, not a different escort goal
- `d2/main/escort.c:2020` classic goal selection checks `ConsoleObject->flags` against player key bits. Keys live in `Players[pnum].flags`; object flags have unrelated meanings
- `d2/main/powerup.c:446` sets the player's blue-key bit and deliberately leaves the powerup in multiplayer. Invalidating the goal therefore causes classic selection to choose the same key again
- The existing `escort_owned_key_flags()` reads the appropriate guidebot owner's inventory and is already used by enhanced goal selection
- Commit `80af2244` reintroduced the incorrect object-flag checks when separating classic and enhanced routing
- The pickup itself is not captured in guidebot diagnostics, but the second-round repeated blue-key goal is recorded and the source defect directly explains the reported behavior
- Fix direction: use owner inventory flags for classic automatic key choice while preserving classic pathfinding. Cover blue/gold/red keys that remain in co-op, owner selection, and restored inventory
