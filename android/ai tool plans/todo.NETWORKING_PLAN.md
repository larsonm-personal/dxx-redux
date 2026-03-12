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

## Phase 1: LAN Discovery and Lobby

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

1.5. Add JNI method nativeAutoJoin(hostIp, port, missionName, gameMode,
     difficulty) to MainActivity. C side stores params in globals, sets
     auto_join_pending flag.

1.6. Add game startup bypass in d2/main/inferno.c or menu.c: if
     auto_join_pending, skip main menu and call net_udp_direct_join().

1.7. Create net_udp_direct_join() in net_udp.c -- loads mission by name,
     sets co-op mode, constructs sockaddr from host IP/port, sends
     UPID_REQUEST directly, enters NETSTAT_WAITING. Existing
     net_udp_read_sync_packet() handles the rest.

1.8. Host auto-start path: auto_host_pending flag, engine calls
     net_udp_setup_game() + net_udp_start_game() automatically.

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

## Phase 2: Internet Matchmaking Server (Rust)

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
  ERROR           {code, message}
  RATE_LIMITED    {retry_after_ms}

---

## Phase 3: Identity and Anti-Abuse

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
2. On success, gets a server auth code (one-time token)
3. Client sends this token to our matchmaking server
4. Server exchanges token with Google's OAuth2 endpoint
   (POST https://oauth2.googleapis.com/token) to get an ID token
5. Server decodes the ID token to extract the player's stable GPGS player ID
6. This player ID is the persistent identity -- tied to a Google account

Why this works for anti-abuse:
- Google accounts are hard to mass-create (phone verification, CAPTCHA, etc.)
- Banning a GPGS player ID effectively bans the Google account from our game
- No passwords, no email collection, no PII stored on our server
- Free tier handles our scale easily
- Already on every Android device

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

### Additional Anti-Griefing

- Lobby host can kick players (by player ID, not just IP)
- Kicked players can't rejoin the same lobby
- Lobby codes for invite-only sessions (skip public listing)
- Server-side player reputation (future): track kick frequency, reports
- Packet validation: malformed messages -> immediate disconnect, no state
  allocated until after authentication

### How Minecraft Servers Handle This (Reference)

| Technique               | Minecraft                          | Our Equivalent                      |
|--------------------------|------------------------------------|-------------------------------------|
| Account verification     | Mojang session server UUID check   | GPGS token exchange                 |
| Connection throttle      | 4s cooldown per IP (vanilla)       | 3 conn/min/IP rate limit            |
| Persistent bans          | UUID ban (not IP ban)              | GPGS player ID ban                  |
| Whitelist mode            | Pre-approved UUIDs only            | "Verified only" lobby setting       |
| Proxy-level protection   | BungeeCord/Velocity rate limiting  | Server-side rate limiting           |
| Anti-bot                 | Forced movement check, CAPTCHAs   | GPGS (Google's bot prevention)      |

---

## Phase 4: NAT Traversal (STUN + Holepunch + Relay)

### The NAT Landscape for Mobile Games

| NAT Type              | Holepunch Works? | Where You See It             |
|-----------------------|------------------|------------------------------|
| Full Cone             | Always           | Rare (old routers)           |
| Address-Restricted    | Yes              | Common home WiFi             |
| Port-Restricted Cone  | Yes              | Most common home WiFi        |
| Symmetric             | Usually fails    | Mobile carriers (CGNAT),     |
|                       |                  | corporate networks           |

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

### STUN: Discovering Your Reflexive Address

STUN (Session Traversal Utilities for NAT) is a simple protocol:
1. Send 20-byte Binding Request to a STUN server
2. Server replies with your external IP:port (XOR-MAPPED-ADDRESS)
3. This tells you what address peers need to send to

Implementation in Kotlin (~100 lines):
- Build STUN request (20 bytes: type=0x0001, length=0, magic cookie, txn ID)
- Send via the lobby DatagramSocket (same socket that will be used for game)
- Parse response: find XOR-MAPPED-ADDRESS attribute, extract IP:port
- Do this twice to two different STUN servers (or same server, different ports)
  to detect symmetric NAT (reflexive port differs = symmetric)

Public STUN servers: stun.l.google.com:19302, stun.cloudflare.com:3478
(both free, reliable, globally distributed)

### Holepunch Sequence (Coordinated by Server)

```
Timeline (all in lobby, BEFORE game launch):

  Client A                  Server                     Client B
     |                        |                            |
     | STUN_RESULT(203.0.113.5:42424)                      |
     |----------------------->|                            |
     |                        |  STUN_RESULT(198.51.100.7:42424)
     |                        |<---------------------------|
     |                        |                            |
     |  PEER_STUN(198.51.100.7:42424)                      |
     |<-----------------------|                            |
     |                        | PEER_STUN(203.0.113.5:42424)
     |                        |--------------------------->|
     |                        |                            |
     | HOLEPUNCH_OK           |           HOLEPUNCH_OK     |
     |----------------------->|<---------------------------|
     |                        |                            |
     |  HOLEPUNCH_GO          |        HOLEPUNCH_GO        |
     |<-----------------------|--------------------------->|
     |                        |                            |
     | UDP ping-->            |            <--UDP ping     |
     | (to 198.51.100.7:42424)|  (to 203.0.113.5:42424)   |
     |                 simultaneous                        |
     |                 ~30 pings over 3 sec                |
     |                                                     |
     | <------ direct UDP path established ------->        |
     |                                                     |
     | Lobby shows "Connected" for this peer               |
```

Key: HOLEPUNCH_GO is sent to both peers simultaneously so they start
punching at the same time. This is critical -- if A sends before B has
sent, A's packet arrives at B's NAT before B has created a mapping,
and gets dropped. Simultaneous sends maximize success.

### Relay Fallback (Symmetric NAT)

When holepunch fails (both peers behind symmetric NAT, ~5-10% of pairs):

The matchmaking server assigns a relay session. Each peer sends game
packets to the server with a session header:

  [session_token (4 bytes) | dest_player_id (1 byte) | game_packet]

Server forwards to the other peer:

  [session_token (4 bytes) | from_player_id (1 byte) | game_packet]

5 bytes overhead. Server is stateless per-packet (just a lookup table
of session -> {player_id -> peer_addr}).

Added latency: one extra hop through the server. Typically 10-30ms
depending on server location. For a game running at 30fps (33ms per
frame), this is less than one frame of added latency.

Bandwidth: game uses ~5-15 KB/s per player. A $5 VPS can relay hundreds
of simultaneous sessions. Rate-limited per session to prevent abuse.

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

## Phase 5: Drop-In/Drop-Out Co-op

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

### Files to Modify

- d2/main/net_udp.c: save/restore state, session_token in UPID_REQUEST,
  MULTI_WALL_STATUS_BULK, MULTI_TRIGGER_STATUS
- d2/main/multi.c: MULTI_RESTORE_INVENTORY handler
- d2/main/multi.h: new message type constants
- d1/main/ equivalents

---

## Phase 6: File Hash Verification and Level Exchange

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

## Implementation Order

1. LAN lobby + auto-join (Phase 1) -- standalone, no server needed
2. Matchmaking server skeleton (Phase 2) -- Rust, WebSocket, lobby mgmt
3. GPGS authentication (Phase 3) -- identity before opening to internet
4. STUN + holepunch + relay (Phase 4) -- internet play works end-to-end
5. Drop-in/drop-out co-op (Phase 5) -- engine changes, can do in parallel
6. File verification + transfer (Phase 6) -- polish, can do in parallel

Phases 1 and 5 have no server dependency, can proceed in parallel.
Phases 2-4 are sequential (server must exist before auth, auth before
internet play).
Phase 6 can be done at any point after Phase 1.

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
