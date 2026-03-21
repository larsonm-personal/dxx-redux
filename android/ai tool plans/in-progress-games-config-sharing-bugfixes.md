# Plan: In-Progress Game Tracking, Game Config Sharing, Bug Fixes

Six items in three phases:
- (A) Two bug fixes -- net stats "disconnected" and WSS cleartext regression
- (B) Game config sharing -- difficulty/level in lobby, propagated as extensible JSON
- (C) In-progress game tracking + mid-game join with host-side ICE coroutine

---

## Phase A: Bug Fixes -- DONE

### A1. Net stats log shows "disconnected" instead of relay/direct/stun -- DONE

**Root cause**: MatchmakingService.kt ~line 998 handles `ConnectionInfoReceived`
by updating state, but has **zero NetLog.log() call**. Every other message type
gets logged. Without log output, the user sees nothing in the net log -- it looks
like "disconnected".

**Fix**: Added NetLog.log("CONNECTION", ...) and state.appendLog() for each peer
in the ConnectionInfoReceived handler. Logs callsign, method, relay/direct,
detail, and latency.

**Files**: MatchmakingService.kt

### A2. WSS "cleartext not permitted" for LAN connections -- DONE

**Root cause**: `normalizeServerUrl()` detects private RFC1918 IPs and downgrades
wss:// to ws://. Android's network_security_config.xml only permits cleartext for
10.0.2.2/localhost/127.0.0.1, so 192.168.x.x is blocked. This is a regression
from URL rewriting changes.

**Fix**: Removed wss->ws downgrade entirely. Always wss://. lanClient handles
self-signed certs for private IPs. Updated DEFAULT_SERVER_URL to wss://.
Default port selection uses isPrivateAddress() (9000 for LAN, 443 for public).

**Files**: MatchmakingService.kt, NetworkConstants.kt

---

## Phase B: Game Config Sharing -- DONE

### Design: Extensible game_info JSON object

Rather than adding individual fields (difficulty, level_num) to CREATE_LOBBY,
LOBBY_LIST, etc., introduced a `game_info` JSON object that travels with the
lobby. This makes future extensions (mods, asset hashes, game flags, etc.)
trivial -- clients and server preserve unknown fields.

- `game_info`: arbitrary JSON object, max 5KB serialized
- Fundamental fields defined with typed accessors: `difficulty` (u8 0-4),
  `level_num` (i32), `mission` (string), `mode` (string)
- Server stores lobby.game_info as a serde_json::Value
- Server validates max size (5KB) on receipt, rejects larger payloads
- LOBBY_LIST includes game_info for each lobby
- GAME_STARTING includes game_info from the lobby
- Clients deserialize known fields, pass unknown fields through unchanged
- Future: mods[], asset_hashes{}, game_flags, kill_goal, time_limit, etc.

### B1. Add level/difficulty to CreateLobbyDialog (Kotlin UI) -- DONE

Added difficulty selector (Trainee/Rookie/Hotshot/Ace/Insane toggle buttons)
and level number input to CreateLobbyDialog. Builds game_info JsonObject with
mission, mode, difficulty, level_num. Updated CreateLobbyMsg to accept
gameInfo: JsonObject. Updated MatchmakingService.createLobby() to new signature.

**Files**: MultiplayerScreen.kt, NetworkProtocol.kt, MatchmakingService.kt

### B2. Server: Store and propagate game_info -- DONE

Replaced separate `mission`, `mode` fields with `game_info: serde_json::Value`
in Lobby struct, LobbyInfo, GameStarting, CreateLobby. Added helper methods
mission() and mode() on Lobby for backwards-compat access. Added
UPDATE_GAME_INFO handler (host-only, validates 5KB limit, broadcasts update).
Updated all usages in ws_handler.rs, http_api.rs. All 59 integration tests pass.

**Files**: server/src/lobby.rs, server/src/protocol.rs, server/src/ws_handler.rs,
  server/src/http_api.rs, server/tests/integration.rs

### B3. Client: Display game config in lobby list and lobby screen -- DONE

