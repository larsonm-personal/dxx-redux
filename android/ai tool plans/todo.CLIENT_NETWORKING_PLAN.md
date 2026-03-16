# Client Networking Plan (Android Launcher)

## Overview

The Android launcher (SetupActivity / Compose UI) needs a full multiplayer
flow: connect to the matchmaking server, browse lobbies, manage friends,
see active games, and ultimately launch the game engine with the right
auto-join or auto-host parameters.

This document covers the client-side UI, libraries, permissions, and
implementation steps. The server-side protocol is defined in
todo.NETWORKING_PLAN.md (Phases 2, 2.5, 3, 4).

---

## Architecture

```
SetupActivity (existing)
  |
  +-- MultiplayerScreen (new Compose screen, entry point)
  |     |
  |     +-- ServerBrowser tab (lobbies waiting for players)
  |     +-- ActiveGames tab (games in progress, display only)
  |     +-- Friends tab (online friends, presence, join/message)
  |     +-- LAN tab (local WiFi discovery)
  |     |
  |     +-- ConnectionStatusBar (own ping, online count)
  |
  +-- LobbyScreen (inside a specific lobby)
  |     |
  |     +-- Player list with ready states
  |     +-- Host controls (kick, settings, start)
  |     +-- Game settings panel
  |     +-- Chat/message area
  |
  +-- MatchmakingService (singleton, owns WebSocket + state)
        |
        +-- WebSocket connection to matchmaking server
        +-- State: lobbies, friends, ping, session
        +-- LAN discovery (UDP broadcast)
```

### Data Flow

```
MatchmakingService (Kotlin coroutine)
    |
    | WebSocket (OkHttp / Ktor client)
    v
Matchmaking Server (Rust, WSS on port 9000)
    |
    | Messages: AUTH_OK, SERVER_STATUS, LOBBY_LIST,
    |           FRIEND_LIST_RESP, LOBBY_UPDATE, etc.
    v
StateFlow<MatchmakingState> (observable by Compose UI)
    |
    v
MultiplayerScreen / LobbyScreen (Compose)
```

---

## Dependencies to Add

All versions pinned in `get_deps/tool_versions.conf`.

### WebSocket Client

**OkHttp** (recommended): already a transitive dependency of many Android
libraries, lightweight, well-tested WebSocket support.

```gradle
implementation "com.squareup.okhttp3:okhttp:4.12.0"
```

Alternative: Ktor client (more Kotlin-idiomatic but heavier).
OkHttp is preferred because it's smaller and doesn't pull in a full
HTTP framework.

### Google Play Games Services (identity)

```gradle
implementation 'com.google.android.gms:play-services-games-v2:21.1.0'
```

Required for stable player identity. The GPGS player ID is hashed or
mapped server-side to our stable UUID. The raw GPGS ID is never shared
with other clients.

### JSON Serialization

Already using serde-compatible JSON on the server. Client side:

```gradle
implementation "org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3"
```

Kotlin serialization is preferred over Gson/Moshi because it works with
data classes and sealed classes (matching the tagged union message format).

### Version Pins (add to tool_versions.conf)

```
OKHTTP_VERSION=4.12.0
PLAY_GAMES_VERSION=21.1.0
KOTLINX_SERIALIZATION_VERSION=1.7.3
```

---

## Permissions Required

### Already Present

- `android.permission.INTERNET` -- WebSocket + UDP

### New Permissions Needed

```xml
<!-- LAN discovery: WiFi multicast/broadcast -->
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />

<!-- Foreground service for maintaining connection during game -->
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_CONNECTED_DEVICE" />
```

### Runtime Permissions (Android 13+)

- **Nearby WiFi Devices** (`android.permission.NEARBY_WIFI_DEVICES`):
  required on Android 13+ (API 33) for WiFi-based peer discovery.
  Must request at runtime before starting LAN discovery.
  Pre-API 33 devices use `ACCESS_FINE_LOCATION` instead (required for
  WiFi scanning on older Android versions). we aren't supporting pre-33 devices (google play blocks new uploads for those now) so don't bother

```kotlin
// In MultiplayerScreen, before enabling LAN tab:
if (Build.VERSION.SDK_INT >= 33) {
    requestPermission(Manifest.permission.NEARBY_WIFI_DEVICES)
} else {
    requestPermission(Manifest.permission.ACCESS_FINE_LOCATION)
}
```

- **Microphone** (`android.permission.RECORD_AUDIO`): only needed for
  Phase 7 (voice chat). Not needed for initial implementation.

---

## UI Layout

### Main Multiplayer Screen

Entry point from SetupActivity. Three (later four) tabs plus a
persistent status bar.

```
+--------------------------------------------------------------+
| [<- Back]          MULTIPLAYER              [own ping: 32ms] |
|                                             [23 online]      |
+--------------------------------------------------------------+
| [Server Browser]  [Active Games]  [Friends]  [LAN]           |
+--------------------------------------------------------------+
|                                                              |
|  (tab content area)                                          |
|                                                              |
+--------------------------------------------------------------+
```

#### Tab: Server Browser (default)

Shows lobbies with state == Waiting that are joinable.

```
+--------------------------------------------------------------+
| Lobbies waiting for players (3)                              |
+--------------------------------------------------------------+
| [Coop] Counterstrike! L1        PlayerOne    2/4   ~45ms     |
|        Host ping: 12ms + Your ping: 32ms                     |
| [Join]                                                       |
+--------------------------------------------------------------+
| [Anarchy] Mars War Zone         AceFlyer     5/8   ~67ms     |
|        Host ping: 35ms + Your ping: 32ms                     |
| [Join]                                                       |
+--------------------------------------------------------------+
| [Coop] Vertigo!                 NovaPilot    1/4   ~28ms     |
|        Host ping: 6ms + Your ping: 22ms                      |
| [Join]                                                       |
+--------------------------------------------------------------+
```

Each lobby row shows:
- Game mode icon/label (Coop, Anarchy, CTF, etc.)
- Mission name
- Host callsign
- Player count / max players (max depends on mode: coop=4, others=8)
- Estimated ping: `host_ping_ms + your_ping_ms` (shown as "~Xms")
- Breakdown on second line: "Host ping: Xms + Your ping: Xms"
- Join button

Tapping Join sends `JOIN_LOBBY { lobby_id }` and navigates to LobbyScreen.

Sorting: by estimated ping (lowest first) by default. Option to sort
by player count or mode.

Auto-refresh: the server pushes LOBBY_LIST updates. No polling needed
after the initial welcome bundle.

#### Tab: Active Games

Display-only list of games in progress. Cannot join from here (need
friend join or Phase 5 drop-in later).

```
+--------------------------------------------------------------+
| Games in progress (7, showing 7)                             |
+--------------------------------------------------------------+
| [Coop] Counterstrike! L5       3 players    12:34 elapsed    |
| [Anarchy] Asteroid Base        6 players     3:21 elapsed    |
| [Coop] First Strike L2        2 players     8:45 elapsed b   |
| ...                                                          |
+--------------------------------------------------------------+
```

Data comes from `ServerStatus.active_games` in the welcome bundle.
Updated periodically (server pushes refreshed status every 30-60 seconds
or client requests it).

#### Tab: Friends

Shows friends with presence and action buttons.

```
+--------------------------------------------------------------+
| Friends (2 online, 1 offline)                                |
+--------------------------------------------------------------+
| [*] PlayerTwo          In Game: Counterstrike! L3 (2/4)      |
|     [Join Game]  [Message]                                   |
+--------------------------------------------------------------+
| [*] AceFlyer           In Lobby: Mars War Zone (waiting)     |
|     [Join Lobby] [Message]                                   |
+--------------------------------------------------------------+
| [ ] NovaPilot          Offline (last seen: 2h ago)           |
|     [Remove]                                                 |
+--------------------------------------------------------------+
|                                                              |
| [Add Friend by Callsign]                                     |
+--------------------------------------------------------------+
| Pending Requests:                                            |
| RocketJoe wants to be your friend   [Accept] [Decline]       |
+--------------------------------------------------------------+
```

Friend status comes from `FRIEND_LIST_RESP` (pushed in welcome bundle,
updated via `FRIEND_PRESENCE_UPDATE` pushes).

Actions:
- **Join Game/Lobby**: sends `JOIN_FRIEND_GAME { friend_player_id }`.
  On success, navigates to LobbyScreen.
- **Message**: opens a minimal chat dialog. Sends `SEND_MESSAGE`.
- **Add Friend**: dialog with callsign text input, sends `FRIEND_REQUEST`.
- **Accept/Decline**: for pending requests, sent via `FRIEND_ACCEPT`/`FRIEND_REMOVE`.
- **Remove**: `FRIEND_REMOVE { player_id }`.

#### Tab: LAN

Local WiFi discovery (Phase 1 in NETWORKING_PLAN.md). Shows lobbies
discovered via UDP broadcast on port 42400.

```
+--------------------------------------------------------------+
| Local Network Games                                          |
+--------------------------------------------------------------+
| [Coop] Counterstrike!          PlayerOne    1/4   <1ms       |
| [Join]                                                       |
+--------------------------------------------------------------+
|                                                              |
| [Host a LAN Game]                                            |
+--------------------------------------------------------------+
```

