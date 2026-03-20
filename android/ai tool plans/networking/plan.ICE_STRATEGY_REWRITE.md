# ICE Strategy Rewrite Plan

## Problem Statement
LAN coop games silently fail because host candidate gathering is gated behind STUN
server availability. When the server has no `stun_public_addrs`, the client sends 0
candidates, the server classifies the connection as Unknown (not Relay), and the game
starts with empty peer addresses. The engine never connects and silently times out.

This is a broader problem than just LAN: the original networking plan
(`todo.NETWORKING_PLAN.md`) specified 5 NAT traversal strategies, but the client-side
implementation has critical gaps that prevent multiple strategies from working.

## Root Cause Chain (LAN failure)
1. LAN server AUTH_OK has no `stun_addrs`
2. `launchStunDiscovery()` checks `stunAddrs.size < 2`, skips `discover()` entirely
3. Sends `StunResult { candidates: [], natType: "unknown" }` to server
4. Server gets 0 candidates from both peers
5. `determine_connection_type()` falls through to `Unknown` (not `Relay`)
6. `best_candidate_addr([])` returns `""` (empty string)
7. `GameStarting` message embeds empty peer addr
8. Kotlin skips peers with empty/invalid addr
9. `LocalhostProxy` never set up
10. Game engine times out waiting for peer connection on 127.0.0.1

## 5-Strategy Pipeline (from original plan)
1. **Direct LAN** -- same public IP or host-only candidates on same subnet
2. **forwarded port** -- server will detect public IP forwarded ports
3. **UPnP/NAT-PMP** -- port mapped via UPnP
4. **STUN Holepunch** -- both cone NAT, standard hole-punching
5. **Predictive Port** -- symmetric NAT with predicted port candidates
6. **Server Relay** -- fallback when nothing else works

Server side: all 5 strategies are implemented.
Client side: strategies 1, 3, 4 are broken due to the candidate gathering gate.
Strategy 2 (UPnP) is not implemented. Strategy 5 (relay) fails silently when
relay_public_addr is not configured.

## Phases

### Phase 1: Fix Host Candidate Gathering (CRITICAL)
**Files:** `StunClient.kt`, `MatchmakingService.kt`

**StunClient.discover():**
- Refactor to always gather host candidates first via `getLocalIpv4Addresses()`
- Open a DatagramSocket to determine the local port even without STUN servers
- STUN queries become an optional addition when `stunAddrs.size >= 2`
- Return `natType = "no_stun"` when STUN is unavailable (not "unknown")
- Never return an empty candidate list if the device has any network interfaces

**MatchmakingService.launchStunDiscovery():**
- Remove the `stunAddrs.size < 2` early-return gate
- Always call `StunClient.discover()`, passing whatever stunAddrs are available
- The function already sends "unknown" + empty candidates on exception -- now it will
  only hit that path on actual errors, not on missing STUN servers

### Phase 2: Fix Server Edge Cases
**File:** `server/src/ws_handler.rs`

**determine_connection_type():**
- Both peers have only host candidates (no srflx): check if host IPs are on the same
  subnet (private IP ranges). If so, return `DirectLan`. This enables LAN play without
  STUN.
- Both peers have 0 candidates: return `Relay` (not `Unknown`). This ensures the relay
  fallback instead of a broken game start.

**GameStarting addr validation:**
- Before embedding a peer addr in PeerAssignment, validate it is non-empty
- If addr is empty for a direct connection type: downgrade to Relay
- If addr is empty and relay is also unconfigured: send an Error message to clients
  instead of starting a broken game

**best_candidate_addr():**
- Currently returns empty string when no candidates match -- this should still fallback
  to host candidates (it already does via the `.or_else()` chain, but we need to
  confirm this works correctly with the new host-only candidate lists)

### Phase 2.5: Port Forwarding via Server-Observed Candidates (COMPLETED)
**File:** `server/src/ws_handler.rs`

The matchmaking server knows each client's public IP from the WebSocket TCP
connection. When a player has manually forwarded their game port at their router,
the server can construct a candidate address that would be directly reachable.

**generate_observed_candidates():**
- For each player's host candidate port, construct `ws_public_ip:host_port`
- Skip loopback IPs, deduplicate against existing srflx candidates
- Inject "observed" candidates between predicted and stored candidates

**Priority and classification:**
- "observed" candidates get priority 70 (between srflx:75 and predicted:60)
- `determine_connection_type()` treats observed like srflx for LAN detection
  and holepunch classification (uses `a_public`/`b_public` combining both)
- One-sided public candidate path: if only one peer has observed/srflx, returns
  DirectHolepunch since connectivity check will verify reachability
- `ConnectivityOk` maps "observed" winning type to DirectHolepunch

**Tests:** 7 new unit tests covering observed candidate generation, deduplication,
priority, and integration with determine_connection_type.

### Phase 3: UPnP/NAT-PMP Client (COMPLETED)
**Files:** `UpnpClient.kt`, `StunClient.kt`, `ConnectivityChecker.kt`,
`MatchmakingService.kt`, `LocalhostProxy.kt`

Lightweight UPnP IGD port mapping client. Discovers the local router's UPnP
service via SSDP M-SEARCH, parses the device description XML, and uses SOAP
to map a UDP port and query the external IP.

**Shared candidate socket:**
A single DatagramSocket is created at the start of candidate discovery and
persists through the entire matchmaking lifecycle:
1. Created in `launchStunDiscovery()` before STUN/UPnP run in parallel
2. Passed to `StunClient.discover(stunAddrs, socket)` -- STUN queries use it,
   host candidates reflect its port