LobbyInfo and GameStartingMsg now use gameInfo: JsonObject with derived
properties (mission, mode, difficulty, levelNum) for backwards-compat access.
LobbyCard shows config line: "mission -- mode / difficulty / Lv N".
LobbyScreen shows full config summary when inside a lobby.
CurrentLobbyState carries gameInfo, populated from lobby list or
pendingGameInfo (for host-created lobbies).

**Files**: NetworkProtocol.kt, MultiplayerScreen.kt, LobbyScreen.kt,
  MatchmakingState.kt, MatchmakingService.kt, SetupActivity.kt

### B2 cleanup: Removed backwards-compat wrappers -- DONE

Removed `Lobby::mission()` and `Lobby::mode()` helper methods from lobby.rs.
Replaced all callers in ws_handler.rs with `game_info_str(&l.game_info, "mission")`.
Removed derived properties (mission, mode, difficulty, levelNum) from Kotlin
LobbyInfo and GameStartingMsg. Updated all callers (MatchmakingService.kt,
MultiplayerScreen.kt, SetupActivity.kt) to extract from gameInfo directly.
Added `game_info_str()` free function in lobby.rs and exported it.

---

## Phase C: In-Progress Game Tracking + Mid-Game Join

### C1. Host sends periodic game state updates -- DONE

After GAME_STARTING, the host's Kotlin layer starts a periodic update coroutine
(~10s interval) that polls C game state via JNI.

1. New JNI function `nativeGetNetgameState()` in jni_main.c: returns int[] with
   [game_status, numconnected, max_numplayers, levelnum, Game_mode]
2. New client message: UPDATE_GAME_STATE { player_count, max_players,
   current_level, game_status }
3. Coroutine in MatchmakingService: starts on GAME_STARTING (host only),
   sends updates every 10s, stops on disconnect()

**Files**: jni_main.c, MainActivity.kt, MatchmakingService.kt, NetworkProtocol.kt

### C2. Server tracks in-progress games -- DONE

1. Handle UPDATE_GAME_STATE: update lobby's runtime fields, transition
   Starting -> InGame on first update
2. Added runtime fields to Lobby: runtime_player_count, runtime_level,
   runtime_game_status, last_state_update
3. Updated is_joinable() to return true for InGame when not full and not stale
4. Include in-progress games in LOBBY_LIST with lobby_state field
   ("waiting" / "in_progress") and current_level
5. Stale detection: no update in >60s -> unjoinable

**Files**: server/src/ws_handler.rs, server/src/lobby.rs, server/src/protocol.rs,
  server/src/http_api.rs

### C3. Client can browse and join in-progress games -- DONE

1. LobbyCard: "IN GAME" badge for in_progress lobbies
2. Shows current level for in-progress games
3. "Join" button available for joinable in-progress games

**Files**: MultiplayerScreen.kt, NetworkProtocol.kt

### C4. Server + host run ICE for late joiner -- DONE

**Revised approach**: Instead of a temporary DatagramSocket (which opens pinholes
on the wrong port), the host uses its existing shared socket for late-join ICE.

**Key insight**: A temp socket's NAT pinholes don't help the shared socket that
carries actual game traffic. Instead:
1. Host sends blind probes from the shared socket (opens NAT pinholes)
2. LocalhostProxy echoes probe REQUESTs from unknown senders (for the joiner's
   ConnectivityChecker to complete successfully)
3. No temp socket needed -- all pinholes are on the right socket

**Protocol additions**:
- `LATE_JOIN_PROBE` (server->host): joiner_id, joiner_callsign, probe_addrs[]
- `LATE_JOIN_APPROVED` (server->host): peer PeerAssignment for addPeer()
- Joiner receives standard CONNECTIVITY_CHECK_GO and GAME_STARTING

