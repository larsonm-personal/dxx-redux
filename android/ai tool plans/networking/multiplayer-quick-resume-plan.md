# Multiplayer Quick Resume Plan

## Scope

Planning only for a launcher/game flow that offers a quick multiplayer resume button when first opening the multiplayer submenu.

Primary target: two players who habitually play LAN coop together and want one obvious action on each device to get back to a hosted game with the recent coop save loaded.

## Research Checklist

- [x] Map current launcher multiplayer entry points
- [x] Map LAN discovery and direct IP join paths
- [x] Map host setup and save game preset paths
- [x] Survey multiplayer autosave/checkpoint/save slot behavior
- [x] Identify state that must be persisted after host/client sessions
- [x] Note implications for future multiplayer rewind support
- [x] Propose phased implementation and tests

## Current Implementation

### Launcher Entry And Multiplayer UI

- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
	- `SetupScreen` owns `showMultiplayerPage`
	- The main launcher `Multiplayer` button sets `showMultiplayerPage = true`
	- `MultiplayerScreen(onBack, onLaunchGame)` is shown as a full subpage
	- `launchMultiplayerGame(info)` creates the game-process intent and passes multiplayer extras
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt`
	- Top-level nav is `BROWSER`, `LOBBY`, `FRIENDS`, `LAN`
	- Browser page has server URL, callsign, connect, create lobby, LAN entry, lobby list, active games
	- Connected browser page already shows `RecentCoopGames`, but only after connecting to the matchmaking server
	- LAN page wraps `LanDiscoveryTab`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`
	- `GameLaunchInfo` is the central launch DTO for online and LAN paths
	- `HostGameDefaults` persists host defaults in `dxx_prefs`
	- `CallsignPrefs` persists the launcher multiplayer callsign

### LAN Discovery And Direct IP

- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt`
	- UDP JSON lobby discovery on `NetworkConstants.LAN_LOBBY_PORT` 42400
	- Game engine UDP port is `NetworkConstants.ENGINE_PORT` 42424
	- `startDiscovery()` opens socket and receives LAN lobby packets
	- `hostLobby()` starts a hosted lobby and announce loop
	- `joinLobby()` sends JOIN with retries
	- `joinLobbyByIp()` and `tryJoinLobbyByIp()` send `QUERY` to a known IP, wait for `ANNOUNCE`, then join
	- `startGame()` sends START to joiners, keeps in-game announces alive, and emits host `GameLaunchInfo`
	- `handleStart()` emits joiner `GameLaunchInfo`
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt`
	- ANNOUNCE includes lobby id, callsign, game, mission, mode, player count, max players, build, status, difficulty, level, host port
	- QUERY exists, so the requested "ping previous server IP" can reuse existing protocol behavior
	- Missing: stable host identity, session fingerprint, and selected restore slot are not in LAN protocol
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt`
	- `LanDiscoveryView` shows Host LAN Game, Start/Stop Scanning, Join by IP, discovered lobbies
	- Direct IP join first probes for a lobby, then falls back to direct engine join with generic launch info
	- Direct fallback defaults to coop, level 1, difficulty 1, empty mission, and no save restore context
	- `LanLobbyCard` supports Join In-Game for lobbies announcing `status = in_game`
	- `LanCoopSaveOffer` can auto-select a matching coop save while hosting
- `android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt`
	- Persists recent LAN IPs and matchmaking server URLs, max 5 each
	- This is only address history, not resumable session history

### Online Matchmaking

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt`
	- `connect()` stores current server URL and callsign in `MatchmakingState`
	- `createLobby()` sends game info to the server
	- `joinLobby()` joins by server lobby id
	- `startGame()` requests game start
	- `GameStarting` creates `GameLaunchInfo` and stores it in state for `LobbyScreen`
	- `lastLobbyId` exists only for in-memory websocket reconnect after a transient disconnect
	- Missing: durable last server, durable last lobby, host identity, or known-party resume record
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt`
	- Consumes `gameLaunchInfo` and calls `onLaunchGame`
	- Host-only `CoopSaveOffer` picks the best matching save by lobby callsigns and writes `coop_restore_slot.txt`

### Game Launch Bridge

- `SetupActivity.launchMultiplayerGame(info)`
	- Host intent extras: callsign, host mode, port, mission, game mode int, max players, level, difficulty, coop QoL, full death spew
	- Join intent extras: host address and port, through localhost proxy for LAN joiners
	- Clears `MatchmakingStateHolder.gameLaunchInfo` after consumption
	- Stops LAN discovery for non-LAN-host launches, but LAN hosts keep announcing while in game
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
	- Reads multiplayer extras in the game process
	- Sets native callsign and client id
	- Calls `nativeSetAutoJoin()` or `nativeSetAutoHost()`
- `android/app/src/main/cpp/shared/net/auto_net.c` and `.h`
	- Stores auto-host/auto-join globals
	- `check_auto_net()` triggers engine network actions from the main menu
- `d1/main/net_udp.c` and `d2/main/net_udp.c`
	- Propagate Android client id into `Netgame.players[].client_id`

### Coop Saves, Progress, And Restore

- `android/app/src/main/cpp/shared/coop/coop_save.c` and `.h`
	- Shared D1/D2 coop save metadata and autosave implementation
	- Coop autosaves use `COOP_AUTOSAVE_CALLSIGN` = `coopsave`
	- Rotating autosave slots are `COOP_AUTOSAVE_SLOT_FIRST` 5 through 9
	- Save files are `Players/coopsave.mg5` through `Players/coopsave.mg9`
	- Metadata trailer records active and absent players with callsign, client id, inventory, stats, level, mission, difficulty
	- `coop_autosave()` writes a save and updates `coop_autosave_history.json`
	- `coop_write_progress_json()` writes checkpoint/progress data after level completion plus `coop_progress_inventory.bin`
	- `coop_arm_auto_restore()` reads `coop_restore_slot.txt`, validates a slot, and triggers host-side restore after all players connect
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`
	- Reads matching coop autosaves and progress entries
	- Writes `coop_restore_slot.txt` when a save is selected
	- Saves `HostGameDefaults`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt`
	- `readCoopAutosaveHistory()` filters history by mission and local `ClientIdentity`
	- `readCoopProgressAsEntry()` accepts old progress without client ids, otherwise filters by local id
	- `writeCoopRestoreSlot()` writes the slot consumed by native auto-restore
- `android/app/src/main/cpp/shared/state_android_shared.c`
	- Single-player Android autosave slots skip entirely if `Game_mode & GM_MULTI`
	- This prevents single-player auto-exit and auto-minimize saves from overwriting multiplayer-specific coop saves
- `android/app/src/main/cpp/android_input.c` and `android_meta_actions.c`
	- Minimize and return-to-launcher autosave gates explicitly skip multiplayer
	- Current multiplayer durability comes from coop periodic/disconnect/progress saves, not lifecycle saves

### Current Multiplayer Save Triggers

- Host-only periodic coop autosave every 30 seconds appears in both D1 and D2 `game.c`
- Host-only periodic coop autosave every 30 seconds also appears in both D1 and D2 `multi.c`
- On coop player disconnect, D1 and D2 `multi_disconnect_player()` call `coop_autosave()` after host handling
- At end-level score screen, D1 and D2 `gameseq.c` call `coop_write_progress_json()`
- Minimize and return-to-launcher do not queue coop autosave because Android lifecycle save handling skips multiplayer

### Host Migration And Rewind Context

- `d1/main/multi.c` and `d2/main/multi.c`
	- `Multi_master_playernum` is dynamic on Android
	- Coop host departure elects a new master instead of always ending the session
	- New master writes `host_migration.json`
- `SetupActivity.hostMigrationReceiver`
	- Reads `host_migration.json`
	- Starts host-mode proxy on `NetworkConstants.HOST_PROXY_PORT` 42425
	- Restarts LAN discovery and in-game announcing for the migrated host
- `android/app/src/main/cpp/shared/android_rewind.c` and `android_rewind_policy.c`
	- Rewind captures in single-player or coop-host-only contexts
	- Non-host coop requests report `Not host`
	- Non-coop multiplayer is blocked
	- Coop restore uses `multi_prepare_restore_sync()` before memory restore and resends score after host restore
- `android/app/src/main/cpp/shared/android_meta_actions.c`
	- `META_REWIND` already maps touch/controller rewind controls to `android_rewind_pending`
	- This can remain the single control entry point for host and client devices
	- The missing piece is client-side routing: a non-host coop press should send a rewind request to the current host instead of trying to use a local buffer
- `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt`
	- Existing rewind preferences include enabled/disabled state and a target amount of 5, 10, or 20 seconds
	- Multiplayer rewind should reuse the current host's existing rewind setting and buffer instead of introducing separate multiplayer rewind settings or save rings

## Missing Or Wrong Concepts

### Durable Resume State

There is no durable "last multiplayer session" model. Existing pieces are partial:

- `HostGameDefaults` remembers editable defaults, not an actual completed or launched session
- `RecentAddressPrefs` remembers addresses, not whether they hosted, what game they ran, or whether a save was loaded
- `coop_autosave_history.json` remembers saves, not the network peer or host to reconnect to
- `MatchmakingService.lastLobbyId` is process-memory only and is for websocket reconnect, not launcher resume

Needed durable state:

- schema version
- timestamp of last successful launch and last seen lobby
- role: host or client
- transport: LAN or matchmaking
- game, mission, mode, difficulty, level, max players
- coop QoL and full death spew settings
- local callsign and local client id
- host callsign and stable host client id, if known
- peer callsigns and peer client ids, if known
- LAN host IP and host port
- matchmaking server URL and lobby id or host player id, if known
- selected coop restore slot and the save timestamp/level it came from
- whether the previous session was started fresh or restored from a save
- whether the device became host through migration

### Stable Host Identity

For LAN quick resume, IP alone is not enough. DHCP can assign the previous host IP to another device, and a direct IP fallback can launch a generic join to the wrong kind of game.

Useful missing protocol fields:

- `host_client_id` in ANNOUNCE and JOIN_ACK
- optional `session_fingerprint`, derived from game, mission, mode, level, difficulty, host id, and maybe selected save slot/timestamp
- optional `restore_slot` or `restore_save_timestamp` in ANNOUNCE for display and matching only

### Client Quick Resume Cannot Load The Save By Itself

A joiner can only reconnect to a host. It cannot make the host load a save unless the host has already rehosted with the restore slot selected. So the client one-button flow should be:

1. Start LAN discovery
2. Direct-query the previous host IP
3. Prefer a matching announced host identity/session. a secondary, lower quality match, would be the same host but wrong save file, and that should be preferred if there is not alternative
4. Join that lobby or in-game announce
5. If no match, show a waiting state for the previous host

Avoid direct generic engine fallback for quick resume unless the user explicitly chooses it. That fallback lacks mission, level, difficulty, and save context.

### Host Quick Resume Needs A Hosted Lobby First

The host-side one-button flow can immediately recreate the host lobby with the previous game settings and selected coop restore slot, but should not blindly launch the engine before the expected peer is present unless an explicit auto-start policy exists.

Practical staging:

- Phase 1: one button opens a correctly prefilled hosted lobby with the best recent save armed
- Phase 2: when expected peers join and ready, allow one-button or optional auto-start
- Phase 3: true "both press one button and reach in-game" with expected peer matching and automatic start

### Save Trigger Audit Items

The current save story is good but not complete for the requested use case.

Open audit questions:

- Are the periodic coop autosave calls in both `game.c` and `multi.c` intentionally duplicated, or should they be centralized or throttled through one shared timer?
- Should Android minimize/return-to-launcher trigger a host-only `coop_autosave()` instead of skipping multiplayer entirely?
- Should quick resume prefer the newest full save, the last checkpoint progress, or the save selected in the previous host lobby?
- Should `coop_autosave_history.json` include host/client role, stable client ids for all participants, and a session fingerprint?
- Should direct IP resume refuse generic fallback when it cannot verify host identity?

### Save History Fragility

`coop_autosave_history.json` is currently built by fixed-size C string buffers. That is acceptable for the current small fields, but if this feature adds more metadata, avoid growing ad hoc JSON construction in the engine. Prefer either:

- keep the richer resume record in Kotlin preferences based on `GameLaunchInfo` and current lobby state
- or add a small native helper with bounded JSON writing and tests

### Multiplayer Rewind Controls

Current coop rewind is host-owned and volatile. Quick resume should not try to persist rewind points.

Desired behavior:

- The current host can use the existing `Rewind` binding in coop
- The host owns capture, selection, restore, and the rewind buffer
- The host uses the same rewind enabled setting and target seconds already used for single-player on that host device
- The existing single-player rewind buffer should remain the only buffer; do not add a separate multiplayer save ring
- Clients can press the same `Rewind` binding, but a non-host press should become a request packet to the current host
- A host setting should allow or deny client-initiated rewind requests per hosted coop game
- If the host denies client requests, client presses should not mutate local state and should produce a small status message
- If the host performs a rewind, the restore should work like host-initiated single-player rewind, using the host's buffer and existing coop restore sync path
- After host migration, the new host starts with an empty rewind buffer and begins capturing new points from that moment

Planning constraints:

- Durable resume should select a coop save or progress checkpoint first
- Rewind history should start fresh after the restored session loads
- Only the current coop host should capture and restore rewind points
- Clients may request a rewind only when the current host permits it
- Host migration must explicitly reset rewind history on the new host; do not transfer old rewind snapshots
- The persisted resume model should record whether the device is original host, migrated host, or client
- The persisted resume model should also remember the host's client-rewind permission so quick resume can recreate the same hosted game policy
- Do not put replay-specific or rewind-specific patches into the demo system for this feature

## Proposed Design

### New Kotlin Persistence Helper

Add `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerResumePrefs.kt`.

Store one primary resume record in `dxx_prefs` or a small JSON file under `filesDir`. SharedPreferences is probably enough for the quick shortcut, while a file is easier to inspect through setup introspection. The record should be versioned so pre-release data resets can stay simple.

Suggested shape:

```text
schema_version
updated_at_ms
role
transport
game
mission
mode
difficulty
level_num
max_players
coop_qol
full_death_spew
local_callsign
local_client_id
host_callsign
host_client_id
lan_host_addr
lan_host_port
server_url
lobby_id
peer_callsigns
peer_client_ids
coop_restore_slot
coop_restore_save_time
coop_restore_level
restore_was_selected
host_migrated
clients_can_request_rewind
```

Keep only the newest good record at first. A history list can be added later if the UI needs it.

### Persistence Hooks

Add/update the resume record at these points:

- `SetupActivity.launchMultiplayerGame(info)` after mod checks pass and before `startActivity()`
	- This sees all launches, online and LAN, host and joiner
	- It can combine `GameLaunchInfo`, current `MatchmakingStateHolder.state`, `LobbyService` state, and local client id
- `LobbyService.handleJoinAck()` or the current JOIN_ACK handler
	- Record last successful LAN host address and host identity once protocol includes it
- `LobbyService.hostLobby()`
	- Record hosted lobby setup early enough for "rehost last" even if the game was not launched
- `SetupActivity.hostMigrationReceiver`
	- Mark the local device as migrated host and update host-side LAN details
- `CreateGameDialog` and `CoopSaveOffer` / `LanCoopSaveOffer`
	- Persist the selected coop restore slot because `coop_restore_slot.txt` is one-shot and native deletes it after reading
	- Persist the host's client-rewind permission along with other hosted-game defaults

### Quick Resume Candidate Builder

Add a small Kotlin layer that returns a UI-ready candidate:

- load durable resume record
- verify game data readiness for the recorded game
- if coop and host record, scan `readCoopAutosaveHistory()` for the saved slot/timestamp and find a fallback newest matching save
- validate local client id is part of the save when possible
- for LAN client record, preflight by starting discovery and direct-querying previous host IP only after the user opens multiplayer or presses the candidate
- suppress candidates that are too incomplete, too old, or point at a missing game

### UI Placement

The request says "offer when first opening the multiplayer submenu". Best initial placement:

- In `MultiplayerScreen`, above the browser connection controls, show a compact resume panel once per entry into the multiplayer page
- If the record is host-side, primary action: `Host Last Coop`
- If the record is client-side LAN, primary action: `Find Last Host`
- If online, primary action: `Reconnect Server` or `Find Last Lobby`
- Add a dismiss/hide action for this page visit
- Do not add a launcher-wide stop-showing preference until the feature has proven useful

For LAN specifically, also surface the same quick actions at the top of `LanDiscoveryTab`, since the user may enter LAN directly from the browser controls.

### Host Action Behavior

LAN host resume should:

1. Save `HostGameDefaults` from the resume record
2. Recompute or restore the selected coop restore slot
3. Write `coop_restore_slot.txt` if a valid full save slot exists
4. Start LAN discovery if needed
5. Call `LobbyService.hostLobby()` with recorded settings
6. Show hosted lobby with `LanCoopSaveOffer` still visible and toggleable
7. Later phase: optionally auto-start when expected peer ids join and ready

Online host resume should:

1. Connect to the recorded server if disconnected
2. After auth, create a lobby with recorded game info
3. Reuse the same coop restore slot handling
4. Wait for expected players and ready state before start

### Client Action Behavior

LAN client resume should:

1. Start discovery
2. Send QUERY to previous `lan_host_addr`
3. Match on `host_client_id` if available, otherwise host callsign and game/mission/mode as a weaker signal
4. Join matching lobby or in-game announce
5. If no match, keep scanning and show a concise waiting state
6. Avoid generic direct engine fallback unless chosen explicitly

Online client resume should:

1. Connect to recorded server if disconnected
2. Refresh lobbies after auth
3. Join a lobby matching host id or host callsign plus game/mission/mode
4. If no match, leave the browser connected with status explaining that the previous host is not available

### Multiplayer Rewind Behavior

The multiplayer rewind work should dovetail with quick resume by treating client rewind permission as another hosted-game option, not as a separate subsystem.

Host setting path:

1. Add `clientsCanRequestRewind` to `HostGameDefaults` and the create/host dialogs
2. Add the same field to `GameLaunchInfo` and the launch intent extras
3. Pass the value to native auto-host setup for D1 and D2
4. Store the value in an Android-specific netgame flag or native global owned by the host session
5. Include the value in LAN/matchmaking lobby display metadata if useful
6. Include the value in `host_migration.json` so a migrated host continues the same policy
7. Persist the value in `MultiplayerResumePrefs` so quick resume recreates the previous host policy

Control path:

1. Keep `META_REWIND` as the shared touch/controller binding
2. On a coop host, the binding should queue the existing local `android_rewind_request()` path
3. On a coop non-host, the binding should send a native multiplayer rewind request to `multi_who_is_master()`
4. The client should never call `android_rewind_request()` against its own buffer in coop client mode
5. The host should validate requests on the game thread, then queue the same local rewind path used by host presses
6. The host should use its current `android_rewind_set_enabled()` and target seconds values
7. Failed or rejected requests should report a concise HUD or overlay status to the requester

Native protocol path:

1. Add fixed-size `MULTI_REWIND_REQUEST` and optional `MULTI_REWIND_RESULT` packets to both D1 and D2 `multi.h`
2. Bump the D1 and D2 multiplayer protocol versions if adding packets to the shared protocol table
3. Add handlers in both D1 and D2 `multi.c`
4. Keep packet parsing present on non-Android builds if protocol compatibility needs it, with behavior no-op or rejected there
5. Request packet should include at least requester player number and a small request id so duplicate taps can be collapsed
6. Result packet can include requester player number, status, and rewound seconds for HUD feedback
7. Validate requester slot, connection state, coop mode, host status, host permission, alive/observer state, and a basic rate limit before honoring a request
8. Ignore malformed or stale requests instead of asserting or disconnecting clients

Host restore path:

1. Reuse `android_rewind_request()` for the actual restore
2. Keep using the existing `multi_prepare_restore_sync()` call before memory restore
3. Continue using `multi_send_score()` after host restore when appropriate
4. Do not serialize or copy the rewind buffer to clients
5. Do not persist rewind buffers in quick-resume state or coop save history

Host migration path:

1. When `Multi_master_playernum` changes in D1/D2, reset Android rewind history on the new host
2. Existing non-host devices should have no useful capture history because capture is blocked while not host, but reset explicitly anyway
3. After migration, the new host starts capturing from the next valid coop frame using its own host rewind settings
4. Keep client-request permission from the migrated session if available in `host_migration.json`
5. If the migrated host has rewind disabled locally, requests should fail as disabled even if client requests are allowed

## Work Items

### Phase 1: Persistence Model And Unit Tests

- Add `MultiplayerResumePrefs.kt`
- Add record serialization/deserialization tests under `android/app/src/test/java/com/dxxredux/app/`
- Include schema version and invalid-record handling
- Include old or incomplete record suppression

### Phase 2: Record Current Sessions

- Persist launch records from `SetupActivity.launchMultiplayerGame(info)`
- Persist selected coop restore slot from host-side save selection UI
- Persist LAN join address and host metadata from JOIN_ACK once available
- Persist host migration updates from `hostMigrationReceiver`
- Add setup introspection fields for the current resume record so automation can inspect it without screenshots

### Phase 3: LAN Protocol Enrichment

- Add `host_client_id` to ANNOUNCE and JOIN_ACK
- Consider `session_fingerprint` and restore metadata for display/matching
- Update `LobbyService` parse/build paths and tests
- Keep protocol backward tolerant: absent fields should still work with weaker matching

### Phase 4: Quick Resume UI

- Add a compact resume panel to `MultiplayerScreen` on first page entry
- Add LAN quick action area in `LanDiscoveryTab`
- Implement host action to recreate LAN hosted lobby and arm restore slot
- Implement client action to scan and query previous host IP
- Keep the generic Join by IP fallback separate from quick resume

### Phase 5: Save Trigger Survey And Fixes

- Audit duplicate periodic coop autosave calls in D1/D2 `game.c` and `multi.c`
- Decide whether a shared throttle belongs in `coop_autosave()` itself
- Add host-only coop autosave on Android minimize or return-to-launcher if safe
- Confirm single-player auto slots 8 and 9 are not touched during multiplayer
- Confirm coop rotating slots 5 through 9 are used consistently in D1 and D2
- Add or extend high-level tests around save selection and restore slot writing

### Phase 6: Online Matchmaking Resume

- Reconnect recorded server URL
- Create or join matching lobby after auth
- Reuse host/client resume candidate builder
- Consider server support only if client-side matching cannot be made robust

### Phase 7: Full One-Button Reconnect

- Match expected peer ids/callsigns in the hosted lobby
- Add optional auto-ready or auto-start policy for trusted repeated LAN partner sessions
- Make this opt-in if it can surprise users
- Add a clear waiting state when only one side has pressed resume

### Phase 8: Multiplayer Rewind Controls

- Add host option for allowing client rewind requests to `HostGameDefaults`, host dialogs, `GameLaunchInfo`, launch extras, and native auto-host setup
- Keep host rewind amount and enablement tied to the existing host single-player rewind preferences
- Add D1/D2 native packet definitions and handlers for client rewind request and optional result status
- Route non-host coop `META_REWIND` presses to the current host instead of local `android_rewind_request()`
- Validate request permission, requester state, host state, and rate limiting before executing the host rewind
- Reuse the existing host rewind buffer and `android_rewind_request()` restore path
- Reset rewind history when host migration makes a client the new host
- Add tests for rewind policy classification and client-request permission

### Phase 9: Validation

- Kotlin JVM tests for resume prefs, candidate building, and LAN protocol parsing
- Existing `android/run-code-quality.ps1 --fix` after implementation work
- Android setup automation test for host resume panel and restore slot write
- Two-emulator LAN test: host resume, client find last host, join lobby, start with save armed
- Two-emulator coop rewind test: host-initiated rewind restores from host buffer
- Two-emulator coop rewind test: client request succeeds only when host setting allows it
- Two-emulator coop rewind test: client request is rejected when disabled and does not alter client-local state
- Two-emulator host migration test: new host begins with no rewind point, then captures new host-owned points
- Later: two-emulator host migration plus resume record update

## Key Files

- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ClientIdentity.kt`
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt`
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkConstants.kt`
- `android/app/src/main/cpp/shared/coop/coop_save.c`
- `android/app/src/main/cpp/shared/coop/coop_save.h`
- `android/app/src/main/cpp/shared/net/auto_net.c`
- `android/app/src/main/cpp/shared/net/auto_net.h`
- `android/app/src/main/cpp/shared/state_android_shared.c`
- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/cpp/shared/android_meta_actions.c`
- `android/app/src/main/cpp/shared/android_rewind.c`
- `android/app/src/main/cpp/shared/android_rewind_policy.c`
- `d1/main/game.c`
- `d1/main/gamecntl.c`
- `d1/main/multi.c`
- `d1/main/multi.h`
- `d1/main/gameseq.c`
- `d1/main/net_udp.c`
- `d2/main/game.c`
- `d2/main/gamecntl.c`
- `d2/main/multi.c`
- `d2/main/multi.h`
- `d2/main/gameseq.c`
- `d2/main/net_udp.c`

## Recommended First Implementation Slice

The lowest-risk first slice is Kotlin-only except for tests:

1. Add `MultiplayerResumePrefs`
2. Persist a record when `launchMultiplayerGame(info)` runs
3. Persist selected coop restore slot from existing save-offer UI
4. Show a read-only quick resume panel on multiplayer page entry
5. Implement only the LAN host action that opens a hosted lobby with restore slot armed
6. Add unit tests for the record and candidate logic

That slice proves the data model and host-side UX without changing D1/D2 engine behavior or LAN protocol yet. The next slice can enrich LAN ANNOUNCE with host identity and implement the client `Find Last Host` action safely.

Multiplayer rewind should come after that first slice. It shares the same host-defaults and resume-record plumbing, but it needs mirrored D1/D2 native packet work and two-emulator validation, so it should stay as a separate implementation phase.