3. Passed to `ConnectivityChecker.probe(pairs, socket)` -- probes send/receive
   on the same port, maintaining NAT mappings
4. Handed to the first non-relay PeerProxy in `LocalhostProxy` when GAME_STARTING
   arrives -- the UPnP mapping stays valid on the same bound port
5. Closed only on disconnect/leave-lobby if not consumed by a PeerProxy

**UPnP flow:**
- `UpnpClient.tryMap(candidatePort, localIp)` runs in parallel with STUN
- Maps `candidatePort` (UDP) via SSDP discovery + SOAP AddPortMapping
- Returns `UpnpMapping` with external IP and port
- "upnp" candidate added to candidate list: `externalIp:externalPort`
- Server uses "upnp" candidates with priority 90 (above srflx:80)
- Cleanup: `removeMapping()` called via SOAP DeletePortMapping on disconnect

### Phase 4: Multi-Peer Shared Socket (COMPLETED)
**Files:** `LocalhostProxy.kt`, `MatchmakingService.kt`

All PeerProxies now share a single UDP socket when one is available (from STUN/UPnP
discovery). This ensures UPnP port mappings and NAT pinholes benefit all peers in
3+ player games, not just the first.

**LocalhostProxy changes:**
- Accepts optional `sharedRealSocket: DatagramSocket` in constructor
- When shared socket is set, runs a single `sharedReceiveLoop()` coroutine that
  demuxes incoming packets:
  - Direct peers: matched by source IP:port against registered `realAddr`
  - Relay peers: matched by `from_slot` byte in the 5-byte relay header
- Uses `ConcurrentHashMap` for thread-safe demux map access between `addPeer`
  (called from WebSocket thread) and the shared receive loop (Dispatchers.IO)
- `PeerProxy` no longer runs its own `forwardRealToLocal` when not owning socket;
  receives come via `deliverIncoming()` called by the shared loop
- `shutdown()` closes the shared socket and clears all demux maps

**PeerProxy changes:**
- Constructor takes explicit `realSocket` + `ownsRealSocket` instead of optional
- New `deliverIncoming(data, length)` method for shared-mode packet delivery
- `run()` conditionally launches `forwardRealToLocal` only when `ownsRealSocket`
- `close()` only closes `realSocket` when `ownsRealSocket`

**MatchmakingService changes:**
- GAME_STARTING handler passes shared socket to `LocalhostProxy` constructor
  instead of first PeerProxy -- simpler, no more `sharedSocketUsed` tracking
- `addPeer` no longer needs `existingRealSocket` parameter

### Phase 5: Network Events Overlay (COMPLETED)
Kotlin-side overlay showing connection status, NAT type, relay/direct indicators,
latency. Uses existing NetLog infrastructure.

### Phase 6: Deploy Script Public Address Setup (COMPLETED)
Added automatic public IP detection and config patching to `deploy_build.sh`.

**deploy_build.sh changes (new step 6, web mode only):**
- Detects public IP via api.ipify.org / ifconfig.me / icanhazip.com (with fallback)
- Reads current `relay_public_addr` and `stun_public_addrs` from config
- Suggests the nginx domain (if available), then existing config, then detected IP
- Asks user to confirm or enter a hostname/IP
- Patches config with sed, handling both commented-out and active lines
- If field is missing entirely, inserts before closing brace
- Skips step if values already match

**Config template updates:**
- `config.json5.default`: changed `YOUR_PUBLIC_IP` to `YOUR_HOST` in placeholders
- `server_config.json5.template`: updated examples to use domain name format
- Rust doc comments on `relay_public_addr` and `stun_public_addrs` now note
  that domain names are supported (server passes these as opaque strings to clients)

**Domain name support:**
Both `relay_public_addr` and `stun_public_addrs` are `String` fields resolved
via `resolve()` (not `resolve_addr()`). The server never parses them as
`SocketAddr` -- they are sent as-is to clients in AUTH_OK and RELAY_ASSIGNED.
Clients resolve hostnames via standard Android socket APIs.

### Phase 7: Verification (future)
End-to-end LAN coop test with two emulators, validating the full connection flow.

## Current Implementation Status
- Phase 1: COMPLETED -- StunClient always gathers host candidates; STUN is optional
- Phase 2: COMPLETED -- server handles host-only candidates (shared subnet -> DirectLan),
  empty candidates -> Relay, addr validation + relay downgrade in GameStarting
- Phase 2.5 (new): COMPLETED -- port forwarding detection via server-observed candidates.
  Server constructs "observed" candidates from WS public IP + host candidate ports.
  ConnectivityChecker probes them naturally. 7 new unit tests.
- Phase 3: COMPLETED -- UPnP/NAT-PMP client (UpnpClient.kt). Shared candidate socket
  persists from STUN through connectivity check and is handed to LocalhostProxy PeerProxy.
  UPnP mapping stays valid on the same socket through the full session lifecycle.
- Phase 4: COMPLETED -- Multi-peer shared socket demux in LocalhostProxy.
  All peers share one UDP socket; incoming demuxed by source addr or relay from_slot.
- Phase 5: COMPLETED -- Network events overlay for SetupActivity + in-game admin tray
- Phase 6: COMPLETED -- deploy_build.sh auto-detects public IP, patches relay/STUN config
- Phase 7: NOT STARTED
