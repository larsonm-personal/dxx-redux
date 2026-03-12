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
- wrap this with integration testing and checksumming because it's a difficult problem that will likely have bugs

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

## Phase 7: Voice Chat

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

## Phase 8: Launcher Game Presets (Full Server Setup from Launcher)

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
- Send to stun.l.google.com:19302 via the lobby DatagramSocket
- Parse response: find XOR-MAPPED-ADDRESS (attr type 0x0020), XOR with
  magic cookie to get reflexive IP:port
- Two queries to different servers detects symmetric NAT (ports differ)

Pros: zero dependencies, tiny code, full control over socket reuse.
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