**Server-side flow**:
1. Joiner sends JOIN_LOBBY for InGame lobby -> add_player() succeeds
2. LOBBY_UPDATE triggers joiner's STUN (2+ players)
3. Joiner sends STUN_RESULT -> server detects InGame + both have candidates:
   a. Builds candidate pairs for joiner->host
   b. Sends CONNECTIVITY_CHECK_GO to joiner (normal pairs)
   c. Sends LATE_JOIN_PROBE to host (joiner's addresses)
   d. Adds joiner to lobby.pending_late_joiners
4. Joiner runs ConnectivityChecker.probe() (normal flow)
5. Host's proxy echoes joiner's probes back via DXPC magic detection
6. Joiner reports CONNECTIVITY_OK -> server detects pending late-joiner:
   a. determine_connection_type() on host+joiner candidates
   b. Build PeerAssignment with relay if needed
   c. Send GAME_STARTING to joiner (host as only peer)
   d. Send LATE_JOIN_APPROVED to host (joiner's peer assignment)
   e. Host calls localhostProxy.addPeer() for new joiner

**LocalhostProxy changes**: Added lateJoinProbeActive flag and probe echo in
the shared receive loop (detects DXPC magic, echoes REQUEST as RESPONSE).
Added sendLateJoinProbes() to send blind probes from shared socket.

**Edge cases handled**:
- Cleanup: pending_late_joiners cleared in remove_player() on disconnect
- Multiple simultaneous late joiners: each gets own pending entry + ICE flow
- Host leaves during ICE: lobby dissolved, joiner notified

**Files**: server/src/protocol.rs, server/src/lobby.rs, server/src/ws_handler.rs,
  NetworkProtocol.kt, MatchmakingService.kt, LocalhostProxy.kt

### C5. Host accepts late joiner via existing D2 mechanism -- DONE

**Design**: When the host uses auto_host (matchmaking), RefusePlayers=1 is set
so the host gets an explicit prompt for mid-game joins. The Kotlin overlay polls
the engine's WaitForRefuseAnswer state via JNI and shows an "ACCEPT: callsign"
button. Tapping it sets RefuseThisPlayer=1 (equivalent of F6).

**End-to-end flow**:
1. After LATE_JOIN_APPROVED, host Kotlin sets up proxy for new peer [C4]
2. Joiner gets GAME_STARTING, launches game with auto_join via proxy
3. Joiner engine sends UPID_REQUEST to host via proxy
4. Host engine receives UPID_REQUEST, calls net_udp_do_refuse_stuff
5. Engine shows "X wants to join (accept: F6)" HUD and sets WaitForRefuseAnswer=1
6. AcceptJoinButtonView appears (polled every 100ms by overlayPoller)
7. Host taps "ACCEPT: callsign" button -> nativeAcceptJoinRequest() sets RefuseThisPlayer=1
8. Next UPID_REQUEST from same player triggers net_udp_welcome_player
9. Joiner receives sync, enters game

**Changes**:
- D1+D2 net_udp.c: Set Netgame.RefusePlayers=1 in net_udp_auto_host
- android_input.c: nativeGetJoinRequest() (returns callsign or ""),
  nativeAcceptJoinRequest() (sets RefuseThisPlayer=1)
- AcceptJoinButtonView.kt: new pill button view, shows "ACCEPT: callsign"
- MainActivity.kt: declare JNI externs, create view, add to frame, poll in
  overlayPoller, clean up in error/non-game paths

**Files**: d1/main/net_udp.c, d2/main/net_udp.c, android_input.c,
  AcceptJoinButtonView.kt, MainActivity.kt

---

## Decisions

- WSS everywhere: no cleartext. lanClient handles self-signed for private IPs
- game_info: extensible JSON object (max 5KB). Known fields have typed accessors,
  unknown fields preserved on passthrough
- Mid-game join: existing D2 host-approval (F6 / new touch button)
- Level/difficulty UI: Kotlin CreateLobbyDialog, server-propagated via game_info
- Host background WS: Kotlin maintains WS, polls C state via JNI
- Mid-game ICE: blind probes + echo from shared socket (not temp socket -- temp
  socket opens pinholes on wrong port). Proxy detects DXPC magic and echoes.
- Candidate reuse: host's original STUN candidates used for late-join ICE (v1)
- No backwards compat: pre-release, protocol changes are fine