LAN discovery requires the WiFi multicast lock and NEARBY_WIFI_DEVICES
permission. If permission is denied, show a message explaining why
it's needed.

LAN games bypass the matchmaking server entirely. Auto-join uses direct
IP addresses.

### Connection Status Bar

Persistent bar at the top of MultiplayerScreen showing:
- Own ping to matchmaking server (updated every 30s via WS ping/pong)
- Total online player count
- Connection status indicator (green dot = connected, yellow = reconnecting, red = disconnected)

### Lobby Screen

After joining a lobby (from any tab), navigate to the lobby detail screen.

```
+--------------------------------------------------------------+
| [<- Leave]     Counterstrike! (Coop)        [Settings]       |
+--------------------------------------------------------------+
| Players:                                                     |
| [Host] PlayerOne     -- (you)                Ready           |
| [ OK ] PlayerTwo     32ms  (direct)          Ready           |
| [ .. ] PlayerThree   ---   (connecting...)   Not Ready       |
+--------------------------------------------------------------+
| Server: 45ms                                                 |
+--------------------------------------------------------------+
| [Ready] / [Not Ready]           [Start Game] (host only)     |
+--------------------------------------------------------------+
| Chat:                                                        |
| PlayerTwo: glhf                                              |
| > [type message...]                            [Send]        |
+--------------------------------------------------------------+
```

Elements:
- Leave button: sends LEAVE_LOBBY, returns to MultiplayerScreen
- Player list with ready states, ping, connection type
- Ready toggle
- Start Game button (host only, enabled when conditions met)
- In-lobby chat (rate-limited SEND_MESSAGE scoped to lobby members)
- Settings gear (host only): opens game settings panel

On GAME_STARTING received: launch MainActivity with auto-join intent.
On leave/kick: return to MultiplayerScreen. The welcome bundle is
re-requested (or the client re-sends LIST_LOBBIES + FRIEND_LIST).

---

## MatchmakingService (Core Kotlin Class)

Singleton service that owns the WebSocket connection and manages state.

### State Model

```kotlin
data class MatchmakingState(
    val connectionStatus: ConnectionStatus,  // Connected, Reconnecting, Disconnected
    val ownPingMs: Int?,                     // our ping to the matchmaking server
    val onlinePlayers: Int,
    val activeGames: List<ActiveGameInfo>,
    val activeGamesTotal: Int,
    val lobbies: List<LobbyInfo>,
    val friends: List<FriendInfo>,
    val currentLobby: LobbyState?,          // non-null when inside a lobby
    val pendingMessages: List<ChatMessage>,
)

enum class ConnectionStatus { Disconnected, Connecting, Connected, Reconnecting }

data class LobbyState(
    val lobbyId: UUID,
    val players: List<LobbyPlayerInfo>,
    val isHost: Boolean,
    val settings: GameSettings?,
)
```

Exposed as `StateFlow<MatchmakingState>` for Compose observation.

### Lifecycle

```
App start
  -> SetupActivity creates MatchmakingService (lazy singleton)
  -> User taps "Multiplayer" button
  -> Service connects WebSocket + authenticates
  -> Receives welcome bundle, populates state
  -> User interacts with UI (join lobby, etc.)
  -> On game launch: service stays alive (foreground service)
  -> On game exit: service re-enters lobby/browse state
  -> On app exit: service disconnects
```

### Reconnection

If the WebSocket drops:
1. Set status to Reconnecting
2. Exponential backoff: 1s, 2s, 4s, 8s, max 30s
3. On reconnect: re-authenticate (server assigns new session but stable player_id)
4. Server re-sends welcome bundle
5. If the player was in a lobby, re-join it (if still exists)

### Message Dispatch

Incoming messages are dispatched by `type` tag:

```kotlin
when (message.type) {
    "AUTH_OK"              -> handleAuthOk(message)
    "SERVER_STATUS"        -> handleServerStatus(message)
    "LOBBY_LIST"           -> handleLobbyList(message)
    "FRIEND_LIST_RESP"     -> handleFriendList(message)
    "LOBBY_UPDATE"         -> handleLobbyUpdate(message)
    "GAME_STARTING"        -> handleGameStarting(message)
    "ERROR"                -> handleError(message)
    "MOTD"                 -> handleMotd(message)
    "MESSAGE_RECEIVED"     -> handleMessageReceived(message)
    "FRIEND_REQUEST_RECEIVED" -> handleFriendRequest(message)
    "FRIEND_PRESENCE_UPDATE"  -> handleFriendPresence(message)
    "CONNECTION_INFO"      -> handleConnectionInfo(message)
    "RATE_LIMITED"         -> handleRateLimited(message)
    "KICKED"               -> handleKicked(message)
    "PING"                 -> sendPong()  // WS-level, handled by OkHttp
    else                   -> log warning
}
```

---

## Google Play Games Sign-In Flow

### Setup

1. Create a Play Games project in Google Play Console
2. Configure OAuth consent screen
3. Generate OAuth 2.0 client IDs:
   - Android client ID (linked to app signing key SHA-1)
   - Web/server client ID (for server-side token exchange)
4. Add the Games project to the app's Play Console listing

### Client-Side Flow

```kotlin
// In SetupActivity or MatchmakingService:

// 1. Initialize (once, on app start)
PlayGamesSdk.initialize(context)

// 2. Check if already signed in
val gamesSignInClient = PlayGames.getGamesSignInClient(activity)
gamesSignInClient.isAuthenticated.addOnCompleteListener { task ->
    val isAuthenticated = task.result.isAuthenticated
    if (isAuthenticated) {
        // Already signed in, get server auth code
        requestServerAuthCode()
    } else {
        // Need to sign in (show UI if user taps Multiplayer)
        gamesSignInClient.signIn()
    }
}

// 3. Get server auth code (one-time token for our server)
fun requestServerAuthCode() {
    PlayGames.getGamesSignInClient(activity)
        .requestServerSideAccess(SERVER_CLIENT_ID, /* forceRefresh= */ false)
        .addOnSuccessListener { authCode ->
            // Send to our matchmaking server
            matchmakingService.authenticate(authCode, callsign, platform)
        }
}
```

The `authCode` is sent as the `play_games_token` in the AUTHENTICATE
message. The server exchanges it with Google's OAuth2 endpoint to get
the stable GPGS player ID.

Important: SERVER_CLIENT_ID is the *web* client ID from the Google API
console (type "Web application"), NOT the Android client ID. The Android
client ID is auto-linked by the SDK via the app's signing key SHA-1.

The authCode expires within minutes -- send it to the server immediately
after receiving it. Do not cache or defer. If the server exchange returns
"invalid_grant", the code has expired; request a fresh one with
forceRefresh=true.

### Fallback for Non-GPGS Devices

If PlayGamesSdk is unavailable (no Google Play Services):
- Generate a device-local keypair (Ed25519)
- Store in SharedPreferences (encrypted)
- Send the public key as the `play_games_token` with a special prefix
  (e.g., `"device:base64pubkey"`) so the server knows it's unverified
- Server assigns a stable player_id based on the public key hash
- Player shown with "unverified" badge in lobbies
- Host can set lobby to "verified only" to exclude unverified players

---

## Ping Display Logic

### Own Ping

Measured via WebSocket ping/pong frames:
- OkHttp's `WebSocketListener.onPong()` callback with timestamp
- Client sends a WS ping frame with a timestamp payload every 30 seconds
- On pong, compute RTT = now - timestamp
- Display in the status bar as "Your ping: Xms"

### Lobby Ping Estimate

