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
