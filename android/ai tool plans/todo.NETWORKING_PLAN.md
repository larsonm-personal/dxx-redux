# Multiplayer Networking Plan

## Overview

Add multiplayer support to the Android launcher: LAN discovery, internet matchmaking
with NAT traversal, lobby UI, transparent connection handoff to the game engine,
co-op with drop-in/drop-out, file hash verification, and level pack exchange.

The core insight: the launcher owns the real network connections and presents
localhost addresses to the game engine. The engine never knows whether the peer
is on LAN or across the internet behind a symmetric NAT with relay. This
eliminates the need to modify the engine's networking stack for internet play.

---

## Existing Engine Networking (Reference)

### Protocol Stack

- Transport: pure UDP/IP, default port 42424
- LAN discovery: broadcast to 255.255.255.255 + IPv6 multicast ff02::1
- Protocol version: 30006 (Redux 1.2)
- Host-authoritative architecture (host is master)
- Co-op mode: GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS (just a mode flag, no protocol difference)

### Key Files

- d2/main/net_udp.c (~7000 lines): core UDP implementation
- d2/main/multi.c: high-level multiplayer wrapper
- d2/main/multi.h: game mode constants, message types
- d2/main/multibot.c: robot AI sync for multiplayer
- d1/main/ has near-identical copies of all of these

### Socket Architecture

- 3 static sockets: UDP_Socket[0]=game, [1]=fallback/broadcast, [2]=tracker
- Single socket for all peer communication (sendto with per-peer address)
- No mechanism to pass in pre-created sockets
- Polled via select() each frame

### Packet Types

Discovery/connection: UPID_GAME_INFO_LITE_REQ/RESP, UPID_REQUEST, UPID_SYNC,
UPID_OBJECT_DATA (~20 types total)

Gameplay: UPID_PDATA (position, 26-72 bytes, 15-30/sec), UPID_MDATA (reliable
game state, 10/sec), UPID_PING/PONG

### Existing NAT Traversal

- Holepunching: 30 UDP pings over 3 seconds to each peer, then 1/sec ongoing
- Host relay: UPID_PROXY wrapper (7 bytes overhead), host forwards between peers
- Connection states: CONNT_DIRECT, CONNT_PROXY, CONNT_NONE
- RetroProtocol mode: shares all player addresses in SYNC, enables P2P attempts
- Tracker: game listing service only (retro-tracker.game-server.cc:42420), no NAT help
- No STUN, no TURN, no symmetric NAT handling

### Player Rejoin

- Detected by callsign + IP match (IP match is a problem on mobile)
- Inventory NOT preserved on rejoin
- Object sync sends all game objects + monitor vector
- Wall/trigger states have gaps on rejoin

---

## Architecture

```
                    MATCHMAKING SERVER (Rust)
                    +-----------------------+
                    | WebSocket signaling   |
                    | Lobby management      |
                    | STUN result exchange  |
                    | Identity verification |
                    | UDP relay (fallback)  |
                    +-----------+-----------+
                                | WSS + UDP
----------------------------------------------------------------
  DEVICE                        |
                                |
  +-----------------------------+-------------------------------+
  | SetupActivity / Lobby (Kotlin)                              |
  |                                                             |
  | OWNS the real UDP socket (port 42424)                       |
  | 1. Authenticate via Google Play Games sign-in               |
  | 2. Connect to matchmaking server (WebSocket)                |
  | 3. STUN query -> discovers reflexive address                |
  | 4. Exchange addresses via WebSocket signaling               |
  | 5. Holepunch from this socket (coordinated by server)       |
  | 6. Connection fully established BEFORE game launches        |
  |                                                             |
  | Localhost proxy (once connected):                           |
  | Peer A (real: 1.2.3.4:42424) <-> localhost:42430            |
  | Peer B (real: 5.6.7.8:42424) <-> localhost:42431            |
  | ...one localhost port per peer                              |
  +-----------------------------+-------------------------------+
                                | localhost UDP (zero-cost)
  +-----------------------------+-------------------------------+
  | Game Engine (net_udp.c)                                     |
  |                                                             |
  | Thinks peers are at 127.0.0.1:42430, :42431 ...             |
  | Normal sendto()/recvfrom() -- no engine networking changes  |
  | Auto-join via JNI: nativeAutoJoin("127.0.0.1", 42430, ...)  |
  +-------------------------------------------------------------+
```

### Why Localhost Proxy

The game engine's UDP sockets are static globals in net_udp.c with no mechanism
to accept external sockets. Rather than restructuring the engine's networking
(which would be a massive change and break upstream compatibility), we let the
Kotlin launcher own the real connection and proxy through localhost.

Localhost UDP forwarding has zero meaningful latency -- the kernel handles it
at the loopback layer without hitting any network stack. This is the same
technique used by VPN apps, WireGuard userspace, and game proxy tools.

Benefits:
- Zero engine code changes for internet play
- Lobby connection survives game crashes (can notify peers, allow reconnect)
- Holepunching is complete before the game starts (transparent to engine)
- Same auto-join JNI path for both LAN direct and internet proxied
- Proxy can log, filter, and rate-limit packets if needed

### LAN vs Internet Mode

| Aspect           | LAN Mode                        | Internet Mode                     |
|------------------|---------------------------------|-----------------------------------|
| Discovery        | UDP broadcast (Kotlin)          | WebSocket to matchmaking server   |
| Connection setup | Direct UDP to peer              | STUN + holepunch (or relay)       |
| Lobby UI         | Same Compose screen             | Same Compose screen               |
| Auto-join        | nativeAutoJoin(peerIP, 42424)   | nativeAutoJoin("127.0.0.1", 4243x)|
| Proxy needed?    | No                              | Yes (Kotlin localhost proxy)      |
| Engine changes   | Auto-join path only             | Auto-join path only (same)        |

---

## Phase 1: LAN Discovery and Lobby -- NOT STARTED

### Goal

Two devices on the same WiFi can discover each other, form a lobby, and
launch into a co-op game from the launcher without touching in-game menus.

### Steps

1.1. Create lobby/LobbyService.kt -- owns a DatagramSocket on port 42400,
     broadcasts LOBBY_ANNOUNCE every 3 seconds, listens for announcements.
     Packet format: JSON {type, version, callsign, game, hosting, lobby_id,
     player_count, max_players}

1.2. Add CHANGE_WIFI_MULTICAST_STATE and ACCESS_WIFI_STATE permissions to
     AndroidManifest.xml. Acquire WifiManager.MulticastLock during lobby
     lifetime (Android suppresses broadcast/multicast without this).

1.3. Create lobby/LobbyProtocol.kt -- message types: ANNOUNCE, JOIN_LOBBY,
     LEAVE_LOBBY, PLAYER_LIST, READY, START_GAME, FILE_HASH_REQ,
     FILE_HASH_RESP. Each message carries lobby_id (UUID) for disambiguation.
     Host-authoritative player list.

1.4. Create lobby/LobbyScreen.kt (Compose) -- new screen in SetupActivity.
     Shows discovered LAN lobbies. "Host Game" creates a lobby. "Join" sends
     JOIN_LOBBY. Player list with ready checkboxes. Host has "Start Game"
     button (enabled when all ready).

1.5. Add JNI method nativeSetAutoJoin(hostAddr, hostPort, myPort,
     mission, gameMode, difficulty) to MainActivity. C side stores params
     in globals, sets auto_join_pending flag.
     **Detailed design in todo.CLIENT_NETWORKING_PLAN.md Phase C4b**
     (JNI interface, globals, intent extras, engine menu bypass)

1.6. Add game startup bypass in d2/main/inferno.c or menu.c: if
     auto_join_pending, skip main menu and call check_auto_join().
     Implementation in new auto_net.c/h files (shared pattern for d1/d2).

1.7. check_auto_join() in auto_net.c: initializes networking, opens socket
     on auto_my_port, fills direct_join struct with host addr, calls
     net_udp_game_connect() directly (bypassing all menus).

1.8. Host auto-start path: auto_host_pending flag, check_auto_host()
     initializes networking, configures Netgame, loads mission by name,
     calls net_udp_start_game().

1.9. "Start Game" flow in LobbyScreen: host sends START_GAME to all peers
     (includes host IP, port, mission, mode). All peers launch MainActivity
     with auto_join intent. Host launches with auto_host intent.

### Files to Create

- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyScreen.kt

### Files to Modify

- android/app/src/main/AndroidManifest.xml (permissions)
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt (lobby nav)
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt (auto-join intent)
- android/app/src/main/cpp/jni_main.c (nativeAutoJoin/Host JNI)
- d2/main/net_udp.c (net_udp_direct_join, net_udp_auto_host)
- d2/main/inferno.c or menu.c (auto-join bypass)
- d1/main/ equivalents for all d2/main/ changes

### Verification

- Two emulators on host: both discover each other's lobbies
- Join lobby, both show in player list, ready up
- Host presses Start, both games launch and connect to co-op
- Introspection API shows both on same level with co-op game_mode

---

## Phase 2: Internet Matchmaking Server (Rust) -- PARTIALLY COMPLETE

### Status

Implemented (in server/):
- [x] main.rs: startup, config, TLS, ws+http+relay task spawn
- [x] ws_handler.rs: WebSocket lifecycle, message dispatch, all message handlers
- [x] lobby.rs: lobby state, player tracking, presence, connection types
- [x] protocol.rs: full client/server message definitions with serde
- [x] relay.rs: UDP relay socket, session-token routing, rate limiting
- [x] rate_limit.rs: per-IP and per-identity sliding-window rate limiting
- [x] db.rs: SQLite persistence (players, bans, friends, match history)
- [x] friends.rs: friend request/accept/remove/presence tracking
- [x] stats.rs: in-memory counters (connections, lobbies, games, relays)
- [x] config.rs: server configuration struct
- [x] http_api.rs: health, status, admin endpoints, uptime tracking
- [x] Integration tests: 27 tests covering auth, lobbies, friends, kicks, game start, HTTP API, welcome bundle, messaging, stable identity
- [x] LOBBY_UPDATE broadcasting on create/join/leave/ready
- [x] StartGame handler: CONNECTION_INFO + GAME_STARTING to all members, presence updates
- [x] KickPlayer handler: notifications, session cleanup, lobby broadcast
- [x] FileHashes forwarding to host
- [x] ConnectivityOk/ConnectivityUpdate handlers
- [x] ICE-style candidate exchange via STUN_RESULT/PEER_CANDIDATES
- [x] Connection type reporting (CONNECTION_INFO with method/detail/relay flag)
- [x] DashMap deadlock audit and fixes (StartGame, LeaveLobby, KickPlayer)
- [x] Stable player identity via find_or_create_player_by_gpgs (db.rs + ws_handler.rs)
- [x] Post-auth welcome bundle: SERVER_STATUS, LOBBY_LIST, FRIEND_LIST_RESP (Phase 2.5)
- [x] Player messaging: SEND_MESSAGE handler with rate limiting, block check, validation
- [x] ActiveGameInfo in SERVER_STATUS (host_callsign, mission, mode, player_count, duration)
- [x] host_ping_ms field in LobbyInfo (lobby list and HTTP status)
- [x] is_blocked() DB function for message filtering
- [x] CONNECTIVITY_CHECK_GO orchestration: sent to all players when all STUN_RESULTs received, lobby transitions to Holepunching
- [x] RELAY_ASSIGNED session allocation during StartGame for ConnectionType::Relay pairs, with STUN address pre-population
- [x] Relay address learning: auto-register unknown sources on first packet (relay.rs)
- [x] TLS certificate loading infrastructure: config fields (tls_cert_path, tls_key_path), tls.rs module with rustls-pemfile PEM loading
- [x] relay_public_addr config field for RELAY_ASSIGNED message addressing
- [x] candidate_pair_priority() scoring for ICE-style candidate pair ordering
- [x] Integration tests: 38 tests (added CONNECTIVITY_CHECK_GO, RELAY_ASSIGNED, kicked-player rejoin, lobby codes, friend code bypass, predictive port candidates, verified-only lobby)
- [x] identity.rs (Google Play Games token verification with skip_gpgs_verify dev mode)
- [x] Kicked-player rejoin prevention (lobby.kicked_players tracked, checked in JoinLobby and JoinFriendGame)
- [x] Lobby codes for invite-only sessions (CreateLobby/JoinLobby with lobby_code, has_code in listing, friend bypass)
- [x] broadcast_lobby_update after successful JoinFriendGame join

- [x] Predictive port allocation logic (Strategy 4): generate_predicted_candidates() in ws_handler.rs; server-side sequential port prediction for symmetric NATs, merged into PEER_CANDIDATES
- [x] "Verified only" lobby setting: verified_only field on CreateLobby/Lobby/LobbyInfo, gpgs_verified on PlayerSession, enforcement in JoinLobby and JoinFriendGame

Not yet implemented:
- [ ] Mid-session relay-to-direct migration orchestration

### Goal

A lightweight Rust server handles game listing, lobby management, identity
verification, STUN coordination, and UDP relay for symmetric NAT fallback.

### Server Components

```
matchmaking-server/
  src/
    main.rs           -- startup, config, TLS
    ws_handler.rs     -- WebSocket connections, message dispatch
    lobby.rs          -- lobby state, player tracking, game listing
    identity.rs       -- Google Play Games token verification
    stun_coord.rs     -- STUN result exchange, holepunch coordination
    relay.rs          -- UDP relay for symmetric NAT fallback
    rate_limit.rs     -- per-IP and per-identity rate limiting
```

### Rust Dependencies (pinned)

- tokio (async runtime)
- tokio-tungstenite (WebSocket)
- serde / serde_json (serialization)
- reqwest (Google token verification HTTP calls)
- ring or sha2 (hash verification)

### WebSocket Protocol

Client -> Server:
  AUTHENTICATE  {play_games_token, callsign}
  CREATE_LOBBY  {game, mission, mode, max_players}
  LIST_LOBBIES  {}
  JOIN_LOBBY    {lobby_id}
  LEAVE_LOBBY   {}
  READY         {ready: bool}
  STUN_RESULT   {reflexive_addr, nat_type}
  HOLEPUNCH_OK  {}
  FILE_HASHES   {hashes: [{name, sha256, size}]}
  START_GAME    {}  (host only)
  KICK_PLAYER   {player_id}