For each lobby in the server browser:
- Server includes `host_ping_ms` in LobbyInfo (host's ping to server)
- Client knows its own ping from the status bar
- Display: "~{host_ping + own_ping}ms" as estimated worst-case latency
- Tooltip/details: "Host: {host_ping}ms + You: {own_ping}ms"

This is an upper bound (relay path). Direct connections would be faster.
But it gives players useful relative comparison between lobbies.

### In-Lobby Ping

Once in a lobby, real peer-to-peer ping measurement happens:
- If STUN/holepunch is complete, actual direct-path RTT is known
- Shown per-player in the lobby player list
- Updated via LOBBY_UPDATE messages (ping_ms field on LobbyPlayerInfo)

---

## Navigation Flow

```
SetupActivity
  |
  +-- [Multiplayer button]
  |     |
  |     v
  +-- MultiplayerScreen (connect to server, show tabs)
        |
        +-- [Join lobby from Server Browser or Friends]
        |     |
        |     v
        +-- LobbyScreen (inside the lobby)
        |     |
        |     +-- [Leave] -> back to MultiplayerScreen (re-fetch lists)
        |     |
        |     +-- [Game Starts] -> launch MainActivity with auto-join
        |     |     |
        |     |     +-- [Game ends / player leaves]
        |     |           |
        |     |           v
        |     |     back to MultiplayerScreen (re-fetch lists)
        |     |
        |     +-- [Kicked] -> back to MultiplayerScreen with toast
        |
        +-- [Host LAN Game from LAN tab]
        |     |
        |     v
        +-- LobbyScreen (as host)
        |     |
        |     +-- [Start Game] -> launch MainActivity with auto-host
        |
        +-- [Back] -> return to SetupActivity
```

When returning from a game or lobby, the client either:
- If WebSocket is still connected: sends LIST_LOBBIES + FRIEND_LIST to refresh
- If disconnected: reconnects and gets a fresh welcome bundle

---

## Files to Create

```
android/app/src/main/java/com/dxxredux/app/
    multiplayer/
        MatchmakingService.kt          -- WebSocket connection, state management
        MatchmakingState.kt            -- data classes for UI state
        MultiplayerScreen.kt           -- main tab screen (Compose)
        ServerBrowserTab.kt            -- lobby list tab
        ActiveGamesTab.kt              -- active games display tab
        FriendsTab.kt                  -- friends list + actions
        LanDiscoveryTab.kt             -- WiFi LAN discovery
        LobbyScreen.kt                 -- inside-lobby view
        ChatDialog.kt                  -- message compose dialog
        GameSettingsPanel.kt           -- host game settings (Phase 8)
        NetworkProtocol.kt             -- message data classes + serialization
        NetworkConstants.kt            -- protocol version, server URL
    lobby/
        LobbyService.kt               -- LAN UDP broadcast/listen (Phase 1)
        LobbyProtocol.kt              -- LAN lobby packet format
```

## Files to Modify

```
android/app/build.gradle                          -- add OkHttp, GPGS, kotlinx-serialization
android/app/src/main/AndroidManifest.xml           -- permissions
android/get_deps/tool_versions.conf                -- version pins
android/app/src/main/java/com/dxxredux/app/
    SetupActivity.kt                               -- add Multiplayer nav button
    MainActivity.kt                                -- auto-join/host intent handling
```

---

## Implementation Phases (Client Side)

### Phase C1: MatchmakingService + WebSocket Skeleton -- COMPLETE

- [x] Create NetworkConstants.kt with server URL, protocol version
- [x] Create NetworkProtocol.kt with sealed class message hierarchy
      matching the server's protocol.rs (using kotlinx-serialization)
- [x] Create MatchmakingService.kt: connect via OkHttp WebSocket,
      authenticate with a dev token, parse incoming JSON messages
- [x] Create MatchmakingState.kt with StateFlow
- [x] Integration test: connect to local server, verify AUTH_OK received
      (tested E2E on emulator, full auth flow confirmed via logcat)

### Phase C2: MultiplayerScreen (Server Browser) -- COMPLETE

- [x] Create MultiplayerScreen.kt with connection status, lobby list,
      status log (implemented as single-screen MVP, not tabbed yet)
- [x] Add ping display (host_ping_ms shown per lobby in LobbyCard)
- [x] Join button: send JOIN_LOBBY, navigate to LobbyScreen on LOBBY_UPDATE
- [x] Lobby code dialog for code-protected lobbies
- [x] Nav dispatch: MultiplayerNav.BROWSER vs LOBBY controls sub-screen
- [x] Wire into SetupActivity navigation
- [ ] Create ServerBrowserTab.kt: evolve to tabbed layout (deferred, not blocking)

### Phase C3: LobbyScreen -- COMPLETE

- [x] Create LobbyScreen.kt: player list, ready toggle, leave button
- [x] Handle LOBBY_UPDATE to refresh player list (populates CurrentLobbyState)
- [x] Handle GAME_STARTING (logged, kept for future game launch)
- [x] Handle KICKED/ERROR to return to MultiplayerScreen
- [x] Host-only: Start Game button (enabled when all ready + 2+ players), kick player
- [x] Player cards show callsign, host/you badges, ready status, ping
- [x] MatchmakingService: joinLobby(), leaveLobby(), setReady(), startGame(), kickPlayer()
- [x] Server integration tests: 5 new tests covering full join/ready/start, leave, kick,
      non-host start rejection, ready toggle (48 integration tests total)

### Phase C4: Friends Tab

- [ ] Create FriendsTab.kt: display friend list with presence
- [ ] Add Friend dialog (callsign search)
- [ ] Accept/decline pending requests
- [ ] Join friend's game/lobby
- [ ] Friend presence updates (handle FRIEND_PRESENCE_UPDATE push)

### Phase C4a: NAT Traversal (Client Side) -- NOT STARTED

See detailed section below (after Phase C9) for full implementation plan
including STUN client, connectivity checking, localhost proxy, message
flows, and error handling.

- [ ] StunClient.kt: STUN Binding Request/Response parser, NAT type detection
- [ ] ConnectivityChecker.kt: candidate pair race with probe send/recv
- [ ] LocalhostProxy.kt: per-peer UDP forwarding coroutines + relay wrapping
- [ ] NatTraversal.kt: orchestrator wiring STUN -> candidates -> checks -> proxy
- [ ] Wire into MatchmakingService message dispatch
- [ ] Add protocol messages to NetworkProtocol.kt (StunResult, PeerCandidates,
      ConnectivityCheckGo, ConnectivityOk, ConnectionCandidate, CandidatePair)
- [ ] LobbyScreen: show connection type and ping per player

### Phase C4b: Game Launch from Lobby -- NOT STARTED

See detailed section below (after Phase C4a) for full implementation plan
including JNI interface, engine menu bypass, multiplayer status overlay,
host/join flows, and shared constants.

- [ ] auto_net.c/h in d2/ and d1/: auto_join_pending, auto_host_pending globals,
      check_auto_join(), check_auto_host()
- [ ] jni_auto_net.c: nativeSetAutoJoin, nativeSetAutoHost JNI methods
- [ ] MainActivity.kt: read mp_* intent extras, call JNI before startGame()
- [ ] MultiplayerStatusOverlay.kt: Canvas-based overlay for connect/sync status
- [ ] Engine integration: hook check_auto_join/host into main menu handler
- [ ] Engine: add notify_mp_status() C-to-Kotlin callbacks at state transitions
- [ ] Engine: bind to loopback when auto_join/host_pending (internet mode)

### Phase C5: Google Play Games Sign-In

- [ ] Add GPGS dependency
- [ ] Implement sign-in flow in MatchmakingService
- [ ] Replace dev token with real server auth code
- [ ] Handle non-GPGS fallback (device keypair)

### Phase C6: Active Games + Status

- [ ] Create ActiveGamesTab.kt: display-only game list
- [ ] Handle SERVER_STATUS message
- [ ] Periodic refresh (server push or client poll every 60s)

### Phase C7: Player Messaging -- COMPLETE

- [x] Handle SEND_MESSAGE / MESSAGE_RECEIVED / MESSAGE_SENT / CONNECTION_INFO
- [x] sendLobbyChat() broadcasts to all lobby players
- [x] In-lobby chat area in LobbyScreen (LobbyChatArea)
- [x] Create Lobby UI dialog in MultiplayerScreen (CreateLobbyDialog)
- [x] Chat state cleared on disconnect/leave/kick
- [x] 2 new server integration tests (test_lobby_chat_flow, test_create_lobby_details)
- [ ] Rate limit indicator (show "slow down" when RATE_LIMITED received) -- future

### Phase C-Test: Two-Client Visibility Testing -- COMPLETE

- [x] Fix identity collision: two dev clients using the same hardcoded
      "dev-token" silently overwrote each other's sessions. Now each app
      instance generates a unique UUID-based devToken.
      (MatchmakingService.kt: `devToken = "dev-${UUID.randomUUID()}"`)
- [x] Auto-refresh lobby list every 5 seconds when connected
      (MultiplayerScreen.kt LaunchedEffect)
- [x] PowerShell WebSocket test bot (test_bot_client.ps1):
      connects to server, creates/joins lobbies, auto-replies to chat.
      Supports list/create/join/idle actions.
      Key fix: .NET cancels WebSocket on CancellationToken timeout; bot
      now uses blocking receive (no timeout) for main loops.
- [x] Server integration test: test_two_client_discovery_join_chat
      (two clients connect, one creates lobby, other discovers + joins,
      bidirectional chat, leave, lobby state verified -- 51 tests total)
- [x] Live-tested: bot creates lobby on host, emulator connects from
      Android, emulator sees bot's lobby, joins, bidirectional chat works

### Phase C8: LAN Discovery

- [ ] Create LanDiscoveryTab.kt
- [ ] Create LobbyService.kt: DatagramSocket on port 42400, broadcast
      LOBBY_ANNOUNCE every 3 seconds, listen for responses
- [ ] Runtime permission handling (NEARBY_WIFI_DEVICES / FINE_LOCATION)
- [ ] Acquire WifiManager.MulticastLock during discovery
- [ ] Host LAN Game button + LAN lobby flow
- [ ] Direct auto-join (no localhost proxy needed for LAN)

### Phase C9: Connection Resilience

- [ ] Reconnection with exponential backoff
- [ ] Re-join lobby after reconnect (if lobby still exists)
- [ ] Foreground service to maintain connection during game
- [ ] Handle MAINTENANCE / MAINTENANCE_WARNING messages

---

## Testing Strategy

### Unit Tests (Kotlin, JVM-only)

- NetworkProtocol.kt serialization/deserialization roundtrip
- MatchmakingState transitions (connect, receive welcome bundle, join lobby)
- Ping calculation logic

### Integration Tests (Instrumented, on emulator)

- Connect to a test server instance (started by the test harness)
- Full flow: authenticate -> browse lobbies -> join -> ready -> start
- Friend flow: request -> accept -> see presence -> join game
- Reconnection: kill WebSocket, verify reconnect + state restore
- Permission handling: deny NEARBY_WIFI_DEVICES, verify graceful fallback

### Interop Tests (Client + Server)

- Start the Rust server (`cargo run`) + Android emulator
- Run through the full multiplayer flow
- Verify welcome bundle contents match server state
- Verify lobby join/leave updates are consistent
- Use test_bot_client.ps1 as a second client to test two-player scenarios:
  ```powershell
  # Start bot as lobby host, then connect from emulator
  .\android\test_bot_client.ps1 -Action create -Callsign BotHost
  # Or join an emulator-created lobby
  .\android\test_bot_client.ps1 -Action join -Callsign BotJoiner
  ```

---

---

## Phase C4a: NAT Traversal -- Client Side (Detailed)

This section covers the full client-side NAT traversal pipeline: what
software is needed, which messages flow between client and server at
each step, and what happens on success/failure at every point.

### Software / Libraries Needed (Client Side)

| Component            | Implementation              | Notes                                  |
|----------------------|-----------------------------|----------------------------------------|
| STUN client          | Hand-rolled Kotlin (~60 lines) | RFC 5389 Binding Request is 20 bytes fixed format. Parse XOR-MAPPED-ADDRESS from response. No library needed. |
| UPnP port mapping    | Hand-rolled SSDP+SOAP (~200 lines) or skip for v1 | Android has no built-in UPnP. `cling`/`jupnp` are heavy. Minimal SSDP discovery + single SOAP AddPortMapping call. |
| UDP sockets          | `java.net.DatagramSocket`   | Standard Android. One per peer for localhost proxy. |
| Localhost proxy      | Kotlin coroutines           | One coroutine per peer, forwarding between loopback and real socket. |
| NAT keepalive        | Kotlin coroutine timer      | 1-byte UDP ping every 15s per active NAT mapping. |

### Software / Libraries Needed (Server Side) -- Already Implemented

| Component            | Implementation              | Status    |
|----------------------|-----------------------------|-----------|
| STUN result storage  | ws_handler.rs StunResult    | Done      |
| Predicted port calc  | generate_predicted_candidates() | Done  |
| Candidate distribution | PEER_CANDIDATES broadcast | Done      |
| Connectivity orchestration | build_connectivity_check_messages() | Done |
| Relay session allocation | relay.rs RelaySession    | Done      |
| Connection tracking  | lobby.rs LobbyPlayer fields | Done      |

### Client-Side STUN Query Implementation

The client performs two STUN Binding Requests to detect its public
address and NAT type. This happens automatically when a player enters
a lobby (or when they create one as host).

```
STUN Binding Request (20 bytes, fixed format):
  [00 01]              Message Type: Binding Request
  [00 00]              Message Length: 0 (no attributes)
  [21 12 A4 42]        Magic Cookie (RFC 5389)
  [... 12 bytes ...]   Transaction ID (random)

STUN Binding Response contains XOR-MAPPED-ADDRESS attribute:
  Attribute type: 0x0020
  Port XOR'd with magic cookie upper 16 bits
  IP XOR'd with magic cookie (IPv4) or magic+txn_id (IPv6)
```

Implementation:

```kotlin
// StunClient.kt (~60 lines)
data class StunResult(val publicAddr: InetSocketAddress, val localAddr: InetSocketAddress)

suspend fun queryStun(
    stunServer: InetSocketAddress,
    localSocket: DatagramSocket
): StunResult? {
    // Build 20-byte Binding Request
    val txnId = ByteArray(12).also { SecureRandom().nextBytes(it) }
    val request = ByteBuffer.allocate(20).apply {
        putShort(0x0001.toShort())  // Binding Request
        putShort(0x0000.toShort())  // length = 0
        putInt(0x2112A442)          // magic cookie
        put(txnId)
    }.array()

    // Send and receive with 2s timeout
    localSocket.soTimeout = 2000
    localSocket.send(DatagramPacket(request, 20, stunServer))
    val buf = ByteArray(128)
    val resp = DatagramPacket(buf, 128)
    localSocket.receive(resp)  // throws SocketTimeoutException on failure

    // Parse XOR-MAPPED-ADDRESS from response
    // ... extract port ^ 0x2112, ip ^ 0x2112A442
    return StunResult(publicAddr, localSocket.localSocketAddress)
}
```

NAT type detection:

```kotlin
val socket = DatagramSocket()  // bind once, reuse for both queries
val stun1 = queryStun(InetSocketAddress("stun.l.google.com", 19302), socket)
val stun2 = queryStun(InetSocketAddress("stun.cloudflare.com", 3478), socket)

val natType = when {
    stun1 == null || stun2 == null -> "unknown"
    stun1.publicAddr == stun2.publicAddr -> "full_cone"  // or restricted, can't tell without more
    stun1.publicAddr.port == stun2.publicAddr.port -> "address_restricted"
    abs(stun2.publicAddr.port - stun1.publicAddr.port) <= 3 -> "symmetric_sequential"
    else -> "symmetric"
}
```

Distinguishing full_cone vs port_restricted requires a third query from
a different server IP to the same port. For our purposes, the distinction
doesn't matter -- holepunch works the same for all cone types. The
critical distinction is cone vs symmetric.

### Candidate Gathering (Client Side)

After STUN queries, the client assembles its candidate list:

```kotlin
fun gatherCandidates(
    stunResults: Pair<StunResult?, StunResult?>,
    natType: String,
    upnpMappedPort: Int?   // from UPnP, null if not available
): List<ConnectionCandidate> {
    val candidates = mutableListOf<ConnectionCandidate>()

    // 1. Host candidate: local LAN address
    val localAddr = getWifiIpAddress()  // from WifiManager
    candidates.add(ConnectionCandidate("host", "$localAddr:${UDP_PORT}"))

    // 2. Server-reflexive: STUN result (if available)
    stunResults.first?.let {
        candidates.add(ConnectionCandidate("srflx", "${it.publicAddr}"))
    }

    // 3. UPnP-mapped port (if available, v2+)
    upnpMappedPort?.let {
        val publicIp = stunResults.first?.publicAddr?.address ?: return@let
        candidates.add(ConnectionCandidate("upnp", "$publicIp:$it"))
    }

    // 4. Predicted candidates are generated server-side from our STUN
    //    results. We don't add them here; the server generates them in
    //    generate_predicted_candidates() and distributes via PEER_CANDIDATES.

    return candidates
}
```

### Full Message Flow: Success Path

```
  Client A (Joiner)          Matchmaking Server           Client B (Host)
       |                            |                           |
       |  [enters lobby]            |                           |
       |                            |                           |
       | --- STUN queries to Google/Cloudflare (UDP, direct) ---|
       | (parallel, 2 queries from same socket)                 |
       |                            |                           |
       | STUN_RESULT {              |                           |
       |   candidates: [            |                           |
       |     {host, 192.168.1.50:42424},                        |
       |     {srflx, 73.22.19.44:62014}                         |
       |   ],                       |                           |
       |   nat_type: "port_restricted_cone"                     |
       | }                          |                           |
       |--------------------------->|                           |
       |                            |                           |
       |                            |  (B already sent STUN_RESULT earlier)
       |                            |                           |
       |                            |  Server checks: all players
       |                            |  have candidates?
       |                            |  YES -> transition Waiting
       |                            |         to Holepunching
       |                            |                           |
       |  PEER_CANDIDATES {         |  PEER_CANDIDATES {        |
       |    peer_id: B,             |    peer_id: A,            |
       |    candidates: [B's list], |    candidates: [A's list],|
       |    nat_type: B's type      |    nat_type: A's type     |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |                            |  Server calls             |
       |                            |  build_connectivity_check_messages()
       |                            |  generates CandidatePair lists
       |                            |  sorted by priority       |
       |                            |                           |
       |  CONNECTIVITY_CHECK_GO {   |  CONNECTIVITY_CHECK_GO {  |
       |    peer_addrs: [           |    peer_addrs: [          |
       |      {peer_id:B,           |      {peer_id:A,          |
       |       local:host,          |       local:host,         |
       |       remote:host,         |       remote:host,        |
       |       remote_addr:B_lan,   |       remote_addr:A_lan,  |
       |       priority:100},       |       priority:100},      |
       |      {peer_id:B,           |      {peer_id:A,          |
       |       local:srflx,         |       local:srflx,        |
       |       remote:srflx,        |       remote:srflx,       |
       |       remote_addr:B_pub,   |       remote_addr:A_pub,  |
       |       priority:80},        |       priority:80},       |
       |    ]                       |    ]                      |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  --- connectivity check race (both sides, simultaneous) ---
       |                            |                           |
       |  For each CandidatePair in priority order:             |
       |    Send 4-byte test packet to remote_addr              |
       |    Wait up to 500ms for response                       |
       |    If bidirectional: this pair wins                     |
       |    If timeout: try next pair                           |
       |  Total timeout: 3 seconds                              |
       |                            |                           |
       |  (assume srflx<->srflx wins at priority 80)            |
       |                            |                           |
       | CONNECTIVITY_OK {          |  CONNECTIVITY_OK {        |
       |   peer_id: B,              |    peer_id: A,            |
       |   winning_candidate_type:  |    winning_candidate_type:|
       |     "srflx",               |      "srflx",            |
       |   rtt_ms: 32               |    rtt_ms: 31            |
       | }                          |  }                        |
       |--------------------------->|<--------------------------|
       |                            |                           |
       |                            |  Server updates           |
       |                            |  connection_type and      |
       |                            |  ping_ms for both players |
       |                            |                           |
       |  CONNECTION_INFO {         |  CONNECTION_INFO {        |
       |    connections: [{         |    connections: [{        |
       |      peer_id: B,           |      peer_id: A,          |
       |      method: "direct_holepunch",                       |
       |      server_relay: false,  |      server_relay: false, |
       |      estimated_latency_ms: 32                          |
       |    }]                      |    }]                     |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  Lobby UI updates:         |  Lobby UI updates:        |
       |  "PlayerB 32ms (direct)"   |  "PlayerA 31ms (direct)" |
```

### Full Message Flow: Failure Path (Holepunch Fails, Relay Fallback)

```
  Client A (Joiner)          Matchmaking Server           Client B (Host)
       |                            |                           |
       |  [STUN queries]            |  [STUN queries]           |
       |                            |                           |
       | STUN_RESULT {              |  STUN_RESULT {            |
       |   candidates: [host, srflx],  candidates: [host, srflx],
       |   nat_type: "symmetric"    |    nat_type: "symmetric"  |
       | }                          |  }                        |
       |--------------------------->|<--------------------------|
       |                            |                           |
       |                            |  (Both symmetric -> server generates
       |                            |   predicted candidates. Distributes
       |                            |   PEER_CANDIDATES including predicted.)
       |                            |                           |
       |  CONNECTIVITY_CHECK_GO     |  CONNECTIVITY_CHECK_GO    |
       |  (includes host, srflx,    |  (includes host, srflx,   |
       |   predicted, relay pairs)  |   predicted, relay pairs) |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  --- connectivity check race ---                       |
       |  host<->host: timeout (different subnets)              |
       |  srflx<->srflx: timeout (symmetric NAT, wrong ports)  |
       |  predicted<->predicted: timeout (prediction wrong)     |
       |  Total 3s timeout reached                              |
       |                            |                           |
       |  (No pair succeeded. Client falls back to relay.)      |
       |                            |                           |
       | CONNECTIVITY_OK {          |  CONNECTIVITY_OK {        |
       |   peer_id: B,              |    peer_id: A,            |
       |   winning_candidate_type:  |    winning_candidate_type:|
       |     "relay",               |      "relay",             |
       |   rtt_ms: 0                |    rtt_ms: 0              |
       | }                          |  }                        |
       |--------------------------->|<--------------------------|
       |                            |                           |
       |                            |  Server sets connection_type
       |                            |  = Relay for this pair    |
       |                            |                           |
       |  CONNECTION_INFO {         |  CONNECTION_INFO {        |
       |    connections: [{         |    connections: [{        |
       |      peer_id: B,           |      peer_id: A,          |
       |      method: "relay",      |      method: "relay",     |
       |      server_relay: true,   |      server_relay: true,  |
       |      estimated_latency_ms: null                        |
       |    }]                      |    }]                     |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  Lobby UI: "PlayerB (relay)"| Lobby UI: "PlayerA (relay)"|
```

### Full Message Flow: STUN Failure

```
  Client A                  Matchmaking Server
       |                            |
       |  STUN query to Google      |
       |  -> SocketTimeoutException |
       |  STUN query to Cloudflare  |
       |  -> SocketTimeoutException |
       |                            |
       |  (Both STUN queries failed.|
       |   Can't determine public   |
       |   address. Send host-only  |
       |   candidate.)              |
       |                            |
       | STUN_RESULT {              |
       |   candidates: [            |
       |     {host, 192.168.1.50:42424}  |
       |   ],                       |
       |   nat_type: "unknown"      |
       | }                          |
       |--------------------------->|
       |                            |
       |  (Server proceeds. Host candidate still allows LAN
       |   connections. For internet peers, only relay will work.)
```

When STUN fails, the client still participates but is guaranteed to
need relay for any internet connection. The connectivity check race
will try host<->host (succeeds on LAN) and immediately fall back to
relay for internet peers.

### Full Message Flow: Game Start (After Holepunch)

```
  Client A (Joiner)          Matchmaking Server           Client B (Host)
       |                            |                           |
       |  (holepunch complete,      |  (holepunch complete,     |
       |   lobby shows connected)   |   lobby shows connected)  |
       |                            |                           |
       |                            |  Host clicks "Start Game" |
       |                            |  START_GAME {}            |
       |                            |<--------------------------|
       |                            |                           |
       |                            |  Server:                  |
       |                            |  1. Set lobby.state = Starting
       |                            |  2. For relay pairs: allocate
       |                            |     RelaySession (token, addrs)
       |                            |  3. send_connection_info() to all
       |                            |  4. Send GAME_STARTING to all
       |                            |                           |
       |  RELAY_ASSIGNED {          |  (only sent if this pair  |
       |    relay_addr: "relay.example.com:9001",               |
       |    session_token: "3847291056"                          |
       |  }                         |                           |
       |<---------------------------|  (only if relay needed)   |
       |                            |                           |
       |  CONNECTION_INFO {         |  CONNECTION_INFO {        |
       |    connections: [{         |    connections: [{        |
       |      peer_id: B,           |      peer_id: A,          |
       |      method, detail,       |      method, detail,      |
       |      server_relay, latency |      server_relay, latency|
       |    }]                      |    }]                     |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  GAME_STARTING {           |  GAME_STARTING {          |
       |    host_addr: "73.x.x.x:42424",                       |
       |    mission: "Counterstrike!",                          |
       |    mode: "coop"            |    mode: "coop"           |
       |  }                         |  }                        |
       |<---------------------------|-------------------------->|
       |                            |                           |
       |  Client: handleGameStarting()                          |
       |  1. Set up localhost proxy (see below)                 |
       |  2. Launch MainActivity with auto-join intent          |
       |  Host: handleGameStarting()                            |
       |                            |  1. Set up localhost proxy|
       |                            |  2. Launch MainActivity   |
       |                            |     with auto-host intent |
```

### Connectivity Check Implementation (Client Side)

The connectivity check is the core of the holepunch process. On
receiving CONNECTIVITY_CHECK_GO, the client runs a race across all
candidate pairs in priority order.

```kotlin
// ConnectivityChecker.kt

data class CheckResult(
    val peerIdToWinningType: Map<UUID, String>,  // peer_id -> "host"|"srflx"|...
    val peerIdToRtt: Map<UUID, Int>,
    val peerIdToAddr: Map<UUID, InetSocketAddress>,  // winning resolved address
)

suspend fun runConnectivityChecks(
    socket: DatagramSocket,         // the same socket used for STUN
    candidatePairs: List<CandidatePair>,
    totalTimeoutMs: Long = 3000
): CheckResult {
    // Group pairs by peer_id, sorted by priority (descending = best first)
    val peerGroups = candidatePairs.groupBy { it.peerId }
        .mapValues { (_, pairs) -> pairs.sortedByDescending { it.priority } }

    val results = ConcurrentHashMap<UUID, Pair<String, Int>>() // type, rtt
    val resolvedAddrs = ConcurrentHashMap<UUID, InetSocketAddress>()

    // Test packet: 4-byte magic + 8-byte timestamp
    val magic = byteArrayOf(0xD2.toByte(), 0xCC.toByte(), 0x01, 0x00)

    withTimeoutOrNull(totalTimeoutMs) {
        // Launch parallel checks per peer
        peerGroups.map { (peerId, pairs) ->
            launch {
                for (pair in pairs) {
                    if (results.containsKey(peerId)) break  // already found

                    val addr = parseAddress(pair.remoteAddr)
                    val rtt = probeAddr(socket, addr, magic, timeoutMs = 500)
                    if (rtt != null) {
                        results[peerId] = Pair(pair.remoteType, rtt)
                        resolvedAddrs[peerId] = addr
                        break
                    }
                }
            }
        }.joinAll()
    }

    // Any peer without a result falls back to relay
    for ((peerId, _) in peerGroups) {
        if (!results.containsKey(peerId)) {
            results[peerId] = Pair("relay", 0)
        }
    }

    return CheckResult(
        peerIdToWinningType = results.mapValues { it.value.first },
        peerIdToRtt = results.mapValues { it.value.second },
        peerIdToAddr = resolvedAddrs
    )
}

// Send a probe and wait for echo response
private suspend fun probeAddr(
    socket: DatagramSocket,
    addr: InetSocketAddress,
    magic: ByteArray,
    timeoutMs: Int
): Int? {
    val sendTime = System.nanoTime()
    val payload = ByteBuffer.allocate(12).apply {
        put(magic)
        putLong(sendTime)
    }.array()
    socket.send(DatagramPacket(payload, 12, addr))

    // Listen for response (the peer sends back our magic bytes)
    val buf = ByteArray(12)
    socket.soTimeout = timeoutMs
    return try {
        socket.receive(DatagramPacket(buf, 12))
        if (buf.sliceArray(0..3).contentEquals(magic)) {
            ((System.nanoTime() - sendTime) / 1_000_000).toInt()
        } else null
    } catch (e: SocketTimeoutException) { null }
}
```

Both sides must be listening and sending simultaneously. Each side:
1. Receives CONNECTIVITY_CHECK_GO from server
2. Starts a listener coroutine on its STUN socket
3. Sends probe packets to each candidate address
4. When a probe arrives, echoes it back (so the sender can measure RTT)
5. When an echo of our probe arrives, that pair is confirmed working

The test packet format:
- Bytes 0-3: magic `0xD2CC0100` (identifies our connectivity check)
- Bytes 4-11: sender timestamp (for RTT calculation)

On receiving a packet starting with the magic bytes, the receiver
echoes the full 12 bytes back. The original sender sees the echo,
confirms the pair works, and records RTT from the embedded timestamp.

### Localhost Proxy Implementation (Client Side)

The localhost proxy bridges the game engine (which uses fixed UDP
port 42424 on localhost) to real peer addresses (direct or relay).

Architecture:

```
Game Engine                Localhost Proxy (Kotlin)           Network
  UDP_Socket[0]              per-peer coroutines
  bound to :42424                                          direct/relay addrs
       |                                                         |
       | sendto(127.0.0.1:42430)                                 |
       |-----> [proxy peer 0] -----> sendto(peer0_real_addr) --->|
       |                                                         |
       | sendto(127.0.0.1:42431)                                 |
       |-----> [proxy peer 1] -----> sendto(peer1_real_addr) --->|
       |                                                         |
       |                     recv from real socket                |
       |<----- [proxy peer 0] <----- recvfrom(peer0) <----------|
       |                                                         |
  recvfrom returns                                               |
  source = 127.0.0.1:42430                                      |
  (engine thinks it's peer 0)                                    |
```

For relay connections, the proxy wraps/unwraps relay headers:

```
Game packet going to peer via relay:
  Engine sends: [game_packet] to 127.0.0.1:42430
  Proxy wraps:  [session_token:4LE][dest_player:1][game_packet]
  Proxy sends to: relay_server:9001

Relay packet arriving from peer:
  Proxy recvs:  [session_token:4LE][from_player:1][game_packet]
  Proxy unwraps: [game_packet]
  Proxy sends to: 127.0.0.1:42424 (game engine's listening socket)
```

Implementation:

```kotlin
// LocalhostProxy.kt

class LocalhostProxy(private val scope: CoroutineScope) {
    private val proxies = mutableListOf<PeerProxy>()

    fun addPeer(
        peerIndex: Int,
        realAddr: InetSocketAddress,
        isRelay: Boolean,
        relayToken: UInt? = null,
        relayPlayerSlot: UByte? = null
    ) {
        val localPort = 42430 + peerIndex
        val proxy = PeerProxy(localPort, realAddr, isRelay, relayToken, relayPlayerSlot)
        proxies.add(proxy)
        scope.launch { proxy.run() }
    }

    fun shutdown() {
        proxies.forEach { it.close() }
    }
}

class PeerProxy(
    private val localPort: Int,
    private val realAddr: InetSocketAddress,
    private val isRelay: Boolean,
    private val relayToken: UInt?,
    private val relayPlayerSlot: UByte?
) {
    private val localSocket = DatagramSocket(localPort, InetAddress.getLoopbackAddress())
    private val realSocket = DatagramSocket()  // ephemeral port, for outbound

    suspend fun run() = coroutineScope {
        // Forward: local -> real
        launch {
            val buf = ByteArray(MAX_PACKET_SIZE)
            while (isActive) {
                val pkt = DatagramPacket(buf, buf.size)
                localSocket.receive(pkt)
                if (isRelay) {
                    // Wrap with relay header: [token:4LE][dest:1][payload]
                    val wrapped = ByteBuffer.allocate(5 + pkt.length).apply {
                        order(ByteOrder.LITTLE_ENDIAN)
                        putInt(relayToken!!.toInt())
                        put(relayPlayerSlot!!.toByte())
                        put(pkt.data, 0, pkt.length)
                    }.array()
                    realSocket.send(DatagramPacket(wrapped, wrapped.size, realAddr))
                } else {
                    realSocket.send(DatagramPacket(pkt.data, pkt.length, realAddr))
                }
            }
        }
        // Forward: real -> local
        launch {
            val buf = ByteArray(MAX_PACKET_SIZE)
            val engineAddr = InetSocketAddress(InetAddress.getLoopbackAddress(), ENGINE_PORT)
            while (isActive) {
                val pkt = DatagramPacket(buf, buf.size)
                realSocket.receive(pkt)
                val payload = if (isRelay) {
                    // Unwrap relay header: skip [token:4][from:1]
                    pkt.data.copyOfRange(5, pkt.length)
                } else {
                    pkt.data.copyOfRange(0, pkt.length)
                }
                localSocket.send(DatagramPacket(payload, payload.size, engineAddr))
            }
        }
        // NAT keepalive: send 1-byte ping every 15s
        if (!isRelay) {
            launch {
                while (isActive) {
                    delay(15_000)
                    realSocket.send(DatagramPacket(byteArrayOf(0), 1, realAddr))
                }
            }
        }
    }

    fun close() {
        localSocket.close()
        realSocket.close()
    }

    companion object {
        const val MAX_PACKET_SIZE = 1500
        const val ENGINE_PORT = 42424
    }
}
```

### Localhost Proxy: Telling the Engine About Peer Addresses

The engine normally gets peer addresses from network communication
(UPID_REQUEST/UPID_GAMEINFO handshake). With the localhost proxy, we
need the engine to think peers are at 127.0.0.1:4243x. There are two
approaches:

**Approach A (preferred): Let the engine discover peers normally.**
The proxy is transparent. When the host broadcasts game info on its
real socket, the proxy intercepts and forwards to localhost:42424.
When the joiner sends UPID_REQUEST, the proxy forwards it to the host's
real address. The engine sees the source as 127.0.0.1:4243x and stores
that as the peer address. All subsequent traffic goes through the proxy
automatically.

This requires:
- The joiner's auto-join resolves the host address to 127.0.0.1:42430
  (the localhost proxy port for peer 0 = the host)
- The host's proxy listens for incoming packets from joiners on the
  real socket and maps each joiner to a localhost port

**Approach B (simpler for v1): Pre-fill and skip discovery.**
Set Netgame.players[N].protocol.udp.addr to 127.0.0.1:4243N via JNI
before the game opens sockets. The engine skips discovery and uses
the pre-filled addresses directly.

Approach A is more robust (matches existing engine flow). Approach B
is simpler but requires more invasive engine changes. Use Approach A
for v1.

### Engine Socket Binding: Port Conflict Avoidance

The engine binds UDP_Socket[0] to UDP_MyPort (default 42424). The
localhost proxy also needs ports for each peer (42430+N). These don't
conflict because:
- The engine binds to 0.0.0.0:42424 (all interfaces, or just loopback
  if we modify it)
- The proxy binds to 127.0.0.1:42430, 127.0.0.1:42431, etc.
- Different ports, no conflict

For the engine socket on Android internet play, we should bind to
127.0.0.1:42424 (loopback only) rather than 0.0.0.0:42424. This
prevents the engine from accidentally receiving real network traffic
that bypasses the proxy. This requires a one-line engine change:

```c
// In udp_open_socket(), for Android internet mode:
#ifdef __ANDROID__
    if (auto_join_pending || auto_host_pending) {
        sAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
        sAddr.sin_addr.s_addr = INADDR_ANY;
    }
#else
    sAddr.sin_addr.s_addr = INADDR_ANY;
#endif
```

### Timing and State Machine Summary

```
Player enters lobby
  |
  v
STUN queries (2x, parallel, 2s timeout each)
  |
  v
Send STUN_RESULT to server via WebSocket
  |
  v
Wait for server (all players must submit STUN_RESULT)
  |
  v
Receive PEER_CANDIDATES (peer addresses)
  |
  v
Receive CONNECTIVITY_CHECK_GO (candidate pair list)
  |
  v
Run connectivity check race (3s total timeout)
  |
  +-> Success: send CONNECTIVITY_OK with winning type + RTT
  |
  +-> All pairs timeout: send CONNECTIVITY_OK with type="relay"
  |
  v
Receive CONNECTION_INFO (final connection status per peer)
  |
  v
Lobby UI shows connection status per peer
  |
  v
Wait for host to click "Start Game"
  |
  v
Receive GAME_STARTING -> set up localhost proxy -> launch game
```

### Error Handling Summary

| Error                        | Consequence                           | Recovery                          |
|------------------------------|---------------------------------------|-----------------------------------|
| STUN timeout (both servers)  | No srflx candidate                    | Proceed with host-only, server assigns relay |
| STUN timeout (one server)   | Can't detect NAT type precisely       | Assume "unknown", proceed         |
| All connectivity checks fail | No direct path found                  | Fall back to relay (always works) |
| Relay token not received     | Can't use relay                       | Bug; show error, leave lobby      |
| WebSocket drops during check | Lost signaling channel                | Reconnect, re-join lobby, restart STUN |
| Proxy socket bind fails     | Can't bridge engine to network        | Fatal error, show toast, return to lobby |
| Peer disappears mid-check   | Connectivity check hangs for that peer| 3s timeout, relay fallback        |

### Files to Create (Phase C4a)

```
android/app/src/main/java/com/dxxredux/app/multiplayer/
    StunClient.kt             -- STUN Binding Request/Response, NAT type detection
    ConnectivityChecker.kt     -- candidate pair race, probe send/recv
    LocalhostProxy.kt         -- per-peer UDP forwarding coroutines
    NatTraversal.kt            -- orchestrator: gatherCandidates, handleStunResult,
                                  handleConnectivityCheckGo, handleGameStarting
```

### Files to Modify (Phase C4a)

```
android/app/src/main/java/com/dxxredux/app/multiplayer/
    MatchmakingService.kt      -- wire NatTraversal into message dispatch,
                                  start STUN on lobby join, store checkResult
    NetworkProtocol.kt         -- add StunResult, PeerCandidates,
                                  ConnectivityCheckGo, ConnectivityOk,
                                  ConnectionCandidate, CandidatePair messages
    LobbyScreen.kt             -- show connection type/ping per player
```

---

## Phase C4b: Game Launch from Lobby (Detailed)

This section covers how the lobby triggers the actual game, skipping all
menus, presenting a Kotlin multiplayer status overlay, and starting gameplay.

### Overview

When the server sends GAME_STARTING, the client:
1. Sets up the localhost proxy (Phase C4a) with peer addresses from
   the connectivity check results
2. Launches MainActivity with special intent extras for auto-join or
   auto-host
3. Shows a Kotlin overlay (Canvas-based, matching existing overlay system)
   with multiplayer status ("Connecting...", "Waiting for players...")
4. The game engine receives auto-join/host parameters via JNI, bypasses
   all menus, and goes directly to the join/host flow
5. When the engine reaches gameplay, the overlay transitions to the
   normal touch controls overlay

### Intent Extras for Multiplayer Launch

```kotlin
// In handleGameStarting() within MatchmakingService/NatTraversal:

fun launchGame(context: Context, gameStarting: GameStarting, isHost: Boolean) {
    val intent = Intent(context, MainActivity::class.java).apply {
        putExtra("game", "d2")  // or "d1", from lobby settings
        putExtra("mp_mode", if (isHost) "host" else "join")
        putExtra("mp_host_addr", "127.0.0.1")  // always localhost (proxy)
        putExtra("mp_host_port", 42430)         // proxy port for host peer
        putExtra("mp_my_port", 42424)           // engine's local port
        putExtra("mp_mission", gameStarting.mission)
        putExtra("mp_game_mode", gameStarting.mode)  // "coop", "anarchy", etc.
        putExtra("mp_difficulty", lobbySettings.difficulty) // 0-4
        putExtra("mp_max_players", lobbySettings.maxPlayers)
        putExtra("mp_level_num", lobbySettings.levelNum)
    }
    context.startActivity(intent)
}
```

### JNI Interface for Auto-Join / Auto-Host

New JNI methods in jni_main.c:

```c
// Called from MainActivity.kt before startGame()
// Sets globals that DoMenu()/main menu will check on first frame

static int auto_join_pending = 0;
static int auto_host_pending = 0;
static char auto_host_addr[128] = "";
static int auto_host_port = 42424;
static int auto_my_port = 42424;
static char auto_mission[64] = "";
static char auto_game_mode[16] = "";
static int auto_difficulty = 1;
static int auto_max_players = 4;
static int auto_level_num = 1;

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetAutoJoin(
    JNIEnv *env, jobject obj,
    jstring hostAddr, jint hostPort, jint myPort,
    jstring mission, jstring gameMode, jint difficulty)
{
    const char *addr = (*env)->GetStringUTFChars(env, hostAddr, NULL);
    const char *miss = (*env)->GetStringUTFChars(env, mission, NULL);
    const char *mode = (*env)->GetStringUTFChars(env, gameMode, NULL);
    strncpy(auto_host_addr, addr, sizeof(auto_host_addr)-1);
    auto_host_port = hostPort;
    auto_my_port = myPort;
    strncpy(auto_mission, miss, sizeof(auto_mission)-1);
    strncpy(auto_game_mode, mode, sizeof(auto_game_mode)-1);
    auto_difficulty = difficulty;
    auto_join_pending = 1;
    (*env)->ReleaseStringUTFChars(env, hostAddr, addr);
    (*env)->ReleaseStringUTFChars(env, mission, miss);
    (*env)->ReleaseStringUTFChars(env, gameMode, mode);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetAutoHost(
    JNIEnv *env, jobject obj,
    jint myPort, jstring mission, jstring gameMode,
    jint difficulty, jint maxPlayers, jint levelNum)
{
    const char *miss = (*env)->GetStringUTFChars(env, mission, NULL);
    const char *mode = (*env)->GetStringUTFChars(env, gameMode, NULL);
    auto_my_port = myPort;
    strncpy(auto_mission, miss, sizeof(auto_mission)-1);
    strncpy(auto_game_mode, mode, sizeof(auto_game_mode)-1);
    auto_difficulty = difficulty;
    auto_max_players = maxPlayers;
    auto_level_num = levelNum;
    auto_host_pending = 1;
    (*env)->ReleaseStringUTFChars(env, mission, miss);
    (*env)->ReleaseStringUTFChars(env, gameMode, mode);
}
```

### Engine-Side Menu Bypass

The game engine normally shows a chain of menus:

```
Main Menu -> Multiplayer -> UDP/IP -> Host Game -> Select Mission ->
  Configure Game -> Start Game -> Wait for Players -> StartNewLevel

Main Menu -> Multiplayer -> UDP/IP -> Join Game -> Enter Address ->
  Show Game Info -> Send Join Request -> Wait for Sync -> StartNewLevel
```

For auto-join, we bypass ALL of this and go directly to the connect+sync
flow. For auto-host, we bypass to the select_players wait screen.

Implementation in the main menu handler (checked once per frame):

```c
// In DoMenu() or game_handler() in d2/main/inferno.c:

void check_auto_join(void)
{
    if (!auto_join_pending) return;
    auto_join_pending = 0;

    // Initialize networking
    net_udp_init();

    // Set our port
    snprintf(UDP_MyPort, sizeof(UDP_MyPort), "%d", auto_my_port);

    // Open our socket
    if (udp_open_socket(0, auto_my_port) != 0) {
        con_printf(CON_URGENT, "Auto-join: failed to open socket on port %d\n", auto_my_port);
        return;
    }

    // Set up direct_join with the host address (localhost proxy)
    direct_join dj;
    memset(&dj, 0, sizeof(dj));
    strncpy(dj.addrbuf, auto_host_addr, sizeof(dj.addrbuf)-1);
    snprintf(dj.portbuf, sizeof(dj.portbuf), "%d", auto_host_port);
    udp_dns_filladdr(dj.addrbuf, atoi(dj.portbuf), &dj.host_addr);
    dj.connecting = 1;
    dj.start_time = timer_query();
    dj.last_time = 0;

    // Set difficulty
    Difficulty_level = auto_difficulty;

    // Enter the connect+join loop
    // This calls net_udp_game_connect() in a loop until connected or timeout
    net_udp_game_connect(&dj);

    // After game_connect succeeds, the engine proceeds to sync and StartNewLevel
}

void check_auto_host(void)
{
    if (!auto_host_pending) return;
    auto_host_pending = 0;

    // Initialize networking
    net_udp_init();

    // Set our port
    snprintf(UDP_MyPort, sizeof(UDP_MyPort), "%d", auto_my_port);

    // Configure the game
    Difficulty_level = auto_difficulty;
    Netgame.max_numplayers = auto_max_players;
    Netgame.levelnum = auto_level_num;
    // Set game mode from auto_game_mode string
    // "coop" -> GM_MULTI_COOP, "anarchy" -> 0, etc.
    Netgame.gamemode = parse_game_mode(auto_game_mode);

    // Select mission
    // net_udp_setup_game() normally does this interactively
    // For auto-host, we need to select the mission by name
    load_mission_by_name(auto_mission);

    // Start hosting
    net_udp_start_game();
    // This opens sockets, enters net_udp_select_players() blocking loop
    // When enough players join and host starts, calls StartNewLevel
}
```

The exact insertion point needs care. The engine's main loop runs
through `event_process()` -> window handlers. The cleanest approach is
to check `auto_join_pending` / `auto_host_pending` in the main menu's
handler function, before showing any menu UI:

```c
// In DoMenu() / newmenu_handler for the main menu:
int main_menu_handler(...)
{
    // Check for auto-join/host before processing normal menu events
    if (auto_join_pending) {
        check_auto_join();
        return 0;
    }
    if (auto_host_pending) {
        check_auto_host();
        return 0;
    }
    // ... normal menu handling
}
```

### Multiplayer Status Overlay (Kotlin)

During the join/host process, the game engine shows its own status
screens (net_udp_game_connect shows "Connecting...",
net_udp_select_players shows "Waiting for players..."). These are
text-based menus rendered by the engine.

We add a Kotlin overlay ON TOP of these screens to show a more
informative multiplayer status with modern UI. This overlay is part
of the existing FrameLayout stack in MainActivity:

```
GameSurfaceView (SDL rendering, shows game's own connect screen)
  |
TouchOverlayView (existing, hidden during menus)
  |
MultiplayerStatusOverlay (new, shown during connect/sync phase)
  |
overlayContainer (existing)
```

The overlay shows:
- Current state: "Connecting to host...", "Waiting for game sync...",
  "Loading level...", "Starting game..."
- Connection quality per peer (from pre-game holepunch results)
- Cancel button (leave lobby, return to MultiplayerScreen)
- Animated progress indicator

Implementation: Canvas-based view (matching existing TouchOverlayView),
added to FrameLayout in MainActivity.onCreate():

```kotlin
class MultiplayerStatusOverlay(context: Context) : View(context) {
    var status: String = "Connecting..."
    var peers: List<PeerStatus> = emptyList()
    var showCancel: Boolean = true

    override fun onDraw(canvas: Canvas) {
        // Semi-transparent background
        canvas.drawColor(Color.argb(180, 0, 0, 0))

        // Status text centered
        val paint = Paint().apply {
            color = Color.WHITE
            textSize = 48f
            textAlign = Paint.Align.CENTER
        }
        canvas.drawText(status, width / 2f, height / 2f - 100, paint)

        // Peer connection info
        peers.forEachIndexed { i, peer ->
            val y = height / 2f + i * 60
            val icon = if (peer.isRelay) "[R]" else "[D]"
            canvas.drawText(
                "$icon ${peer.callsign}: ${peer.pingMs}ms",
                width / 2f, y, paint
            )
        }

        // Cancel button at bottom
        if (showCancel) {
            canvas.drawText("[ Cancel ]", width / 2f, height - 200f, paint)
        }
    }
}
```

The overlay is:
- VISIBLE from the moment GAME_STARTING is received until the engine
  enters gameplay (in_game = true via introspection or JNI callback)
- Updated via JNI callbacks as the engine progresses through its
  connect/sync states (Network_status changes)
- HIDDEN once the game reaches the cockpit view, at which point the
  normal TouchOverlayView becomes visible

### Engine-to-Kotlin Status Callbacks

New JNI calls from C to Kotlin to report connection progress:

```c
// In net_udp.c, at key state transitions:

// When sending join request
void notify_mp_status(const char *status) {
#ifdef __ANDROID__
    jni_notify_mp_status(status);  // calls Kotlin
#endif
}

// Called at key points:
// net_udp_game_connect(): notify_mp_status("requesting_game_info")
// net_udp_send_request(): notify_mp_status("sending_join_request")
// net_udp_sync_poll():    notify_mp_status("waiting_for_sync")
// StartNewLevel():        notify_mp_status("loading_level")
// When game starts:       notify_mp_status("in_game")
```

The Kotlin side receives these and updates MultiplayerStatusOverlay:

```kotlin
// In MainActivity.kt:
fun onMultiplayerStatus(status: String) {
    runOnUiThread {
        multiplayerOverlay?.status = when (status) {
            "requesting_game_info" -> "Connecting to host..."
            "sending_join_request" -> "Requesting to join..."
            "waiting_for_sync"     -> "Synchronizing game state..."
            "loading_level"        -> "Loading level..."
            "in_game"              -> { multiplayerOverlay?.visibility = View.GONE; "" }
            else -> status
        }
        multiplayerOverlay?.invalidate()
    }
}
```

### Host-Side Flow (Auto-Host)

The host's flow is similar but has different engine calls:

1. GAME_STARTING received (host sent START_GAME, gets GAME_STARTING back)
2. Localhost proxy starts (one proxy per non-host peer)
3. MainActivity launches with mp_mode="host" intent
4. JNI: nativeSetAutoHost() sets globals
5. Engine checks auto_host_pending in main menu handler
6. Engine calls net_udp_start_game() which:
   - Opens socket on UDP_MyPort (127.0.0.1:42424, loopback)
   - Sets Netgame fields (mode, mission, level, etc.)
   - Enters net_udp_select_players() blocking loop
   - Waits for joiners to send UPID_REQUEST (via proxy)
   - When all expected players join, calls StartNewLevel
7. Multiplayer overlay shows "Waiting for players to join... (2/4)"
8. When level starts, overlay hides, touch controls appear

### Post-Game Return to Lobby

When the game ends (level complete, all players quit, or disconnect):

1. Engine exits to main menu (or exits process on Android)
2. Since Android uses process kill on exit, the return to lobby is
   handled by the Kotlin lifecycle:
   - MatchmakingService (foreground service) survives the process kill
   - When SetupActivity resumes, it reconnects to MatchmakingService
   - If the lobby still exists (server kept it during InGame state),
     the player can rejoin or the lobby transitions back to Waiting
3. For a cleaner experience (future optimization): keep the process
   alive, return to a "game results" overlay, then back to lobby

### Race Condition: Joiner Arrives Before Host is Ready

The host and joiners receive GAME_STARTING simultaneously, but the
host might take longer to start (loading mission, opening socket).
If a joiner sends UPID_REQUEST before the host's socket is open, it
gets no response.

This is handled by the existing engine retry logic:
- net_udp_game_connect() retries UPID_REQUEST every 1 second
- Timeout is 10 seconds
- The host typically starts within 1-2 seconds
- The localhost proxy queues/discards packets that arrive before
  the host socket is ready (the proxy itself starts before the engine)

### Shared Constants (Kotlin <-> C)

These constants must match between Kotlin and C code. Document here
so both copies can be maintained:

```
UDP_PORT_DEFAULT = 42424      // d2/main/net_udp.c, NetworkConstants.kt
PROXY_PORT_BASE = 42430       // LocalhostProxy.kt, new #define in net_udp.c
CONNECTIVITY_MAGIC = 0xD2CC0100  // ConnectivityChecker.kt, new in net_udp.c
                                 // (only used between Kotlin peers, not engine)
```

### Files to Create (Phase C4b)

```
android/app/src/main/java/com/dxxredux/app/
    MultiplayerStatusOverlay.kt  -- Canvas-based overlay for connect/sync status

d2/main/auto_net.c               -- check_auto_join(), check_auto_host(),
                                    auto_join/host globals, notify_mp_status()
d2/main/auto_net.h               -- extern declarations for auto_net globals
d1/main/auto_net.c               -- same as d2 (duplicated per project rules)
d1/main/auto_net.h               -- same as d2

android/app/src/main/jni/jni_auto_net.c  -- JNI wrappers: nativeSetAutoJoin,
                                            nativeSetAutoHost, jni_notify_mp_status
```

### Files to Modify (Phase C4b)

```
android/app/src/main/java/com/dxxredux/app/
    MainActivity.kt               -- read mp_* intent extras, create overlay,
                                     call nativeSetAutoJoin/Host before startGame()
    multiplayer/
        MatchmakingService.kt     -- handleGameStarting() sets up proxy, launches game
        NatTraversal.kt           -- launchGame() builds intent with mp_* extras

d2/main/inferno.c                 -- #include "auto_net.h", call check_auto_join/host
                                     in main menu handler
d2/main/net_udp.c                 -- add notify_mp_status() calls at state transitions,
                                     bind to loopback when auto_join/host_pending
d1/main/inferno.c                 -- same changes as d2
d1/main/net_udp.c                 -- same changes as d2

android/app/CMakeLists.txt        -- add jni_auto_net.c to build
d2/CMakeLists.txt                 -- add auto_net.c to build
d1/CMakeLists.txt                 -- add auto_net.c to build
```

---

## Open Questions

- **Server URL**: hardcode for now, configurable later? The server doesn't
  exist yet in production. Use `10.0.2.2:9000` for emulator testing
  (Android emulator's host loopback alias).

- **Offline mode**: what happens if the server is unreachable? The LAN tab
  should still work. Internet tabs should show "Server unreachable" with
  a retry button. No crash, no hang.

- **Connection during game**: keep the WebSocket alive during gameplay?
  Pros: real-time friend presence, mid-game notifications. Cons: battery,
  data usage. Recommend: keep alive but reduce message frequency (no
  lobby list refreshes, only friend presence + connection health).

- **Deep links**: support `dxx-redux://join/lobbyid` links for sharing?
  Would let players share a link that opens the app and joins a lobby.
  Nice to have, not v1.

- **Multiple accounts**: what if a device has multiple Google accounts?
  GPGS handles this via the Games profile. The player picks their
  profile once and it's remembered.

- **Proxy port exhaustion**: with 7 peers max (8-player game), we need
  ports 42430-42436. These are high ports unlikely to conflict. If a port
  is already in use (another app), fail with a clear error message.

- **IPv6 support**: the STUN client and proxy need to handle both IPv4
  and IPv6 addresses. The engine uses `struct _sockaddr` which is
  typedef'd to either sockaddr_in or sockaddr_in6 based on compile flags.
  For v1, IPv4 only is acceptable (most mobile networks provide IPv4).

- **Connectivity check packet vs game packet**: the connectivity check
  uses a 12-byte probe packet with magic 0xD2CC0100. This magic must NOT
  collide with any UPID_* value used by the game engine. UPID values are
  single-byte (0x00-0x40 range), so a 4-byte magic starting with 0xD2 is
  safe.

- **Relay session cleanup**: the server currently has no relay session
  cleanup (RelaySession entries are never removed). This must be fixed
  server-side before relay is usable in production. Add a created_at
  timestamp and periodic cleanup of sessions older than 2 hours.