Server -> Client:
  AUTH_OK         {player_id, session_token}
  AUTH_FAIL       {reason}
  LOBBY_LIST      {lobbies: [...]}
  LOBBY_UPDATE    {players, readiness, file_status}
  PEER_STUN       {player_id, reflexive_addr}
  HOLEPUNCH_GO    {peer_addrs: [...]}
  GAME_STARTING   {host_addr, mission, mode, peer_localhost_ports}
  FILE_MISMATCH   {player, mismatched_files}
  RELAY_ASSIGNED  {relay_addr, session_token}
  CONNECTION_INFO {connections: [{peer_id, method, detail}]}
  ERROR           {code, message}
  RATE_LIMITED    {retry_after_ms}

### Connection Type Reporting (CONNECTION_INFO)

After connection establishment is complete (holepunch or relay fallback),
the server sends each client a CONNECTION_INFO message describing how it
is connected to each peer. The client can display this in a stats view
so players can see their connection quality.

```
Server -> Client (sent after GAME_STARTING, once per peer):
  CONNECTION_INFO {
    connections: [
      {
        peer_id: UUID,             // the peer player
        peer_callsign: string,     // for display
        method: string,            // "direct_holepunch", "relay", "direct_lan"
        detail: string | null,     // extra context, e.g. "both peers cone NAT",
                                   // "symmetric NAT fallback", "same subnet"
        server_relay: bool,        // true if traffic goes through the server
        estimated_latency_ms: u32 | null  // server-measured one-way latency
                                          // to the relay, if applicable
      }
    ]
  }
```

Connection method values:

| method              | meaning                                               |
|---------------------|-------------------------------------------------------|
| `direct_lan`        | Both peers on same LAN/subnet, no NAT traversal       |
| `direct_holepunch`  | UDP holepunch succeeded, direct peer-to-peer path     |
| `relay`             | Holepunch failed, traffic relayed through server      |

The `detail` field provides human-readable context:
- `"both peers cone NAT"` -- holepunch was straightforward
- `"symmetric NAT on one side"` -- holepunch succeeded but was less certain
- `"both symmetric NAT, holepunch failed"` -- explains why relay is needed
- `"same subnet detected"` -- LAN game, no internet traversal

The client uses this to show a connection quality indicator per peer in
the in-game stats overlay and post-game summary. Relay connections may
have higher latency; showing this helps players understand performance.

---

## Phase 3: Identity and Anti-Abuse -- COMPLETE

### Status

- [x] Rate limiting (server-side sliding-window: ws_message, lobby_create, lobby_join, auth)
- [x] Player banning (db.rs: ban/unban/is_banned, admin HTTP endpoints)
- [x] Lobby host kick (KickPlayer handler with notifications)
- [x] Match result tracking (db.rs)
- [x] Friend system (friend request/accept/remove, mutual verification)
- [x] Google Play Games identity verification (identity.rs, skip_gpgs_verify config for dev/test)
- [x] Lobby codes for invite-only sessions (lobby_code on CreateLobby/JoinLobby, has_code in listing)
- [x] Kicked-player rejoin prevention (lobby.kicked_players, JoinLobby + JoinFriendGame checks)
- [x] Proof-of-work fallback for non-Google devices (pow.rs: challenge generation, SHA-256 PoW verification, ed25519 signature verification; db.rs: keypair_identities table; protocol.rs: PowChallenge/PowSolution messages; ws_handler.rs: two-phase auth flow for new keys, fast path for known keys; config: pow_difficulty setting)
- [x] "Verified only" lobby setting (verified_only on CreateLobby, Lobby, LobbyInfo; gpgs_verified on sessions; VERIFIED_ONLY error code)

### The Problem with Bare Keypairs

Generating an Ed25519 keypair takes microseconds. A bot farm can generate
millions of identities for free. Banning by pubkey is useless when new keys
are free. This is analogous to banning by IP -- trivially circumvented.

Minecraft solves this with Mojang accounts (web auth, email verification,
payment gate). We can't build that, but we can leverage an existing identity
system that's already hard to mass-create: Google accounts.

### Google Play Games Services (GPGS) for Identity

GPGS provides server-side identity verification without building an account
system:

1. Android client calls PlayGamesSdk.getGamesSignInClient().signIn()
2. On success, gets a server auth code via requestServerSideAccess()
3. Client sends this token to our matchmaking server as play_games_token
4. Server exchanges token with Google's OAuth2 endpoint
   (POST https://oauth2.googleapis.com/token) to get an access_token + id_token
5. Server uses the access_token to call the Play Games REST API
   (GET https://www.googleapis.com/games/v1/players/me) to get the stable
   GPGS player ID (distinct from the Google account ID in the id_token sub claim)
6. This GPGS player ID is the persistent identity -- tied to a Google account

Why this works for anti-abuse:
- Google accounts are hard to mass-create (phone verification, CAPTCHA, etc.)
- Banning a GPGS player ID effectively bans the Google account from our game
- No passwords, no email collection, no PII stored on our server
- Free tier handles our scale easily
- Already on every Android device

### Server-Side Token Exchange Details

The server exchanges the one-time auth code for tokens via a standard
OAuth2 authorization_code grant. Confirmed by the Google docs and the
StackOverflow C# example at
https://stackoverflow.com/questions/56786698 (same flow, different language).

```
POST https://oauth2.googleapis.com/token
Content-Type: application/x-www-form-urlencoded

grant_type=authorization_code
&code=SERVER_AUTH_CODE_FROM_CLIENT
&client_id=WEB_CLIENT_ID (from Google API console, NOT the Android client ID)
&client_secret=WEB_CLIENT_SECRET
&redirect_uri=  (empty string -- must match the Google API console config)
```

Response:
```json
{
  "access_token": "...",
  "id_token": "...",
  "token_type": "Bearer",
  "expires_in": 3600,
  "refresh_token": "..."
}
```

Important implementation notes:
- The client_id/client_secret are the WEB type OAuth credentials, not the
  Android client ID. The Android client ID is only used client-side in
  requestServerSideAccess(SERVER_CLIENT_ID).
- redirect_uri must match what's configured in the API console. For mobile
  app flows this is typically empty string. Getting this wrong gives a
  misleading "redirect_uri_mismatch" error.
- Server auth codes expire within minutes. Exchange immediately on receipt --
  no caching or queuing. If debugging manually, the code may expire before
  you can use it (Google returns "invalid_grant" which is misleading).
- The id_token is a JWT whose "sub" claim is the Google account ID, NOT the
  GPGS player ID. To get the stable GPGS player ID, use the access_token to
  call GET https://www.googleapis.com/games/v1/players/me -- the response
  contains a "playerId" field which is the GPGS-specific stable identity.
- The access_token from this exchange is short-lived (~1 hour). We only need
  it once to fetch the player ID, then discard it. No need to store or
  refresh tokens.

### Current Implementation Gap

The server has the scaffolding ready:
- config.rs: google_client_id and google_client_secret loaded from env vars
  (GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET) but not yet used
- ws_handler.rs: AUTHENTICATE handler has a TODO comment, currently passes
  play_games_token directly to find_or_create_player_by_gpgs as the identity
  key (dev mode -- any string works as identity)
- db.rs: find_or_create_player_by_gpgs() maps gpgs_player_id string to a
  stable server-side UUID. Ready to receive real GPGS player IDs.
- identity.rs: does not exist yet. Will contain the token exchange +
  player ID fetch logic.

When implementing identity.rs:
1. Use reqwest (already a dependency) to POST to the token endpoint
2. Parse the JSON response to get access_token
3. Use access_token to GET /games/v1/players/me
4. Extract playerId from the response
5. Return the playerId string to the AUTHENTICATE handler
6. Handler passes it to find_or_create_player_by_gpgs as before
7. Add a config flag (e.g. SKIP_GPGS_VERIFY=true) for dev/test mode that
   preserves the current behavior of using play_games_token directly
8. Integration tests should set SKIP_GPGS_VERIFY=true so they can continue
   using fake-token-* strings without hitting Google's servers

### Gradle Dependencies to Add

```gradle
implementation 'com.google.android.gms:play-services-games-v2:21.1.0'
```

Plus Play Console setup: create a Play Games project, configure OAuth consent,
generate OAuth 2.0 client IDs (Android + server).

### Fallback for Non-Google Devices

Some Android devices lack Google Play Services (Amazon Fire, Huawei, custom
ROMs). Fallback chain:

1. Try GPGS sign-in
2. If unavailable: generate a device-local Ed25519 keypair + require
   proof-of-work on first registration (slow enough to deter mass creation
   but fast enough for real users -- ~200ms on phone, ~0.1ms per bot attempt
   on server to verify)
3. Unverified players shown with a "?" badge in lobbies so hosts know
4. Hosts can set lobby to "verified only" (GPGS accounts only)

### Rate Limiting

At the matchmaking server:

| Limit                          | Value         | Scope      |
|--------------------------------|---------------|------------|
| New WebSocket connections      | 3/min         | Per IP     |
| Failed authentications         | 5/hour        | Per IP     |
| Lobby creation                 | 1/10 sec      | Per player |
| Lobby join attempts            | 5/min         | Per player |
| Relay sessions                 | 2 concurrent  | Per player |
| Relay bandwidth                | 50 KB/s       | Per session|
| Relay session duration         | 2 hours max   | Per session|
| WebSocket message rate         | 30/sec        | Per conn   |

Implementation: sliding window counters in HashMap<Key, VecDeque<Instant>>.
Cheap in memory, O(1) amortized per check.
Clients should be told which rate limit they hit so that human players are less frustrated/can submit remediation requests more easily

### Additional Anti-Griefing

- Lobby host can kick players (by player ID, not just IP)
- Kicked players can't rejoin the same lobby
- Lobby codes for invite-only sessions (skip public listing)
  * quick share option for texting. the share link should load the game and pre-fill everything needed to join the lobby and connect (with a warning about sharing if it includes local IPs, etc.)
- Server-side player reputation (future): track kick frequency, reports
- Packet validation: malformed messages -> immediate disconnect, no state
  allocated until after authentication

### How Minecraft Servers Handle This (Reference)

| Technique                | Minecraft                          | Our Equivalent                      |
|--------------------------|------------------------------------|-------------------------------------|
| Account verification     | Mojang session server UUID check   | GPGS token exchange                 |
| Connection throttle      | 4s cooldown per IP (vanilla)       | 3 conn/min/IP rate limit            |
| Persistent bans          | UUID ban (not IP ban)              | GPGS player ID ban                  |
| Whitelist mode           | Pre-approved UUIDs only            | "Verified only" lobby setting       |
| Proxy-level protection   | BungeeCord/Velocity rate limiting  | Server-side rate limiting           |
| Anti-bot                 | Forced movement check, CAPTCHAs    | GPGS (Google's bot prevention)      |

---

## Phase 4: NAT Traversal (ICE-Style Multi-Strategy Pipeline) -- PROTOCOL DEFINED, ORCHESTRATION NOT STARTED

### Status

- [x] Protocol messages defined: STUN_RESULT (with candidates/nat_type), PEER_CANDIDATES, CONNECTIVITY_CHECK_GO, CONNECTIVITY_OK, CONNECTIVITY_UPDATE
- [x] ConnectionCandidate and CandidatePair types in protocol.rs
- [x] ConnectionType enum: Unknown, DirectLan, DirectUpnp, DirectHolepunch, PredictedHolepunch, Relay
- [x] determine_connection_type() logic based on candidates and NAT type
- [x] Candidate exchange: STUN_RESULT stores candidates on LobbyPlayer, PEER_CANDIDATES distributes to peers
- [x] ConnectivityOk handler records winning type and RTT
- [x] ConnectivityUpdate handler for mid-session migration
- [x] CONNECTION_INFO reporting to all lobby members
- [x] CONNECTIVITY_CHECK_GO orchestration: server broadcasts to all players when all STUN_RESULTs received
- [x] Relay session assignment wired to lobby flow (RELAY_ASSIGNED on StartGame for Relay pairs)
- [x] Predictive port allocation (Strategy 4 algorithm): generate_predicted_candidates() generates next-port candidates for sequential symmetric NATs
- [ ] Client-side implementation (Kotlin STUN, holepunching, localhost proxy)
      **Detailed client-side plan in todo.CLIENT_NETWORKING_PLAN.md Phase C4a**
      (StunClient.kt, ConnectivityChecker.kt, LocalhostProxy.kt, NatTraversal.kt)
- [ ] UPnP/PCP/NAT-PMP client-side integration (deferred to v2, relay covers gap)
- [ ] Game auto-join/auto-host via JNI (menu bypass, intent extras, status overlay)
      **Detailed plan in todo.CLIENT_NETWORKING_PLAN.md Phase C4b**
      (auto_net.c/h in d1/ and d2/, jni_auto_net.c, MultiplayerStatusOverlay.kt)
- [ ] Relay session cleanup server-side (RelaySession currently never cleaned up)

### Design Philosophy: ICE for Games

ICE (Interactive Connectivity Establishment) is a framework, not a single
protocol. It defines a deterministic pipeline that gathers every possible
network path between two peers, races them in parallel, and picks the best
one. WebRTC, Steam Networking Sockets, Xbox Live, and PlayStation Network
all implement some variant of this.

Our implementation follows the same pattern but is tuned for game traffic:
- UDP-first (latency-critical)
- Minimal candidate gathering (fast startup -- games can't wait 10 seconds)
- Custom orchestration (hand-rolled; no crate covers the full pipeline)
- Rust crates for the low-level primitives (STUN parsing, TURN relay, etc.)

The key insight: ICE is not "try one thing and fall back." It is a parallel
race across multiple candidate paths. The first path that succeeds wins.

### The NAT Landscape for Mobile Games

| NAT Type              | UDP Holepunch? | TCP Holepunch? | Where You See It             |
|-----------------------|----------------|----------------|------------------------------|
| Full Cone             | Always         | Always         | Rare (old routers)           |
| Address-Restricted    | Yes            | Yes            | Common home WiFi             |
| Port-Restricted Cone  | Yes            | Sometimes      | Most common home WiFi        |
| Symmetric             | Usually fails  | Usually fails  | Mobile carriers (CGNAT),     |
|                       |                |                | corporate networks           |

### IPv6 Does NOT Solve NAT on Mobile

A common claim is "IPv6 means no NAT, so holepunching is unnecessary."
This is wrong in practice for mobile:

- Nearly all mobile carriers run stateful firewalls even on IPv6. The
  firewall drops unsolicited inbound UDP by default. This means even
  with globally routable IPv6 addresses, a peer can't send to you
  unless you've sent to them first -- same problem as NAT.
- Carrier IPv6 firewalls behave like port-restricted cone NAT from the
  perspective of holepunching: outbound traffic creates a pinhole that
  allows return traffic from the same endpoint. So holepunching still
  works the same way -- both sides send to each other, creating pinholes.
- Some carriers do NAT66 (IPv6-to-IPv6 NAT) or NAT64 (IPv6 client
  behind NAT to reach IPv4 servers). These are becoming less common
  but still exist.
- Enterprise/corporate networks also firewall IPv6 same as IPv4.
- Bottom line: treat IPv6 the same as IPv4 for connection establishment.
  Always do STUN + holepunch regardless of IP version. The only
  difference is that symmetric NAT (port randomization) is less common
  on IPv6, so holepunching succeeds more often.

### The Five NAT Traversal Strategies

The server orchestrates all of these in parallel (or rapid sequence) for
each peer pair. The first strategy that succeeds is used. The strategies
are listed in priority order (best to worst).

#### Strategy 1: Direct LAN (No Traversal Needed)

Both peers share the same public IP (same LAN or subnet). Detected by
comparing STUN-reflected addresses. Skip all punching, connect directly.

Priority: highest. Latency: sub-1ms.

#### Strategy 2: UPnP / PCP / NAT-PMP Port Mapping

Request the local router to create an explicit port mapping. This turns
a restricted NAT into an open port.

- UPnP (Universal Plug and Play): XML/SOAP, most home routers support it
- NAT-PMP (NAT Port Mapping Protocol): Apple's simpler alternative
- PCP (Port Control Protocol): successor to NAT-PMP (RFC 6887)

Cannot be relied on (many routers have UPnP disabled, enterprise networks
never support it), but when available it gives zero-overhead direct
connectivity.

Client-side only (Kotlin). The server is informed via a new message if
a mapping was successfully created, so other peers can connect directly
to the mapped port.

Rust crate (server-side, for reference/testing): `igd-next` (UPnP IGD).
Client-side (Kotlin): Android has no built-in UPnP; use the `cling` or
`jupnp` Java library, or a minimal hand-rolled SSDP+SOAP client (~200
lines for the port mapping case only).

Priority: high (opportunistic). Latency: direct.

#### Strategy 3: UDP Hole Punching (STUN-Coordinated)

The core NAT traversal technique. Works against all cone NATs (full,
address-restricted, port-restricted) which covers ~85-90% of home networks.

1. Each peer sends a STUN Binding Request to discover their reflexive
   address (public IP:port as seen by the STUN server).
2. Peers exchange reflexive addresses via the matchmaking server.
3. Both peers send priming UDP packets to each other's reflexive address
   simultaneously (coordinated by the server).
4. The NAT creates mappings for outbound packets. When the peer's packet
   arrives, the NAT recognizes it as a response and lets it through.
5. Once bidirectional UDP flows, the direct path is established.

Implementation notes:
- Self-hosted STUN server embedded in the matchmaking server process.
  Two UDP listeners on separate ports (default 3478 and 3479) for NAT
  type detection (client queries both from the same local socket).
- IP allowlisting: only responds to IPs that have an active authenticated
  WebSocket session. IPs are added on auth, removed on disconnect.
  Prevents abuse from unauthenticated/public traffic.
- STUN server addresses sent to client in AUTH_OK response
  (stun_addrs field: ["host:3478", "host:3479"]).
- Client side hand-rolls the 20-byte binding request (trivial format).
- Query both self-hosted STUN ports to detect symmetric NAT:
  if reflexive ports differ, the NAT is symmetric.

Symmetric NAT detection: if two STUN queries from the same local socket
return different external ports, the NAT assigns ports unpredictably and
standard holepunching will not work. Move to Strategy 4 or 5.

Priority: primary. Latency: direct P2P.

#### Strategy 4: Predictive Port Allocation (Symmetric NAT Workaround)

Many symmetric NATs allocate ports sequentially (e.g., 50000, 50001,
50002...). By observing the last two STUN-reflected ports, the server
can predict the next port the NAT will allocate and instruct peers to
punch to the predicted port.

Algorithm:
1. Client sends STUN to server A -> gets reflexive port P1
2. Client sends STUN to server B -> gets reflexive port P2
3. If P2 = P1 + 1 (or close), NAT is sequential-symmetric
4. Server predicts next port = P2 + 1 (or P2 + delta)
5. Server tells the other peer to punch to IP:predicted_port
6. Client sends a new outbound packet (to trigger the NAT allocation)
7. If prediction was correct, holepunch succeeds on the predicted port

This is not reliable (port prediction can be wrong, some NATs are random),
but it catches a meaningful fraction of symmetric NAT cases that would
otherwise require relay. Worth attempting for ~500ms before falling back.

Success rate: ~30-50% of symmetric NATs. Latency: direct P2P if it works.

#### Strategy 5: Server-Relayed UDP (TURN-Like Fallback)

When all direct-connection strategies fail (both peers behind symmetric
NAT with random port allocation, or aggressive firewalls), the matchmaking
server relays UDP traffic.

Relay packet format:

  [session_token (4 bytes LE) | dest_player_slot (1 byte) | game_packet]

Server forwards to the destination peer:

  [session_token (4 bytes LE) | from_player_slot (1 byte) | game_packet]

5 bytes overhead per packet. Server is stateless per-packet (just a
lookup table: session_token -> {player_slot -> peer_addr}).

Relay characteristics:
- Added latency: one extra hop, typically 10-30ms depending on server
  location. For a game at 30fps (33ms/frame), this is less than one
  frame of added latency.
- Bandwidth: game uses ~5-15 KB/s per player. A $5 VPS can relay
  hundreds of simultaneous sessions.
- Rate-limited per session to prevent abuse (50 KB/s cap).

Rust crate considerations: the relay is simple enough to hand-roll (it's
just a UDP recv/lookup/send loop). A full TURN server (RFC 5766) is
overkill for our relay because we don't need TURN's allocation mechanism,
channel binding, or authentication -- our relay sessions are created via
the WebSocket signaling channel with session tokens, not via TURN
protocol messages. `turn-rs` or `coturn` could be used if interoperability
with standard TURN clients were needed, but for a custom game relay the
hand-rolled approach is simpler and more efficient.

Priority: lowest (always works). Latency: 10-30ms added.

### Strategy Summary and Expected Coverage

| Strategy                   | Works Against         | Success Rate | Latency  |
|----------------------------|-----------------------|--------------|----------|
| Direct LAN                 | Same subnet           | 100%         | <1ms     |
| UPnP/PCP/NAT-PMP           | Home routers w/ UPnP  | ~40-60%      | Direct   |
| UDP Holepunch (STUN)       | All cone NATs         | ~85-90%      | Direct   |
| Predictive Port (Symmetric)| Sequential symmetric  | ~30-50%      | Direct   |
| Server Relay               | Everything            | 100%         | +10-30ms |

Combined, the first four strategies should achieve direct P2P for ~95%+
of real-world peer pairs. Only the hardest cases (both behind random-port
symmetric NATs with no UPnP, ~5%) will use relay.

### ICE-Style Candidate Gathering

Each client gathers a set of connection "candidates" before the game
starts. This happens in the lobby phase, transparent to the player.

```
Candidate types (gathered per client):
  1. Host candidate:  local LAN IP:port (e.g., 192.168.1.50:42424)
  2. Server-reflexive: STUN-discovered public IP:port (e.g., 73.22.19.44:62014)
  3. Predicted:        predicted next port for sequential symmetric NATs
  4. UPnP-mapped:      explicitly mapped port via UPnP/PCP (if available)
  5. Relay:            server relay endpoint (always available as fallback)
```

The client sends all gathered candidates to the server via STUN_RESULT
(extended to carry multiple candidates). The server distributes each
player's candidates to all other players and coordinates connectivity
checks.

### Connectivity Check Race

Once candidates are exchanged, the server triggers simultaneous
connectivity checks across all candidate pairs. This is the ICE "race":

```
For each peer pair (A, B):
  candidate_pairs = cross_product(A.candidates, B.candidates)
  sort by priority:
    1. host <-> host           (LAN, if same public IP)
    2. UPnP <-> any            (if A or B got a mapping)
    3. srflx <-> srflx         (standard UDP holepunch)
    4. predicted <-> predicted  (symmetric NAT workaround)
    5. relay <-> relay          (guaranteed fallback)

  For each pair (in priority order):
    Both peers send test packets simultaneously
    First pair to get bidirectional response wins
    Timeout per pair: 500ms (aggressive for fast game startup)
    Total timeout: 3 seconds (then relay fallback is forced)
```

The key difference from WebRTC ICE: we check fewer candidates (5 types
max vs WebRTC's potentially dozens) and use shorter timeouts (games
need fast connection, not exhaustive probing).

### Holepunch Sequence (Coordinated by Server)

```
Timeline (all in lobby, BEFORE game launch):

  Client A                  Server                     Client B
     |                        |                            |
     | STUN_RESULT(candidates=[...])                       |
     |----------------------->|                            |
     |                        |  STUN_RESULT(candidates=[...])
     |                        |<---------------------------|
     |                        |                            |
     |  PEER_CANDIDATES(B's candidates)                    |
     |<-----------------------|                            |
     |                        | PEER_CANDIDATES(A's candidates)
     |                        |--------------------------->|
     |                        |                            |
     |  CONNECTIVITY_CHECK_GO |   CONNECTIVITY_CHECK_GO    |
     |<-----------------------|--------------------------->|
     |                        |                            |
     | -- parallel checks across all candidate pairs --    |
     | UDP pings to each candidate addr simultaneously     |
     |                                                     |
     | CONNECTIVITY_OK(winning_pair)                       |
     |----------------------->|                            |
     |                        |  CONNECTIVITY_OK(winning_pair)
     |                        |<---------------------------|
     |                        |                            |
     | Lobby shows "Connected (direct)" or "(relay)"       |
```

CONNECTIVITY_CHECK_GO is sent to both peers simultaneously so they
start probing at the same time. Simultaneous sends maximize holepunch
success -- if A sends before B, A's packet arrives at B's NAT before
B has created a mapping and gets dropped.

### Mid-Session Migration (Relay -> Direct)

If a pair starts on relay (because holepunch timed out during the
initial 3-second window), the client continues sending periodic
direct-path probes in the background. If a direct path opens later
(e.g., NAT mapping stabilized, network change), the client can
migrate from relay to direct mid-game.

Implementation:
- Every 30 seconds, the localhost proxy sends a test packet via the
  direct candidate (not through relay).
- If a response comes back, switch the forwarding path.
- Notify the server via CONNECTIVITY_UPDATE so connection_type is
  updated and CONNECTION_INFO reflects the change.
- No packet loss during migration: both paths are valid briefly,
  old path continues until new path is confirmed.

This is an optimization, not required for v1. Implement after the
basic pipeline is stable.

### Rust Crates for NAT Traversal Components

The orchestration layer (candidate gathering, connectivity check race,
priority logic, fallback decisions) is hand-rolled because no crate
covers the full pipeline for games. But the low-level primitives should
use existing crates where possible:

| Component              | Crate                  | Notes                              |
|------------------------|------------------------|------------------------------------|
| STUN message parsing   | `stun_codec` (=0.3)    | RFC 5389/8489, Binding Request/    |
|                        |                        | Response, XOR-MAPPED-ADDRESS.      |
|                        |                        | Handles encoding/decoding only.    |
| STUN transport         | hand-rolled            | Just send/recv UDP with the parsed |
|                        |                        | messages. ~30 lines.               |
| NAT type detection     | hand-rolled            | Compare two STUN results. ~20 lines|
| UPnP port mapping      | `igd-next` (=0.15)     | UPnP IGD for requesting port maps. |
|                        |                        | Server-side testing/reference only.|
| UDP relay              | hand-rolled            | Simple recv/lookup/send loop. TURN |
|                        |                        | (RFC 5766) is overkill for our     |
|                        |                        | custom session-token format.       |
| Connectivity checks    | hand-rolled            | send test packets, track responses,|
|                        |                        | pick winner. ~100 lines.           |
| Candidate exchange     | existing WS protocol   | STUN_RESULT extended to carry      |
|                        |                        | multiple candidates.               |
| Orchestration/ICE logic| hand-rolled            | Priority sorting, timeout logic,   |
|                        |                        | fallback decisions. ~200 lines.    |

Why not use a full ICE/WebRTC crate (e.g., `webrtc-rs`, `str0m`)?
- They bundle DTLS, SRTP, SDP, congestion control -- all unnecessary
  for raw game UDP.
- They are designed for media streams, not game packets.
- Their ICE agents assume SDP-based signaling; we use our own WebSocket
  protocol.
- The orchestration layer is ~300 lines total. Pulling in a WebRTC stack
  for that would add massive dependency weight and complexity.

Game networking crates (`laminar`, `renet`, `naia`, `quinn`) handle
reliability/ordering layers but none implement NAT traversal. They
assume you bring your own connectivity.

### Protocol Changes for ICE-Style Traversal

Extended client messages:

```
  STUN_RESULT  {
    candidates: [
      {type: "host",   addr: "192.168.1.50:42424"},
      {type: "srflx",  addr: "73.22.19.44:62014"},
      {type: "upnp",   addr: "73.22.19.44:42424"},       // if UPnP succeeded
      {type: "predicted", addr: "73.22.19.44:62016"}      // if symmetric+sequential
    ],
    nat_type: "port_restricted_cone" | "symmetric" | "symmetric_sequential" | "unknown"
  }

  CONNECTIVITY_OK  {peer_id, winning_candidate_type, rtt_ms}
  CONNECTIVITY_UPDATE  {peer_id, new_method, detail}  // mid-session migration
```

Extended server messages:

```
  PEER_CANDIDATES  {peer_id, candidates: [...]}   // relay peer's candidates
  CONNECTIVITY_CHECK_GO  {peer_addrs: [...]}       // renamed from HOLEPUNCH_GO
  RELAY_ASSIGNED  {relay_addr, session_token}       // unchanged
  CONNECTION_INFO {connections: [{peer_id, method, detail, ...}]}  // unchanged
```

### Localhost Proxy (Kotlin)

The proxy is a ~50-line Kotlin coroutine per peer:

```
For each peer:
  Bind a localhost DatagramSocket on port 42430+N
  Loop:
    // Forward game -> peer
    recv from localhost port -> sendto real peer addr (or relay server)
    // Forward peer -> game
    recv from real socket (filtered by peer addr) -> sendto localhost port
```

The game engine calls sendto(127.0.0.1:42430) to talk to peer 0,
sendto(127.0.0.1:42431) for peer 1, etc. The proxy transparently
routes to the real addresses (direct or via relay).

NAT keepalive: proxy sends a 1-byte ping through the NAT every 15
seconds during idle periods (menus, loading) to prevent the NAT
mapping from expiring (typical timeout: 30-60 seconds for UDP).

---

## Phase 5: Drop-In/Drop-Out Co-op -- NOT STARTED

### Goal

Players can join and leave a co-op game mid-level. Returning players
get their inventory back.

### Inventory Persistence on Disconnect

In net_udp.c or multi.c, when a player disconnects (timeout or quit):

```c
typedef struct saved_player_state {
    char callsign[CALLSIGN_LEN+1];
    char gpgs_player_id[64];   // from launcher, passed via JNI
    int valid;
    fix energy, shields;
    int score, lives;
    int laser_level;
    uint primary_weapon_flags, secondary_weapon_flags;
    ushort primary_ammo[MAX_PRIMARY_WEAPONS];
    ushort secondary_ammo[MAX_SECONDARY_WEAPONS];
    uint vulcan_ammo;
    uint flags;  // includes keys
} saved_player_state;

static saved_player_state saved_states[MAX_PLAYERS];
```

Save on disconnect, keyed by callsign (or GPGS player ID if available,
to handle callsign changes). Restore after object sync on rejoin.

### Improved Rejoin Detection

Current: callsign + IP match. Problem: IP changes on mobile.

Change to: callsign match only, with a session_token (random 32-bit
value generated at first join). Add session_token to UPID_REQUEST.
Host verifies token matches the saved state for that callsign slot.
This prevents impersonation while allowing IP changes.

### Full Mid-Level State Transfer

The existing object sync (net_udp_send_objects) sends game objects and
the monitor vector. Extend with:

- MULTI_WALL_STATUS_BULK: send all wall states (open/closed/destroyed)
  as a single reliable message after object sync
- MULTI_TRIGGER_STATUS: send trigger fire counts so the joining player's
  level state matches (prevents ghost doors, missing walls, etc.)
- MULTI_RESTORE_INVENTORY: new reliable message that sets the rejoining
  player's inventory fields after sync completes
- wrap this with integration testing and checksumming because it's a difficult problem that will likely have bugs

### Files to Modify

- d2/main/net_udp.c: save/restore state, session_token in UPID_REQUEST,
  MULTI_WALL_STATUS_BULK, MULTI_TRIGGER_STATUS
- d2/main/multi.c: MULTI_RESTORE_INVENTORY handler
- d2/main/multi.h: new message type constants
- d1/main/ equivalents

---

## Phase 6: File Hash Verification and Level Exchange -- NOT STARTED

### Game File Hashing

Add jni_file_hash.c with a SHA-256 implementation (public domain single-file,
e.g. a minimal implementation -- the game data files are small enough that
performance doesn't matter). Exposed via JNI:

  nativeHashGameFiles(filesDir) -> JSON [{filename, sha256, size}, ...]

Hash: .hog, .pig, .ham, .mn2/.msn mission files

### Lobby Hash Exchange

During lobby phase (before game start):
1. Host sends FILE_HASH_REQ via lobby protocol (LAN) or WebSocket (internet)
2. Client responds with FILE_HASH_RESP containing hashes
3. Lobby UI shows per-file match/mismatch indicators
4. Mismatched base game files -> warning
5. Missing mission files -> "Transfer" option (if file is 3rd party)

### Level Pack Transfer

For 3rd-party levels the host has but a client lacks:
- Transfer via reliable channel (TCP on port 42401, or reliable UDP)
- Filter: hardcoded blocklist of paid asset filenames prevents
  transferring copyrighted base game files
- Client saves received files to active file set directory
- Verify hash after transfer

---

## Server Hosting

| Component       | Monthly Cost | Capacity                           |
|-----------------|-------------|------------------------------------|
| VPS (Hetzner)   | $4-6        | ~500 concurrent lobbies, 200 relay |
| Domain          | ~$1         | --                                 |
| TLS cert        | Free        | Let's Encrypt                      |

At scale: relay is the bandwidth bottleneck. Options:
- Per-session bandwidth cap (50 KB/s, game uses 10-15 KB/s)
- Geographic distribution (add server regions as playerbase grows)
- Evaluate libjuice (pure C ICE library, ~5K lines) to offload TURN
  to third-party TURN servers (Cloudflare, Twilio free tiers)

---

## Multi-Player Connection Topology (3-8 Players)

### Engine Topology: Star with Opportunistic Mesh

The engine supports two protocol modes controlled by `Netgame.RetroProtocol`:

**RetroProtocol=1 (default on Android): Hybrid Star-to-Mesh**

- Every player sends PDATA (position) and unreliable MDATA to all
  other players via `net_udp_send_to_player()`.
- Reliable MDATA (needack) always goes star through the host. The host
  rebroadcasts to all others.
- Per-peer connection state in `connection_statuses[MAX_PLAYERS]`:
  `CONNT_DIRECT`, `CONNT_PROXY`, or `CONNT_NONE`.
- On join, non-host clients have `CONNT_DIRECT` to the host and
  `CONNT_PROXY` (through host) to all other peers.
- The engine's P2P holepunch (`UPID_P2P_PING` with `force_direct=1`,
  30 attempts at 10Hz) runs automatically to upgrade proxy connections
  to direct. If direct fails, traffic stays proxied through host.

**RetroProtocol=0 (legacy): Pure Star**

- Clients only send to the host. Host forwards everything to all clients.
- Not relevant for our purposes since Android defaults to RetroProtocol=1.

### Implications for Lobby NAT Traversal

**Only N-1 holepunch pairs are required, not N*(N-1)/2.**

The critical connections are client-to-host. Non-host peer-to-peer
connections are optional -- if holepunch between two non-host peers fails,
traffic routes through the host proxy automatically. The engine already
handles this via `UPID_PROXY`:

```
UPID_PROXY packet: [UPID_PROXY][token:4][to_player:1][from_player:1][inner_packet...]
```

The host inspects `to_player`, looks up that player's address, and forwards.

**Lobby holepunch strategy for N players:**

1. Lobby facilitates N-1 holepunch pairs (each client to host)
2. Lobby shares all peer reflexive addresses in the START_GAME message
3. Engine's built-in P2P holepunch handles non-host peer connections
   opportunistically after game launch
4. If non-host holepunch fails: automatic host relay, transparent to gameplay

### Relay Server Impact for Multi-Player

In an 8-player game where the host is behind symmetric NAT:

- Worst case: all 7 clients relay through our server to the host.
  That is 7 relay streams, each ~10-15 KB/s bidirectional = ~105-210 KB/s
  per game session.
- Additionally, non-host peers that can't reach each other directly
  will proxy through the host (which itself may be relayed). This is
  double-relayed: client A -> relay server -> host -> relay server ->
  client B. This adds latency but works.

The relay server needs to support multiple simultaneous relay sessions
per game. The session token in the relay packet already includes a
`dest_player_id` field, so the server routes correctly within a
multi-player session.

**Relay bandwidth budget per game:**

| Players | Host-only relay (KB/s) | Full relay worst case (KB/s) |
|---------|------------------------|------------------------------|
| 2       | ~15                    | ~15                          |
| 4       | ~45                    | ~90 (double relay)           |
| 8       | ~105                   | ~350 (all double relay)      |

The worst case (8 players, all behind symmetric NAT, all relayed) is
unlikely but budgetable. The 50 KB/s per-session rate limit from Phase 3
needs adjustment: it should be per-player-pair, not per-session.

### Future: Dedicated Server Mode

The host-as-relay pattern means one player is always the critical node.
If that player has bad connectivity, everyone suffers. A future dedicated
server mode would:

- Run the game engine headless on a VPS (the engine already has a
  `-nographics` or similar path for dedicated servers on desktop)
- All players connect to the server as clients (pure star, no NAT issues)
- The relay server could double as the game host, eliminating relay
  entirely since the server has a public IP
- Requires: headless build for Linux, auto-start via command line,
  launcher UI to provision/select dedicated servers
- This is a significant feature but the architecture supports it: the
  auto-host path (Phase 1) already starts the engine in host mode
  from external parameters
- TODO: investigate headless engine build, server provisioning UI

---

## Lobby and Game Data Channel Relationship

### The Lobby Channel Is NOT a Wrapper for Game Data

The lobby channel and the game engine's UDP channel are separate:

```
LOBBY PHASE (before game launch):
  Kotlin lobby <--lobby protocol (UDP 42400 or WebSocket)--> peers/server
  Purpose: discovery, player management, file hashing, readiness, config

GAME PHASE (after "Start Game"):
  Game engine <--engine protocol (UDP 42424)--> peers (direct or via proxy)
  Purpose: gameplay packets (PDATA, MDATA, PING, etc.)

  Kotlin proxy (internet only):
    engine localhost:4243x <--loopback--> Kotlin <--real UDP--> peer
```

The lobby channel shuts down (or goes idle) when the game launches.
The game engine opens its own socket on port 42424 and runs its own
protocol. The two never share a socket or a packet format.

### Why Not Wrap?

Wrapping game packets inside lobby packets would mean:
- Every game packet passes through Kotlin serialization/deserialization
- The lobby becomes a performance-critical path (it isn't designed for this)
- No benefit: the engine already has a complete, tested UDP protocol

The Kotlin localhost proxy (for internet mode) is different from wrapping.
The proxy is a dumb pipe at the UDP level -- it copies raw datagrams
between the loopback socket and the real socket without inspecting or
modifying them. Zero overhead beyond a memcpy.

### Out-of-Band Messages During Gameplay

Some lobby-level concerns persist during gameplay:
- Connection health monitoring (is the holepunch still alive?)
- NAT keepalive pings every 15 seconds
- Drop/reconnect coordination if a peer disappears

These are handled by the Kotlin proxy layer, not by injecting OOB
packets into the game stream. The proxy coroutine can monitor its own
sockets and send keepalives independently. If a peer drops, the proxy
can signal the lobby (which may still be alive in the background) or
notify the user via Android UI.

For LAN mode, there is no proxy and no OOB channel during gameplay.
The engine handles disconnection natively via its own timeout mechanism.

---

## Pre-Connection File Hash Verification (Detail)

### When Hashing Happens

File hash verification is a lobby-phase operation. It completes entirely
before "Start Game" is pressed. The flow:

```
1. Player joins lobby
2. Lobby sends FILE_HASH_REQ to the new player
   (LAN: direct UDP to peer; Internet: via WebSocket)
3. Player computes hashes of their game files and responds
4. Host compares against their own hashes
5. Lobby UI shows match/mismatch per file
6. Mismatches block readiness (configurable by host)
```

### What Files Are Hashed

| File Pattern | Required | Description |
|--------------|----------|-------------|
| `descent2.hog` | Yes | Main game HOG (levels, textures) |
| `descent2.ham` | Yes | Object definitions |
| `descent2.s11` / `descent2.s22` | If present | Expanded mission data |
| `d2x.hog` | If present | Redux-specific data |
| `*.mn2` / `*.msn` | Selected mission only | Mission definition |
| `*.hog` (mission) | Selected mission only | Mission-specific HOG |

Base game files (descent2.hog, descent2.ham) are hashed to detect version
mismatches (e.g., demo vs retail, v1.0 vs v1.2). Mission files are hashed
to ensure everyone is playing the same custom level.

### Hash Implementation

SHA-256 via a minimal C implementation in jni_file_hash.c (~300 lines,
public domain). Exposed via JNI:

```c
// Returns JSON: [{"name":"descent2.hog","sha256":"abcd...","size":1234}, ...]
JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeHashGameFiles(
    JNIEnv *env, jobject obj, jstring filesDir);
```

Files are small enough (1-24 MB) that hashing takes <1 second on any phone.
Hashes can be cached by filename + size + mtime and invalidated when files
change.

### Lobby Hash Protocol Packets

#### LAN Mode (UDP on port 42400)

```
FILE_HASH_REQ (host -> joining player):
  [type: u8 = 0x10]
  [lobby_id: 16 bytes (UUID)]
  [hash_flags: u8]   // bit 0: require base game match
                      //        bit 1: require mission match
  [mission_name: 32 bytes, null-terminated]

FILE_HASH_RESP (player -> host):
  [type: u8 = 0x11]
  [lobby_id: 16 bytes (UUID)]
  [file_count: u8]
  For each file:
    [name_len: u8]
    [name: variable, UTF-8]
    [sha256: 32 bytes]
    [size: u32, little-endian]
```

#### Internet Mode (WebSocket JSON)

```json
// Client -> Server -> Host
{"type": "FILE_HASHES", "hashes": [
  {"name": "descent2.hog", "sha256": "a1b2c3...", "size": 7654321},
  {"name": "mars.mn2",     "sha256": "d4e5f6...", "size": 4567}
]}

// Host -> Server -> All
{"type": "FILE_STATUS", "players": [
  {"id": "player123", "status": "match"},
  {"id": "player456", "status": "mismatch",
   "mismatched": ["descent2.hog"]}
]}
```

### Mismatch Handling

| Scenario | Action |
|----------|--------|
| Base game version mismatch | Warning in lobby, host can override |
| Mission file missing | Offer transfer (Phase 6) for 3rd-party missions |
| Mission file hash mismatch | Block join -- different level versions |
| Extra files present | Ignored (not a problem) |
| Player has demo, host has retail | Hard block -- game will crash/desync |

---

## Lobby Ping Display

### What Pings to Show

The lobby should display three types of latency to help players
understand their connection quality before starting a game:

1. **Ping to relay server** -- always measured during internet play
2. **Ping to host** -- always measured (both LAN and internet)
3. **Ping to each peer** -- measured after holepunch establishes direct path

### Measurement Method

**Relay server ping:** Measured over the WebSocket connection. The client
sends a `PING_REQ {timestamp_ms}` message, the server echoes it back as
`PING_RESP {timestamp_ms}`. Round-trip time = now - timestamp_ms.
Measured every 5 seconds. Displayed as "Server: 45ms" in lobby.

**Host/peer ping:** Measured over UDP. During the lobby phase, the Kotlin
lobby service sends lightweight ping packets on port 42400:

```
LOBBY_PING (any player -> any player):
  [type: u8 = 0x20]
  [lobby_id: 16 bytes (UUID)]
  [sender_slot: u8]
  [timestamp_ms: u64, little-endian]

LOBBY_PONG (responding player -> sender):
  [type: u8 = 0x21]
  [lobby_id: 16 bytes (UUID)]
  [sender_slot: u8]
  [echo_timestamp_ms: u64]
```

RTT = now - echo_timestamp_ms. Measured every 2 seconds to each peer.

For internet mode, peer pings go through the localhost proxy path
(or relay), so they reflect the actual game latency including NAT
traversal and relay overhead.

### Display in Lobby UI

```
Players:
  [Host] PlayerOne     -- (you)
  [ OK ] PlayerTwo     32ms  (direct)
  [ OK ] PlayerThree   78ms  (relay)
  [    ] PlayerFour    ---   (connecting...)

Server: 45ms
```

| Element | Source | Color Coding |
|---------|--------|-------------|
| Per-peer ping | UDP LOBBY_PING/PONG | Green <50ms, Yellow 50-100ms, Red >100ms |
| Connection type | Holepunch result | "direct" or "relay" |
| Server ping | WebSocket PING_REQ/RESP | Same color coding |

### Ping During Gameplay

Once the game launches, the engine's own UPID_PING/PONG takes over for
in-game latency display. The lobby pings stop. The engine already tracks
per-player ping internally (`Netgame.players[n].ping` field) but the
in-game HUD display for this could be improved (future work).

---

## Phase 7: Voice Chat -- NOT STARTED

### Goal

Optional push-to-talk voice chat between players, usable in both lobby
and in-game. Voice data travels the same path as game data (direct UDP
or relay), keeping the architecture simple.

### Audio Codec: Opus via libopus

Opus is the standard for real-time voice in games. Properties:
- 6-510 kbps (we'd use ~16-24 kbps for voice)
- ~150KB compiled for ARM
- BSD-licensed
- Built for packet loss and variable bandwidth
- Standard in Discord, WebRTC, Steam Voice, etc.

### Build Integration

Add to android/app/src/main/cpp/CMakeLists.txt via FetchContent:

```cmake
FetchContent_Declare(
    opus
    GIT_REPOSITORY https://gitlab.xiph.org/xiph/opus.git
    GIT_TAG        v1.5.2
)
set(OPUS_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(opus)
target_link_libraries(main opus)
```

### Voice Packet Format

```
VOICE_DATA (player -> all or player -> player):
  [type: u8 = UPID_VOICE]      // new UPID constant
  [sender_slot: u8]             // which player is speaking
  [sequence: u16]               // for ordering, jitter buffer
  [opus_frame_len: u16]         // length of encoded audio
  [opus_frame: variable]        // Opus-encoded audio data

Typical size: ~60-100 bytes per 20ms frame at 24 kbps
Rate: 50 packets/sec while talking (20ms frames)
Bandwidth: ~3-5 KB/s per speaking player
```

### Audio Capture and Playback (Android)

**Capture:** Android `AudioRecord` with `MediaRecorder.AudioSource.VOICE_COMMUNICATION`
at 16kHz mono, 16-bit PCM. `VOICE_COMMUNICATION` enables the platform's
acoustic echo canceler (AEC) and noise suppressor automatically on most
devices. Controlled via `android.media.audiofx.AcousticEchoCanceler`.

**Playback:** `AudioTrack` in low-latency mode (`AudioAttributes.USAGE_GAME`
with `PERFORMANCE_MODE_LOW_LATENCY`). One AudioTrack per speaking player,
mixed by Android's audio pipeline.

**Jitter buffer:** Simple 3-frame (60ms) buffer per player. Frames
arrive with sequence numbers; buffer reorders and outputs at steady 20ms
intervals. Late frames are dropped. This absorbs typical network jitter
without adding perceptible delay.

### Push-to-Talk Activation

A dedicated on-screen touch button (part of the game overlay, not the
engine UI). When held:
1. AudioRecord starts capturing 20ms frames
2. Each frame is Opus-encoded via JNI call to libopus
3. Encoded frame is sent as VOICE_DATA to all peers (or host for relay)
4. Release stops capture, sends a VOICE_STOP packet

### Voice in Lobby vs In-Game

| Phase | Transport | Control |
|-------|-----------|---------|
| Lobby | UDP on lobby port 42400 (LAN) or via relay (internet) | Kotlin AudioRecord/AudioTrack + JNI to libopus |
| In-game | UDP on game port 42424 via localhost proxy | Same Kotlin audio pipeline, packets injected via proxy |

In-game voice packets are injected by the Kotlin proxy layer alongside
game packets. The proxy recognizes UPID_VOICE and routes to the audio
pipeline instead of to the game engine. The engine never sees voice packets.

### Files to Create

- android/app/src/main/java/com/dxxredux/app/voice/VoiceCapture.kt
- android/app/src/main/java/com/dxxredux/app/voice/VoicePlayback.kt
- android/app/src/main/java/com/dxxredux/app/voice/VoiceCodec.kt (JNI wrapper)
- android/app/src/main/cpp/jni_opus.c (JNI bridge to libopus)

### Files to Modify

- android/app/src/main/cpp/CMakeLists.txt (FetchContent for opus)
- android/app/src/main/AndroidManifest.xml (RECORD_AUDIO permission)
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyScreen.kt (PTT button)
- d2/include/multi.h (UPID_VOICE constant, shared with Kotlin)

---

## Phase 8: Launcher Game Presets (Full Server Setup from Launcher) -- NOT STARTED

### Goal

The launcher configures all game server settings before launch. Players
never need to navigate the in-game setup menu tree. The host selects
a preset or customizes settings in the lobby UI, and these are written
as a .ngs file that the engine loads automatically on startup.

### How It Works

The .ngs preset file format already exists (playsave.c). The launcher
writes a .ngs file, then passes its path via JNI. The engine's auto-host
path calls `netgame_set_defaults()` then `read_netgame_settings_file()`
to load the preset, then starts the game.

```
Launcher flow:
1. Host configures settings in lobby Compose UI
2. Launcher writes /files/lobby_preset.ngs (key=value text)
3. nativeAutoHost(presetPath, missionName, levelNum, ...)
4. Engine: netgame_set_defaults() -> read_netgame_settings_file(presetPath)
5. Engine: start game with loaded settings

Joining flow:
1. Joiners receive settings summary in lobby (display only)
2. nativeAutoJoin(hostAddr, port, ...)
3. Engine: join game, receive Netgame struct from host via SYNC
```

### .ngs File Format Reference

Plain text, one `key=value` per line. Integer values parsed with strtol.
The only string field is `game_name` (only in .ngp per-player profiles,
not in .ngs presets -- presets use `no_name=1`).

### Complete Settable Fields (42 keys)

| Key | Type | Default | UI Widget | Description |
|-----|------|---------|-----------|-------------|
| `gamemode` | u8 | 0 | Radio | 0=Anarchy 1=Team 2=RoboAnarchy 3=Coop 4=CTF 5=Hoard 6=TeamHoard 7=Bounty |
| `RefusePlayers` | u8 | 0 | Toggle | Restricted game mode |
| `difficulty` | u8 | player default | Slider 0-4 | Trainee through Insane |
| `max_numplayers` | u8 | 8 | Slider 2-8 | Max 4 for coop |
| `max_numobservers` | u8 | 0 | Slider 0-14 | Increments of 2 |
| `game_flags` | u8 | 0 | Bitmask | NETGAME_FLAG_CLOSED etc. |
| `AllowedItems` | u32 | NETFLAG_DOPOWERUP | Checkboxes | 27 powerup flags |
| `Allow_marker_view` | i16 | 1 | Toggle | |
| `AlwaysLighting` | i16 | 0 | Toggle | Indestructible lights |
| `ShowEnemyNames` | i16 | 0 | Toggle | |
| `BrightPlayers` | i16 | 1 | Toggle | |
| `SpawnStyle` | u8 | 1 | Radio | 0=None 1=HalfSec 2=TwoSec 3=Preview |
| `NewSpawnAlgorithm` | u8 | 0 | Toggle | |
| `GaussAmmoStyle` | u8 | 4 | Radio | 1=Dup 2=Deplete 3=Drop 4=Respawn |
| `KillGoal` | i32 | 0 | Slider 0-10 | x10 kills |
| `PlayTimeAllowed` | fix | 0 | Slider 0-10 | x5 minutes |
| `control_invul_time` | i32 | 0 | Slider 0-10 | Reactor invuln minutes |
| `PacketsPerSec` | i16 | 20 | Slider 10-30 | |
| `ShortPackets` | u8 | 0 | Toggle | |
| `NoFriendlyFire` | u8 | 0 | Toggle | Team/coop only |
| `RetroProtocol` | u8 | 1 | Toggle | P2P protocol |
| `RespawnConcs` | u8 | 0 | Toggle | |
| `LowVulcan` | u8 | 0 | Toggle | |
| `AllowPreferredColors` | u8 | 0 | Toggle | |
| `AllowColoredLighting` | u8 | 0 | Toggle | |
| `FairColors` | u8 | 0 | Toggle | All players blue |
| `BlackAndWhitePyros` | u8 | 1 | Toggle | Alternate ship colors 6/7 |
| `BornWithBurner` | u8 | 0 | Toggle | |
| `OriginalD1Weapons` | u8 | 0 | Toggle | |
| `RebalancedWeapons` | u8 | 0 | Toggle | Beta feature |
| `PrimaryDupFactor` | u8 | 0 | Slider 0-3 | Extra primary x1/x2/x3/x4 |
| `SecondaryDupFactor` | u8 | 0 | Slider 0-3 | Extra secondary x1/x2/x3/x4 |
| `SecondaryCapFactor` | u8 | 0 | Slider 0-2 | Uncapped/MaxSix/MaxTwo |
| `obs_delay` | u8 | 0 | Toggle | Broadcast delay for observers |
| `obs_min` | u8 | 0 | Toggle | Minimal observer info |
| `HomingUpdateRate` | u8 | 25 | Slider 20-30 | Stored as-is, menu shows +20 |
| `RemoteHitSpark` | u8 | 0 | Toggle | |
| `AllowCustomModelsTextures` | u8 | 0 | Toggle | |
| `ReducedFlash` | u8 | 0 | Toggle | |
| `DisableGaussSplash` | u8 | 0 | Toggle | |
| `Tracker` | u8 | 1 | Toggle | Only when USE_TRACKER defined |

### Launcher UI Organization

Group settings into tabs or expandable sections matching the engine's
menu structure:

1. **Game Setup** -- mode, difficulty, players, observers, access
2. **Gameplay** -- kill goal, time limit, reactor, spawn style, friendly fire
3. **Weapons** -- allowed items (27 checkboxes), duplication, caps, ammo style
4. **Visuals** -- colors, lighting, markers, bright ships, HUD options
5. **Network** -- packets/sec, short packets, RetroProtocol, tracker

### Built-in Presets

Ship a few .ngs files with the app:

| Preset Name | Description |
|-------------|-------------|
| `coop_standard.ngs` | Cooperative, 4 players, default weapons |
| `anarchy_classic.ngs` | Anarchy, 8 players, all weapons |
| `anarchy_competitive.ngs` | Anarchy, balanced weapons, no respawn concussions |
| `team_ctf.ngs` | CTF, fair colors, team-appropriate settings |
| `casual_coop.ngs` | Coop, easy difficulty, extra lives |

Users can save custom presets from the launcher UI. These are stored
in the app's files directory and listed alongside built-in presets.

### JNI Interface Extension

```c
// Auto-host with a preset file
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutoHost(
    JNIEnv *env, jobject obj,
    jstring presetPath,    // path to .ngs file (null = defaults)
    jstring missionName,   // e.g. "Descent: First Strike"
    jint levelNum,         // starting level
    jstring gameName,      // display name for the game
    jint maxPlayers);      // override for max_numplayers
```

### Files to Create

- android/app/src/main/java/com/dxxredux/app/lobby/GameSettingsScreen.kt
- android/app/src/main/java/com/dxxredux/app/lobby/PresetManager.kt
- android/app/src/main/assets/presets/coop_standard.ngs
- android/app/src/main/assets/presets/anarchy_classic.ngs
  (and other built-in presets)

### Files to Modify

- android/app/src/main/cpp/jni_main.c (nativeAutoHost preset parameter)
- d2/main/net_udp.c (auto-host reads preset before starting)
- d1/main/net_udp.c (same)

---

## Appendix A: STUN and NAT Traversal Libraries

### Option 1: Custom Kotlin STUN (Recommended for Initial Implementation)

STUN binding requests are ~100 lines of code:
- Build 20-byte request (type 0x0001, magic cookie 0x2112A442, random txn ID)
- Send to self-hosted STUN server (addresses received in AUTH_OK)
- Parse response: find XOR-MAPPED-ADDRESS (attr type 0x0020), XOR with
  magic cookie to get reflexive IP:port
- Two queries to the two self-hosted STUN ports detects symmetric NAT
  (ports differ)

Pros: zero dependencies, tiny code, full control, no reliance on
  third-party STUN servers, IP allowlisting prevents abuse.
Cons: no ICE, no TURN, no candidate gathering beyond basic reflexive.

### Option 2: libjuice (Recommended for Full ICE)

libjuice (JUICE Is a UDP Interactive Connectivity Establishment library):
- Pure C, ~5,000 lines, no dependencies
- MPL-2.0 license (compatible with our project)
- Full ICE-LITE implementation: STUN, candidate gathering, connectivity checks
- Optional TURN client support
- Used by libdatachannel (WebRTC for C/C++)

Build integration via CMake FetchContent:

```cmake
FetchContent_Declare(
    libjuice
    GIT_REPOSITORY https://github.com/paullouisageneau/libjuice.git
    GIT_TAG        v1.5.4
)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_SERVER ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libjuice)
target_link_libraries(main juice)
```

JNI bridge needed to expose to Kotlin. Or use from C directly if the
lobby connection handling moves to the native side (less likely).

Pros: proper ICE with TURN fallback, well-maintained, small footprint.
Cons: adds a C dependency, JNI bridge overhead, more complex than needed
if we only need basic STUN.

### Option 3: Platform STUN (Android IceCandidate via WebRTC)

Android's WebRTC library includes full ICE. But:
- Massive dependency (~10MB)
- Overkill for our use case
- Tight coupling to WebRTC data channels

Not recommended.

### Recommended Path

Start with custom Kotlin STUN (Option 1) for Phase 4. It handles the
common case (cone NAT holepunch). Add libjuice (Option 2) later if we
need TURN fallback to third-party TURN servers instead of our own relay.

---

## Appendix B: Rust Relay Server Details

### Crate Stack (pinned versions)

```toml
[dependencies]
tokio = { version = "1.43", features = ["full"] }
tokio-tungstenite = { version = "0.26", features = ["rustls-tls-webpki-roots"] }
rustls = "0.23"
serde = { version = "1.0", features = ["derive"] }
serde_json = "1.0"
reqwest = { version = "0.12", features = ["json", "rustls-tls"], default_features = false }
dashmap = "6.1"
ring = "0.17"
tracing = "0.1"
tracing-subscriber = { version = "0.3", features = ["env-filter"] }
rcgen = "0.13"          # for development/test cert generation
tokio-rustls = "0.26"
```

### Server Architecture Sketch

```
main.rs:
  - Load config (env vars: LISTEN_ADDR, TLS_CERT_PATH, TLS_KEY_PATH,
    GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET)
  - Bind TCP listener for WebSocket (TLS via rustls)
  - Bind UDP socket for relay
  - Spawn ws_acceptor task (accepts WebSocket connections)
  - Spawn relay_task (reads UDP relay packets, forwards)

ws_handler.rs:
  - Per-connection task: read messages in a loop, dispatch to handler
  - Handlers modify shared state via DashMap<LobbyId, Lobby>
  - Broadcast to lobby members via per-player mpsc channels

lobby.rs:
  struct Lobby {
      id: Uuid,
      host: PlayerId,
      players: Vec<LobbyPlayer>,  // max 8
      settings: GameSettings,     // mirrors .ngs fields
      state: LobbyState,         // Waiting, Holepunching, Starting, InGame
  }
  struct LobbyPlayer {
      id: PlayerId,
      callsign: String,
      identity: Identity,        // GPGS or keypair
      ready: bool,
      stun_result: Option<StunResult>,
      connection_type: ConnectionType,  // Direct, Relay, Unknown
      ping_ms: Option<u32>,
      file_status: FileMatchStatus,
  }

relay.rs:
  - UDP socket receives: [session_token:4][dest_player:1][payload...]
  - Lookup session_token in DashMap<SessionToken, RelaySession>
  - RelaySession contains player_id -> SocketAddr mapping
  - Forward payload to dest_player's address with
    [session_token:4][from_player:1][payload...]
  - Rate limit per session: token bucket, 50 KB/s per player pair
  - Session timeout: 2 hours, or 5 minutes of inactivity
```

### Deployment

- Single binary, statically linked with musl for Linux
- Docker container: `FROM scratch`, just the binary + TLS certs
- Let's Encrypt via certbot (cron renewal) or acme-client crate
- SystemD unit file for bare-metal VPS deployment
- Health endpoint on a separate HTTP port for monitoring

### Scaling

At 10-15 KB/s per player pair, a single VPS with 1 Gbps can relay
~8,000 simultaneous player pairs. WebSocket connections are lightweight
(~2KB each in memory). The bottleneck is relay bandwidth, not CPU or memory.

If the player base grows beyond one server:
- Add regions (US-East, EU-West, Asia) with separate server instances
- Lobby list queries go to all regions, sorted by ping
- Cross-region play: player connects to their nearest server, servers
  don't communicate (players pick a server, all must use the same one)

---

## Appendix C: Server Authentication and PKI

### TLS for WebSocket Connections

All WebSocket connections use TLS (WSS). The server presents a valid
certificate from Let's Encrypt. This provides:
- Encryption: lobby/signaling data is confidential
- Server authentication: clients verify they're talking to our server
- Integrity: no man-in-the-middle on signaling messages

### Certificate Management

**Option 1: Let's Encrypt with certbot (recommended)**

```bash
# Initial setup
certbot certonly --standalone -d matchmaking.dxx-redux.com
# Auto-renewal via cron
0 0 * * * certbot renew --deploy-hook "systemctl restart matchmaking-server"
```

The Rust server loads the cert and key from disk on startup and on
SIGHUP for live reload.

**Option 2: acme-client crate (ACME in-process)**

The server handles ACME challenges directly. More complex but avoids
the certbot dependency. Libraries: `instant-acme` (async ACME client).
Probably not worth the complexity for a single server.

### Client Certificate Pinning (Optional Hardening)

The Android app can pin the server's public key (SPKI hash) to prevent
MITM even if a CA is compromised. Add the pin to the app's
`network_security_config.xml`:

```xml
<network-security-config>
  <domain-config>
    <domain includeSubdomains="false">matchmaking.dxx-redux.com</domain>
    <pin-set expiration="2027-01-01">
      <pin digest="SHA-256">base64encodedSPKIhash=</pin>
    </pin-set>
  </domain-config>
</network-security-config>
```

Pinning requires updating the app when certificates rotate. Let's Encrypt
certs last 90 days. Pin the intermediate CA's key instead of the leaf
cert's key to avoid frequent rotations (ISRG Root X1 key is stable).

### UDP Relay Authentication

The relay uses session tokens (4 bytes) assigned during the WebSocket
signaling phase. This is not cryptographic auth -- it's a session
identifier. Security properties:

- Tokens are random, assigned by the server, and short-lived
- An attacker who guesses a token can inject relay traffic, but the
  game engine validates game-level packet integrity (checksums, sequence
  numbers) so injected garbage is dropped
- For stronger relay auth: extend the token to 16 bytes and HMAC the
  packet payload with a per-session key derived from the WebSocket
  handshake. Cost: ~32 bytes overhead per packet. Probably not needed
  for v1 -- the game's own packet validation is sufficient.

### Identity Verification Flow (GPGS)

```
Client                    Our Server                   Google
  |                          |                            |
  | (1) GPGS sign-in         |                            |
  |------------------------->|                            |
  | gets server_auth_code    |                            |
  |                          |                            |
  | (2) AUTHENTICATE         |                            |
  |   {server_auth_code}     |                            |
  |------------------------->|                            |
  |                          | (3) POST /token            |
  |                          |   grant_type=authorization_code
  |                          |   code=server_auth_code    |
  |                          |   client_id + client_secret|
  |                          |--------------------------->|
  |                          |                            |
  |                          | (4) {id_token, access_token}|
  |                          |<---------------------------|
  |                          |                            |
  |                          | (5) Decode id_token (JWT)  |
  |                          |   -> gpgs_player_id, email |
  |                          |   Verify signature with    |
  |                          |   Google's public keys     |
  |                          |                            |
  | (6) AUTH_OK              |                            |
  |   {player_id, session}   |                            |
  |<-------------------------|                            |
```

The server caches Google's public keys (JWKS endpoint) and refreshes
them periodically. Token exchange takes ~100-200ms (one HTTP round-trip
to Google). Session tokens are then used for all subsequent WebSocket
messages -- no re-authentication needed per message.

---

## Implementation Order (Revised)

1. LAN lobby + auto-join (Phase 1) -- standalone, no server needed
2. Launcher game presets (Phase 8) -- .ngs writing, no server needed,
   pairs naturally with Phase 1 since auto-host needs settings
3. File hash verification (Phase 6) -- lobby-phase feature, no server needed
4. Matchmaking server skeleton (Phase 2) -- Rust, WebSocket, lobby mgmt
5. GPGS authentication (Phase 3) -- identity before opening to internet
6. STUN + holepunch + relay (Phase 4) -- internet play works end-to-end
7. Drop-in/drop-out co-op (Phase 5) -- engine changes
8. Voice chat (Phase 7) -- requires UDP infrastructure from earlier phases

Independent tracks (can proceed in parallel):
- Track A: Phases 1, 8, 6 (all local, no server)
- Track B: Phases 2, 3, 4 (server, sequential)
- Phase 5 can start anytime after Phase 1
- Phase 7 requires Phase 1's infrastructure at minimum

---

## Phase 9: Protocol Versioning and Client Rejection

### Goal

The server must be able to reject outdated clients with a clear,
actionable error message so players know exactly what to do.

### Version Handshake

Every WebSocket connection begins with an AUTHENTICATE message. Extend
it to include version information:

```
Client -> Server:
  AUTHENTICATE {
    protocol_version: u32,        // monotonically increasing, e.g. 1
    client_version: string,       // human-readable, e.g. "1.0.3"
    min_server_version: u32,      // oldest server this client supports
    play_games_token: string,
    callsign: string,
    platform: string,             // "android", "windows", "linux"
    build_type: string            // "release", "debug"
  }
```

The server checks `protocol_version` against its own supported range:

```
Server config:
  min_client_protocol: u32       // oldest client the server will accept
  current_protocol: u32          // server's own protocol version
  update_url: string             // link to update instructions/store page
  deprecation_message: string    // optional custom message
```

### Rejection Response

```
Server -> Client:
  VERSION_REJECTED {
    reason: string,               // human-readable explanation
    required_version: u32,        // minimum protocol version needed
    required_version_name: string,// e.g. "1.2.0"
    current_server_version: u32,
    update_url: string,           // Play Store link or website URL
    grace_period_end: string      // ISO 8601 date, if applicable
  }
```

Example reason strings:
- "Your client is too old (v1.0.3). Please update to v1.2.0 or later."
- "This server requires protocol version 3. Your client uses version 1.
   Update the app from the Play Store."
- "Your client version will stop working on 2026-05-01. Please update."

### Soft Deprecation

Before hard-rejecting old clients, the server can send a warning:

```
Server -> Client:
  VERSION_WARNING {
    message: string,              // "Update available: v1.3.0"
    update_url: string,
    deadline: string              // when this version stops working
  }
```

The launcher displays this as a non-blocking banner in the lobby UI.
After the deadline, the server switches to hard rejection.

### Protocol Version Bump Rules

Bump the protocol version when:
- WebSocket message schema changes (new required fields, changed types)
- Relay packet format changes
- Lobby protocol changes that break backward compatibility
- Authentication flow changes

Do NOT bump for:
- New optional fields in existing messages
- New message types (old clients ignore unknown types)
- Server-side-only changes

### Version Constants

Shared between client and server. On the client side, defined in:
- `android/app/src/main/java/com/dxxredux/app/lobby/NetworkConstants.kt`

On the server side, defined in:
- `server/src/protocol.rs`

Both files document that they must be kept in sync.

### Minecraft Reference

Minecraft's approach: the handshake packet includes a protocol version
number. The server responds with human-readable JSON:
`{"text": "Outdated client! Please use 1.20.4"}` which is displayed
in the server browser. The version string and protocol number are
separate -- the string is for humans, the number is for code.

We follow the same pattern: `protocol_version` for code decisions,
`client_version` for display.

---

## Phase 10: Friends List and Online Status

### Goal

Players can maintain a friends list and see whether friends are online,
in a lobby, or in-game. Identity is based on a server-assigned opaque
player ID derived from (but not directly exposing) the Google Play
Games ID.

### Identity Mapping

The server assigns each authenticated player an opaque `player_id`
(UUID v4 or similar). This ID is:
- Derived from the GPGS player ID at first login (one-to-one mapping)
- Stored server-side in a `players` table
- Used for all social features (friends, stats, leaderboards)
- Shared with other players for friend requests and display
- NOT the raw GPGS player ID (privacy: prevents cross-referencing
  with other games or Google services)

```
Server DB table: players
  player_id      UUID PRIMARY KEY     -- our opaque ID
  gpgs_player_id TEXT UNIQUE          -- Google's ID, never shared externally
  callsign       TEXT                 -- display name
  created_at     TIMESTAMP
  last_seen      TIMESTAMP
  total_playtime INTEGER              -- seconds
  games_played   INTEGER
```

### Friends Data Model

```
Server DB table: friends
  player_id   UUID REFERENCES players
  friend_id   UUID REFERENCES players
  status       TEXT  -- 'pending', 'accepted', 'blocked'
  created_at   TIMESTAMP
  PRIMARY KEY (player_id, friend_id)
```

Friendships are symmetric: when A adds B, both get a row with status
'pending'. When B accepts, both rows flip to 'accepted'. Either side
can remove (delete both rows) or block (set own row to 'blocked').

### Presence Tracking

The server tracks presence as an in-memory enum per connected player:

```rust
enum Presence {
    Offline,                          // no WebSocket connected
    Online,                           // connected, not in lobby or game
    InLobby { lobby_id: Uuid },       // sitting in a lobby
    InGame  { lobby_id: Uuid, mission: String, player_count: u8 },
}
```

Presence updates happen automatically:
- WebSocket connect -> Online
- JOIN_LOBBY -> InLobby
- START_GAME -> InGame
- WebSocket disconnect -> Offline (with last_seen timestamp update)

No explicit presence messages from the client are needed. The server
infers presence from lobby/game state changes.

### Friend Request Protocol

```
Client -> Server:
  FRIEND_REQUEST    { target_callsign: string }
    -- server resolves callsign to player_id
    -- or: FRIEND_REQUEST { target_player_id: UUID }
    -- both work; callsign is for convenience

  FRIEND_ACCEPT     { player_id: UUID }
  FRIEND_REMOVE     { player_id: UUID }
  FRIEND_BLOCK      { player_id: UUID }
  FRIEND_LIST       {}                      -- request full list

Server -> Client:
  FRIEND_LIST_RESP  {
    friends: [{
      player_id: UUID,
      callsign: string,
      status: "pending" | "accepted" | "blocked",
      presence: "offline" | "online" | "in_lobby" | "in_game",
      in_game_details: null | {
        lobby_id: UUID,
        mission: string,
        player_count: u8,
        max_players: u8,
        joinable: bool        -- true if game is not full and not locked
      },
      last_seen: string       -- ISO 8601, only set when offline
    }]
  }

  FRIEND_REQUEST_RECEIVED { from_player_id: UUID, from_callsign: string }
  FRIEND_ACCEPTED         { player_id: UUID }
  FRIEND_REMOVED          { player_id: UUID }
  FRIEND_PRESENCE_UPDATE  { player_id: UUID, presence: ..., details: ... }
```

### Presence Push

When a player's presence changes, the server pushes
`FRIEND_PRESENCE_UPDATE` to all online friends. This keeps the friends
list UI live without polling.

### Privacy Considerations

- The server never shares GPGS player IDs with other players
- Players are identified by callsign + our opaque UUID
- Blocked players cannot see your presence or send requests
- Friend list data is only visible to the account owner
- No discovery/search by GPGS player ID -- only by callsign
- Callsign search is rate-limited to prevent enumeration (5/min)

### Storage

Friends and players tables stored in SQLite on the server. At our
scale (hundreds of players), SQLite handles all reads/writes without
contention. WAL mode for concurrent read/write.

---

## Phase 11: Join Friend's Game

### Goal

A player can see that a friend is in a joinable game and join it
directly from the friends list, without browsing the lobby list.

### Flow

```
1. Client opens friends list
2. Sees: "PlayerTwo - In Game (Counterstrike L3, 2/4 players)"
3. Taps "Join"
4. Client sends JOIN_FRIEND_GAME { friend_player_id }
5. Server checks:
   a. Friend is actually in a game
   b. Game is not full
   c. Game is not locked (RefusePlayers)
   d. Requesting player is not banned from host's lobbies
6. Server responds with JOIN_FRIEND_GAME_RESP:
   - On success: same data as JOIN_LOBBY response (lobby details,
     peer addresses, connection instructions)
   - On failure: reason ("Game is full", "Game is locked",
     "Player not found", etc.)
7. Client proceeds through normal lobby join flow (holepunch, etc.)
   just as if they'd joined from the lobby list
```

### Server-Side

```
Client -> Server:
  JOIN_FRIEND_GAME { friend_player_id: UUID }

Server -> Client:
  JOIN_FRIEND_GAME_RESP {
    success: bool,
    reason: string,          // only on failure
    lobby_id: UUID,          // only on success
    // ...rest is same as LOBBY_UPDATE
  }
```

If successful, the server internally performs a JOIN_LOBBY for the
requesting player, so all existing lobby mechanics (file hash check,
readiness, etc.) apply.

### Host Notification

The host receives a LOBBY_UPDATE showing the new player joined, same
as a regular join. The host doesn't know (or need to know) that the
player joined via friends list vs lobby browser.

### Mid-Game Join

If the game is already running (not just in lobby), this becomes a
mid-game join (Phase 5's drop-in functionality). The flow is the same
but the server also coordinates NAT traversal and tells the joining
client to connect to the game in progress rather than waiting in a
lobby. The engine's existing rejoin/late-join mechanism handles the
game-level sync.

---

## Phase 12: Player Count Tracking and Public API

### Goal

Track concurrent player counts over time. Expose current count via a
lightweight HTTP API for embedding in web pages, Discord bots, etc.

### Server-Side Tracking

The server already knows how many WebSocket connections are alive. Add:

```rust
struct ServerStats {
    current_online: AtomicU32,       // WebSocket connections
    current_in_game: AtomicU32,      // players in active games
    current_lobbies: AtomicU32,      // active lobbies
    peak_online_today: AtomicU32,
    peak_online_alltime: AtomicU32,
    total_unique_players: u64,       // from players table COUNT
    total_games_played: u64,         // from match_results table COUNT
}
```

Periodic snapshots (every 5 minutes) written to a `player_counts`
table for historical graphing:

```
Server DB table: player_count_history
  timestamp    TIMESTAMP PRIMARY KEY
  online       INTEGER
  in_game      INTEGER
  lobbies      INTEGER
```

### HTTP API

A lightweight HTTP server (using `hyper` or `axum`, bound to a separate
port, e.g. 8080) serves public status data:

```
GET /api/v1/status
Response (JSON):
{
  "server_version": "1.0.0",
  "protocol_version": 1,
  "uptime_seconds": 86400,
  "players": {
    "online": 23,
    "in_game": 18,
    "in_lobbies": 5,
    "peak_today": 42,
    "peak_alltime": 127
  },
  "lobbies": {
    "count": 3,
    "games": [
      {
        "lobby_id": "...",
        "host_callsign": "PlayerOne",
        "mission": "Counterstrike!",
        "level": 3,
        "mode": "coop",
        "players": 3,
        "max_players": 4,
        "joinable": true
      }
    ]
  },
  "stats": {
    "total_unique_players": 1842,
    "total_games_played": 5923,
    "total_playtime_hours": 12450
  }
}

GET /api/v1/status/simple
Response (plain text):
23
(just the online count, for minimal embedding)

GET /api/v1/status/badge
Response (SVG):
An SVG badge image showing "Players Online: 23"
suitable for embedding in README files or web pages
(shields.io compatible endpoint)
```

### Rate Limiting

- `/api/v1/status`: 60 requests/min per IP, cached for 10 seconds
- No authentication required (public data)
- CORS headers set to allow embedding from any origin

### Website Embedding Example

```html
<!-- Simple player count widget -->
<script>
fetch('https://matchmaking.dxx-redux.com/api/v1/status')
  .then(r => r.json())
  .then(d => document.getElementById('count').textContent = d.players.online);
</script>
<span id="count">...</span> players online
```

### Minecraft Reference

Minecraft's Server List Ping protocol serves exactly this purpose --
any client (or website) can query a server and get player count, MOTD,
player sample, and version info as JSON. Our HTTP API does the same
but over standard HTTP instead of a custom protocol, making web
integration trivial.

---

## Phase 13: Game Result Tracking and Logging

### Goal

Record the outcome of every completed multiplayer game for statistics,
leaderboards, and player profiles.

---

## Server Logging Infrastructure -- COMPLETE

### Stack

- `tracing` (=0.1.41) for instrumentation macros (info!, warn!, debug!, error!)
- `tracing-subscriber` (=0.3.19) with env-filter and json features for output formatting
- `tracing-appender` (=0.2.3) for file output with daily rotation and non-blocking writes

### Configuration

- `LOG_DIR` env var: set to a directory path to enable JSON file logging alongside console output
  - Empty (default): console-only logging (human-readable format with targets)
  - Set to a path: dual output -- console (human-readable) + file (JSON, daily rotation)
  - File names: `dxx-matchmaking.log.YYYY-MM-DD`
- `RUST_LOG` env var: controls log levels per module (standard tracing-subscriber env filter)
  - Default: `info`
  - Example: `RUST_LOG=debug` for all debug output
  - Example: `RUST_LOG=dxx_matchmaking::relay=trace,info` for per-packet relay logs + info elsewhere

### Log Level Strategy

| Level | Usage |
|-------|-------|
| error | Fatal: DB open failure, server bind failure, TLS cert load failure |
| warn  | Recoverable: rate limit triggers, banned player auth attempts, version rejections, admin auth failures, DB operation errors, non-host game start attempts |
| info  | Significant events: player auth/disconnect, lobby create/join/leave, game start, STUN results, connectivity results, relay allocation, friend accept/remove/block, admin ban/unban, match results, config at startup |
| debug | WS upgrade acceptance, WS read errors, serialization errors |
| trace | Reserved for per-packet relay forwarding (not currently emitted, would spam) |

### Current Coverage by Module

- **main.rs**: startup config (listen addrs, DB path, TLS/GPGS status), TLS load result, file logging init
- **ws_handler.rs**: WS upgrade rate-limit, WS upgrade accept, message rate-limit, version rejection (with versions), banned auth attempt, auth success (pid/callsign/platform), lobby create/join/leave, lobby rate-limits, game start (+ non-host rejection), STUN result (nat_type, candidate count), connectivity ok/update, player kicked, match result, friend accept/remove/block, message rate-limit, player disconnect
- **http_api.rs**: HTTP listen, admin ban (pid/reason/duration), admin unban (pid), admin auth failures
- **relay.rs**: relay listen, session lifecycle (created/allocated), per-packet forwarding at debug level
- **stats.rs**: periodic snapshot (online/in_game/lobbies/peak) every 5 min
- **friends.rs**: friend request sent

### Logging Notes for Future Phases

#### Phase 3 (Identity -- GPGS token verification)
When implementing real GPGS token verification in ws_handler.rs:
- info! on successful GPGS token exchange (player_id, gpgs_sub)
- warn! on GPGS token verification failure (error detail, token prefix for debugging)
- info! on first-time player creation vs returning player
- debug! for token exchange HTTP request timing

#### Phase 4 (NAT Traversal -- client-side)
Client-side Kotlin logging (use Android Log or timber):
- Log STUN server responses and detected NAT type (self-hosted STUN)
- Log each connectivity check attempt and result (candidate type, RTT)
- Log relay fallback trigger
- Log UPnP/PCP mapping attempts and outcomes

#### Phase 5 (Drop-In/Drop-Out Co-op)
- info! on mid-game player join/leave
- warn! on rejoin failures

#### Phase 7+ (Localhost Proxy)
- info! on proxy bind, connection established, peer address mapping
- debug! packet forwarding counts (periodic, not per-packet)
- warn! on proxy socket errors

### Match Result Record

```
Server DB table: match_results
  match_id       UUID PRIMARY KEY
  lobby_id       UUID              -- which lobby spawned this game
  started_at     TIMESTAMP
  ended_at       TIMESTAMP
  duration_secs  INTEGER
  game_mode      TEXT              -- "coop", "anarchy", "team", "ctf", etc.
  mission        TEXT              -- e.g. "Counterstrike!"
  level          INTEGER
  difficulty     INTEGER           -- 0-4
  result         TEXT              -- "completed", "aborted", "host_quit"
  player_count   INTEGER

Server DB table: match_players
  match_id       UUID REFERENCES match_results
  player_id      UUID REFERENCES players
  callsign       TEXT
  score          INTEGER
  kills          INTEGER
  deaths         INTEGER
  assists        INTEGER           -- if trackable
  team           INTEGER           -- for team modes
  result         TEXT              -- "won", "lost", "disconnected"
  connected_secs INTEGER           -- how long this player was in the game
  PRIMARY KEY (match_id, player_id)
```

### How Results Are Reported

The host's client reports match results to the server when the game
ends. The server validates that the reporting player was actually the
host of that lobby.

```
Client (host) -> Server:
  MATCH_RESULT {
    lobby_id: UUID,
    duration_secs: u32,
    result: "completed" | "aborted" | "host_quit",
    players: [{
      player_id: UUID,
      score: i32,
      kills: u32,
      deaths: u32,
      result: "won" | "lost" | "disconnected"
    }]
  }

Server -> Client (host):
  MATCH_RESULT_ACK { match_id: UUID }
```

The server also independently tracks game duration (from START_GAME to
last disconnect or MATCH_RESULT). If the host disconnects without
sending MATCH_RESULT, the server creates a partial record marked
"host_quit" with whatever data it has.

### Aggregate Player Stats

Derived from match_results + match_players, updated periodically or
on match completion:

```
Server DB table (or materialized view): player_stats
  player_id          UUID PRIMARY KEY REFERENCES players
  total_games        INTEGER
  total_wins         INTEGER
  total_kills        INTEGER
  total_deaths       INTEGER
  total_score        INTEGER
  total_playtime     INTEGER          -- seconds
  coop_completions   INTEGER
  favorite_mission   TEXT             -- most played
  last_game_at       TIMESTAMP

  -- per-mode breakdowns (stored as JSON or separate columns)
  anarchy_games      INTEGER
  anarchy_kills      INTEGER
  coop_games         INTEGER
  ctf_games          INTEGER
```

### Leaderboards

Served via the HTTP API:

```
GET /api/v1/leaderboard?mode=anarchy&sort=kills&period=monthly&limit=25
Response (JSON):
{
  "period": "2026-03",
  "mode": "anarchy",
  "sorted_by": "kills",
  "entries": [
    {"rank": 1, "callsign": "AceFlyer", "kills": 342, "deaths": 98, "games": 45},
    ...
  ]
}

GET /api/v1/player/{player_id}/stats
Response (JSON):
{
  "callsign": "AceFlyer",
  "total_games": 230,
  "total_kills": 1842,
  "total_deaths": 923,
  "kd_ratio": 2.0,
  "total_playtime_hours": 145,
  "coop_completions": 67,
  "recent_games": [...]
}
```

### Anti-Tampering

Match results are host-reported, which means a modified client could
lie about scores. Mitigations:

- Server validates that all reported player_ids were in the lobby
- Server validates that game duration roughly matches (within 2x of
  server-tracked time, to account for clock skew)
- Anomaly detection: flag results where a player's K/D jumps
  dramatically from their average
- For v1, accept the limitation. Statistical cheating is detectable
  over time and at our player scale, manual review is feasible

### Coop-Specific Tracking

For cooperative games, additional fields are useful:

- Levels completed (how far the team got)
- Total team deaths
- Whether the mission was completed (all levels) or partial
- Difficulty (higher difficulty = more impressive)

These feed into a separate "coop leaderboard" sorted by mission
completion time at each difficulty.

---

## Phase 14: Server Telemetry and Operational Tracking

### Goal

Track server health, game quality metrics, and operational data for
maintaining reliable service.

### Server Health Metrics

Tracked in-memory, exposed via the HTTP API and optionally pushed to
a monitoring system:

```
GET /api/v1/health
Response (JSON):
{
  "status": "ok",
  "uptime_seconds": 172800,
  "memory_used_mb": 45,
  "websocket_connections": 34,
  "relay_sessions": 8,
  "relay_bandwidth_kbps": 120,
  "db_size_mb": 12,
  "errors_last_hour": 0,
  "avg_auth_latency_ms": 145
}
```

### Connection Quality Tracking

For understanding player experience and debugging connectivity issues:

```
Server DB table: connection_events
  id             INTEGER PRIMARY KEY AUTOINCREMENT
  timestamp      TIMESTAMP
  player_id      UUID
  event_type     TEXT    -- "connect", "disconnect", "auth_fail",
                         -- "holepunch_success", "holepunch_fail",
                         -- "relay_assigned", "relay_timeout"
  details        TEXT    -- JSON blob with specifics
  ip_country     TEXT    -- GeoIP lookup (for regional analysis)
  nat_type       TEXT    -- from STUN result
  latency_ms     INTEGER -- to server
```

### What Minecraft Servers Track (and What We Should Too)

| Minecraft (Paper/Spigot)          | Our Equivalent                       |
|-----------------------------------|--------------------------------------|
| TPS (ticks per second)            | N/A (we don't run game logic)        |
| Player join/leave events          | connection_events table              |
| Chunk load performance            | N/A                                  |
| Memory usage + GC stats           | /health endpoint                     |
| Entity counts                     | lobby count, relay session count      |
| Plugin load times                 | auth latency, holepunch duration     |
| World save times                  | DB write latency                     |
| Player count over time            | player_count_history table           |
| Chat log                          | N/A (voice only, not logged)         |
| Command usage stats               | API endpoint hit counts              |
| Crash dumps + stack traces        | panic handler + structured logging   |

### Structured Logging

All server events logged as structured JSON (via `tracing` crate with
JSON formatter). Log levels:

- ERROR: panics, DB failures, auth backend unreachable
- WARN: rate limit hit, malformed packet, relay timeout
- INFO: player connect/disconnect, lobby create/close, game start/end
- DEBUG: individual WebSocket messages, relay packet forwarding
- TRACE: everything (development only)

Log rotation: daily files, 30-day retention, or ship to a log
aggregator if running multiple server regions.

### Uptime and Crash Recovery

- Server writes a heartbeat file every 30 seconds (for external
  monitoring watchdog)
- On startup, check for stale heartbeat = previous crash
- Recover lobby state? No -- lobbies are ephemeral. Players reconnect
  and re-create lobbies. Match results in-progress are lost (marked
  "server_crash" if partially logged).
- Player accounts and friends survive crashes (persisted in SQLite)

### Operational Alerts

For a single-server deployment, keep it simple:

- Health endpoint returns non-200 on critical failure -> uptime monitor
  (UptimeRobot, free tier) alerts via email/Discord webhook
- Disk space monitoring (SQLite + logs can grow)
- Memory usage trending (leak detection)
- Player count anomalies (sudden drop = possible outage)

### Discord Webhook Integration

Push notable events to a Discord channel for the development team:

- Server start/stop
- Player count milestones (first 10, 50, 100 concurrent)
- Match completions (optional, can be noisy)
- Error rate spikes
- New player registrations

Implementation: POST to a Discord webhook URL with a simple JSON
embed. ~10 lines of code per notification type.

---

## Phase 15: Additional Server Features

### Message of the Day (MOTD)

The server sends an MOTD to clients on connect:

```
Server -> Client (after AUTH_OK):
  MOTD {
    message: string,       // short text, e.g. "Welcome! New map pack available."
    url: string | null,    // optional link
    severity: "info" | "warning"  // warning for urgent stuff
  }
```

Displayed as a toast or banner in the launcher. Configurable via a
server config file or admin API endpoint. Useful for announcing
maintenance windows, new features, or community events.

### Server-Side Event Log (Audit Trail)

For admin purposes, log significant actions:

- Player bans/unbans (who, by whom, reason)
- Lobby kicks (who kicked whom)
- Rate limit enforcement events
- Admin commands (if an admin CLI/API is added)
- Configuration changes

Stored in a separate `admin_log` table, queryable via admin API.

### Planned Maintenance Mode

Server can enter maintenance mode:

```
Server -> All Connected Clients:
  MAINTENANCE_WARNING {
    message: "Server maintenance in 15 minutes. Save your game.",
    shutdown_at: "2026-04-01T03:00:00Z"
  }
```

New connections during maintenance mode get:
```
Server -> Client:
  MAINTENANCE {
    message: "Server is undergoing maintenance. Back soon.",
    estimated_return: "2026-04-01T03:30:00Z"
  }
```

### Admin API (Authenticated)

A separate set of HTTP endpoints (bearer token auth) for server
administration:

```
POST /api/v1/admin/ban         { player_id, reason, duration }
POST /api/v1/admin/unban       { player_id }
GET  /api/v1/admin/players     -- search/list players
GET  /api/v1/admin/matches     -- recent match history
POST /api/v1/admin/motd        { message, url, severity }
POST /api/v1/admin/maintenance { message, shutdown_at }
GET  /api/v1/admin/logs        -- audit trail
GET  /api/v1/admin/health      -- detailed health (more than public)
```

Auth: long-lived bearer token stored in server config. No user-facing
admin UI for v1 -- curl or a simple script is fine.

---

## Implementation Order (Revised, with New Phases)

### Phase 2.5: Post-Auth Welcome Bundle and Client Session Features -- COMPLETE

When a client authenticates, the server should send a "welcome bundle" --
a batch of messages immediately after AUTH_OK that gives the client everything
it needs to render the main multiplayer screen without additional round-trips.

#### Stable Player Identity

Currently `player_id` is `Uuid::new_v4()` on every auth -- the player gets
a new identity each session. This must be fixed before friends work properly.

**Server changes (db.rs, ws_handler.rs):**
- [x] `players` table already has a UNIQUE `gpgs_player_id` column
- [x] Add `find_or_create_player_by_gpgs(gpgs_player_id, callsign) -> Uuid`:
      looks up by `gpgs_player_id`; if not found, creates a new row with
      `Uuid::new_v4()` as the PK. Returns the stable `player_id`.
- [x] In the Authenticate handler, replace the bare `Uuid::new_v4()` with
      this lookup so the same Google account always gets the same player_id.

**Public player ID vs GPGS player ID:**
The `player_id` (server-assigned UUID) is what other clients see. The
`gpgs_player_id` is never exposed in any Server->Client message. This
gives players a stable, non-guessable identity that doesn't leak their
Google account details.

#### Welcome Bundle (sent immediately after AUTH_OK)

```
Server -> Client (in order, all sent automatically):
  1. AUTH_OK          { player_id, session_token }
  2. MOTD             { message, url, severity }               -- if configured
  3. SERVER_STATUS    { online_players, active_games, total_games_played,
                        server_ping_ms }
  4. LOBBY_LIST       { lobbies: [LobbyInfo, ...] }            -- existing message
  5. FRIEND_LIST_RESP { friends: [FriendInfo, ...] }            -- existing message
```

The client doesn't need to send LIST_LOBBIES or FRIEND_LIST to get
the initial data -- the server pushes it.

**New server message: SERVER_STATUS**

```rust
ServerMessage::ServerStatus {
    online_players: u32,          // from stats.current_online
    active_games: u32,            // lobbies with state==InGame, capped at ~20 in the list
    active_games_total: u32,      // total count
    total_games_played: u64,      // from DB
    your_ping_ms: Option<u32>,    // see ping section below
}
```

This is distinct from the HTTP `/api/v1/status` endpoint (which is public
and cacheable). The WebSocket version is targeted at this specific client
and includes their ping.

#### Active Games List

Games that are already in progress (lobby state == InGame) are useful context:
players like to see that the community is active. Currently LIST_LOBBIES
only returns `Waiting` state lobbies. Two options:

**Option A (recommended):** Add a second list to SERVER_STATUS with a
trimmed game-in-progress struct:

```rust
ActiveGameInfo {
    host_callsign: String,
    mission: String,
    mode: String,
    player_count: u8,
    duration_secs: u32,      // how long the game has been running
}
```

Cap the list at 20 entries. Include `active_games_total` for the full count.
These are display-only -- you can't join a game in progress from this list
(that requires the friend join path or Phase 5 drop-in).

**Option B:** Extend LobbyInfo with a `state` field and return all lobbies.
Simpler but conflates joinable lobbies with non-joinable games.

#### Friend List on Connect (Client-Submitted Matching)

The user's question asks: "maybe the client submits its friends list and the
server matches against it?" This is the right approach for the first connect,
but the existing FRIEND_LIST mechanism already handles this because friends
are stored server-side (in the `friends` DB table). The flow:

1. Client authenticates (stable player_id from GPGS)
2. Server calls `build_friend_list()` -- queries DB for accepted friends,
   enriches with live presence from the sessions DashMap
3. Server sends FRIEND_LIST_RESP with friend presence included

No client-side friend list submission is needed because the server is the
source of truth for friendships. The client discovers friends by searching
callsigns (FRIEND_REQUEST), then the relationship is persisted server-side.

However, there IS value in the client submitting its Google Play Games
friends list for discovery (GPGS has an API for this). This would let the
server say "hey, your Google friend PlayerTwo also plays this game":

```
Client -> Server:
  GPGS_FRIENDS { gpgs_ids: [String, ...] }

Server -> Client:
  GPGS_FRIEND_SUGGESTIONS { suggestions: [{player_id, callsign}, ...] }
```

The server matches gpgs_ids against the `players.gpgs_player_id` column.
Only returns matches for players who exist in our DB. Does NOT expose
gpgs_player_id back to the client -- only our stable player_id + callsign.

This is an optional discovery feature, not a requirement for v1.

#### Ping Measurement

The user wants two ping values visible:
1. **Client -> matchmaking server** (the client's own latency)
2. **Lobby/game host -> matchmaking server** (shown alongside each lobby)

**Client ping measurement (server-side initiated):**

After AUTH_OK, the server sends a `PING` WebSocket message (standard
WebSocket ping frame, not a custom message). The server records the
timestamp and measures the round-trip when the PONG comes back. This is
stored on the session and included in SERVER_STATUS as `your_ping_ms`.

Repeat every 30 seconds to update the value.

```rust
// In PlayerSession:
pub ping_ms: Option<u32>,  // last measured client<->server RTT
```

**Lobby host ping (shown in lobby list):**

Each LobbyInfo gets a new field:

```rust
pub host_ping_ms: Option<u32>,    // host's measured ping to the server
```

The client can display estimated total latency as:
`my_ping + host_ping` (approximate worst case for relayed connection).

For direct connections the actual latency would be lower, but this gives
a useful upper bound that helps players choose lobbies.

**Implementation in ws_handler.rs:**
- After authentication, spawn a per-connection ping task that sends a WS
  ping frame every 30 seconds and measures the pong RTT.
- Store the RTT on the PlayerSession.
- When building the lobby list, look up the host's session and include
  their ping_ms.

#### Player Messaging

Rate-limited direct messages between players:

```
Client -> Server:
  SEND_MESSAGE { target_player_id: UUID, text: String }

Server -> Target:
  MESSAGE_RECEIVED { from_player_id: UUID, from_callsign: String, text: String }

Server -> Sender:
  MESSAGE_SENT { target_player_id: UUID }
  -- or ERROR { code: "PLAYER_OFFLINE" | "RATE_LIMITED" | "BLOCKED" }
```

Rate limit: 5 messages per 60 seconds per sender (add to rate_limit.rs).
Messages are NOT persisted -- delivery is fire-and-forget. If the target
is offline, the sender gets an error.

Block list: if A has blocked B (via FRIEND_BLOCK), messages from B to A
are silently dropped (no error to B to avoid confirming the block).

Text validation: max 200 characters, printable ASCII only (no Unicode
exploits), strip leading/trailing whitespace.

#### Server-Side Implementation Checklist

Files to modify:
- [x] `protocol.rs`: Add `ServerStatus`, `ActiveGameInfo`, `SendMessage`,
  `MessageReceived`, `MessageSent` messages
- [x] `ws_handler.rs`: Send welcome bundle after AUTH_OK; add SEND_MESSAGE
  handler with rate limiting; host_ping_ms in lobby list
- [x] `db.rs`: Add `find_or_create_player_by_gpgs()` and `is_blocked()` functions
- [x] `rate_limit.rs`: Add `check_player_message()` limiter (5/60s)
- [x] `stats.rs`: No changes needed (counters already exist)
- [x] `friends.rs`: No changes needed (build_friend_list already works)
- [x] `lobby.rs`: No changes needed

New test cases (all passing, 27 total):
- [x] `test_welcome_bundle`: verify AUTH_OK is followed by SERVER_STATUS,
  LOBBY_LIST, and FRIEND_LIST_RESP without the client requesting them
- [x] `test_player_message`: send message, verify delivery and ack
- [x] `test_player_message_rate_limit`: exceed 5/60s limit, verify RATE_LIMITED
- [x] `test_player_message_blocked`: verify blocked sender gets fake ack but
  target receives nothing
- [x] `test_player_message_validation`: verify oversized text rejected
- [x] `test_stable_player_id`: authenticate twice with same gpgs token,
  verify same player_id returned
- [x] `test_lobby_list_includes_host_ping`: verify host_ping_ms field present

Note: WS ping/pong measurement task not yet implemented (ping_ms field
exists on PlayerSession but no periodic ping sender). This is a future
enhancement -- the infrastructure is in place.

---

## Implementation Order (Revised, with New Phases)

1. LAN lobby + auto-join (Phase 1) -- standalone, no server needed
2. Launcher game presets (Phase 8) -- .ngs writing, no server needed
3. File hash verification (Phase 6) -- lobby-phase feature, no server
4. **Matchmaking server skeleton (Phase 2) -- Rust, includes:**
   - **Protocol versioning (Phase 9) -- built into initial handshake**
   - **Player count API (Phase 12) -- built into initial HTTP server**
   - **Server health endpoint (Phase 14) -- built into initial HTTP server**
5. GPGS authentication (Phase 3) -- identity
6. **Friends list and presence (Phase 10) -- requires identity**
7. **Join friend's game (Phase 11) -- requires friends + lobby**
8. STUN + holepunch + relay (Phase 4) -- internet play
9. Drop-in/drop-out co-op (Phase 5) -- engine changes
10. **Game result tracking (Phase 13) -- requires games being played**
11. Voice chat (Phase 7) -- requires UDP infrastructure
12. **Admin API + operational features (Phase 15) -- ongoing**

Independent tracks (can proceed in parallel):
- Track A: Phases 1, 8, 6 (all local, no server)
- Track B: Phases 2+9+12+14, 3, 10, 11, 4 (server, sequential)
- Phase 5 can start anytime after Phase 1
- Phase 7 requires Phase 1's infrastructure at minimum
- Phase 13 can start as soon as games are being played (after Phase 4)

---

## Open Questions

- Server hosting location: US-East? EU? Need to decide based on expected
  player distribution. Can start with one and add regions later.
- GPGS consent: does the sign-in flow feel intrusive for a retro game?
  Could make it optional (allow unverified play on LAN, require for
  internet). Host controls whether unverified players can join.
- Future: full internet relay via libjuice/ICE to use third-party TURN
  servers and reduce self-hosted relay bandwidth costs.
- Future: cross-platform (desktop builds). The localhost proxy pattern
  works on any OS. Desktop would need a different identity provider
  (Steam, or fall back to keypair + proof-of-work).
- Relay rate limits: per-session 50 KB/s may need to be per-player-pair
  for multi-player games. An 8-player game with all relay would hit
  the per-session cap instantly if it's not per-pair.
- Dedicated server mode: the host-as-relay architecture means one player
  is always the critical node. A headless engine build running on a VPS
  would eliminate this. Worth investigating after the P2P architecture
  is stable. Key work: headless Linux build, command-line auto-host,
  server provisioning UI in launcher.
- Voice chat volume mixing: with 7 players talking, need per-player
  volume controls and possibly spatial audio (attenuate by in-game
  distance). Spatial audio is a nice-to-have, not MVP.
- Double-relay latency: when two non-host peers both relay through the
  server to the host, and the host relays between them, packets traverse
  the relay server twice. This could add 40-60ms. Consider having the
  relay server recognize this pattern and short-circuit (forward directly
  between the two peers if both are in the same relay session).
- Friends list storage: SQLite is fine at small scale, but if we add
  multiple server regions later, friends data needs to be global. Options:
  centralized DB (single region handles friends), replication, or an
  external service. Defer until multi-region is actually needed.
- Leaderboard reset cadence: monthly? seasonal? Seasonal (3-month) is
  more meaningful at small player counts since monthly boards would be
  sparse.
- Match result trust: host-reported results can be faked. At small scale,
  manual review of anomalies is fine. At larger scale, consider requiring
  agreement from multiple players or server-side game state validation.
- MOTD content management: for v1, a JSON config file on the server is
  fine. Later, a simple web admin panel would be better for non-technical
  community managers.
- Player data retention/GDPR: EU players will be banned from accessing the server and the server operator and code authors have zero EU nexus. no need
- Badge/shield endpoint: decide on format. shields.io-compatible JSON
  endpoint is simplest. SVG generation adds complexity for marginal benefit.
