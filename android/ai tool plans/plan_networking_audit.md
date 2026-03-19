# Networking Code Audit -- All Paths

Audit of LAN lobby, direct IP, and matchmaking server networking code paths.
Looks for: missing timeouts, threading issues, race conditions, sequencing problems,
resource leaks, security gaps, and patterns similar to bugs fixed in plan_956.

---

## A. LAN Lobby (LobbyService.kt / LobbyProtocol.kt)

### A1. [CRITICAL] [DONE] Data race on host state vars -- missing @Volatile
- **File:** LobbyService.kt ~L113-118
- `hostedLobbyId`, `hostedGame`, `hostedMission`, `hostedMode`, `hostedMaxPlayers`, `hostCallsign` are plain `var`. Written from UI thread (`hostLobby`, `stopHosting`), read from Dispatchers.IO threads (`handleJoin`, `broadcastAnnounce`).
- Without `@Volatile`, IO threads may see stale/torn values.
- `lastHostSeenMs` already correctly uses `@Volatile`.
- **Fix:** Add `@Volatile` to all six, or consolidate into a single `@Volatile` data class ref.

### A2. [CRITICAL] [DONE] handleStart() doesn't validate lobbyId
- **File:** LobbyService.kt ~L628-663
- Checks `_joinedLobby.value == null` but never validates the START packet's `lobbyId` against `_joinedLobby.value?.lobbyId`.
- Any device on the LAN can send a crafted START with attacker-controlled host address.
- **Fix:** Add `if (lobbyId != joinedInfo.lobbyId) return`.

### A3. [MEDIUM] [DONE] handlePlayerList() updates list without validating lobbyId
- **File:** LobbyService.kt ~L495-524
- `lastHostSeenMs` correctly gated by lobbyId, but `_hostedLobbyPlayers.value = players` runs unconditionally. Spoofable.
- **Fix:** Early-return if lobbyId doesn't match joined/hosted lobby.

### A4. [MEDIUM] [DONE] handleJoinAck() accepts from any sender
- **File:** LobbyService.kt ~L526-543
- No validation that we sent a JOIN for this lobbyId or that senderAddr matches the host.
- `joinRetryJob` not cancelled on ACK receipt (can send one extra JOIN).
- **Fix:** Track pending join target, validate both, cancel retry on ACK.

### A5. [MEDIUM] [DONE] Callsign comparison inconsistency
- `handleJoin` uses case-insensitive comparison; `handleLeave`/`handleReady` use case-sensitive.
- A "Player" who reconnects as "player" can't leave or toggle ready.
- **Fix:** Normalize to case-insensitive consistently.

### A6. [MEDIUM] [DONE] No host-side heartbeat for stale joiners
- When a joiner crashes, their entry persists in `_hostedLobbyPlayers` forever.
- No periodic PING from host, no lastSeen tracking per player.
- **Fix:** Track lastSeen per player, prune in existing prune loop (e.g. 10s).

### A7. [LOW] [DONE] startGame() emits launch event before START packets sent
- **File:** LobbyService.kt ~L606-623
- IO coroutine sends START asynchronously, but `_lanLaunchEvent` is set synchronously.
- If UI navigates away and calls `stopDiscovery()`, socket may close before STARTs complete.
- **Fix:** Emit launch event after sends complete, or guard socket lifetime.

### A8. [LOW] [DONE] closeSocket() cancels scope before closing socket
- `scope?.cancel()` first, then `socket?.close()`. The blocking `receive()` won't exit until socket timeout (500ms).
- **Fix:** Close socket first to unblock receive immediately.

### A9. [LOW] [DONE] leaveLanLobby() fire-and-forget single LEAVE
- UDP packet loss means host never learns player left. Combined with A6, ghost entry persists forever.
- **Fix:** Send LEAVE 2-3 times with short delays.

### A10. [LOW] [DONE] JOINs accepted after startGame()
- `_isHosting` stays true after start. Late JOINs are processed even though game is running.
- **Fix:** Set a started flag and reject JOINs after start.

### A11. [LOW] [DONE] handlePing() responds without lobby ID validation
- Any device can probe the lobby service. Minor info leak.

---

## B. Direct IP Connection (LanDiscoveryTab.kt "Join by IP")

### B1. [MEDIUM] [DONE] No IP address validation
- Any non-blank string accepted. Typos, hostnames, garbage all pass to the engine.
- **Fix:** Basic IP format validation or DNS resolution check.

### B2. [MEDIUM] [DONE] Hardcoded to D2
- `game = "d2"` always, regardless of what the host is running.
- **Fix:** Either add a game selector to the dialog, or auto-detect via a handshake.

### B3. [LOW] No version check
- Normal discovery shows version mismatch warning. Join by IP skips lobby protocol entirely.

### B4. [LOW] No error feedback path
- After `onLaunchGame`, the user is in the C engine with no kotlin-side error recovery if connection fails.

### B5. [LOW] No lobby negotiation
- Bypasses JOIN/JOIN_ACK/PLAYER_LIST flow. Mission/mode/difficulty are hardcoded defaults.

---

## C. Matchmaking Client (MatchmakingService.kt, MatchmakingState.kt)

### C1. [CRITICAL] [DONE] MatchmakingStateHolder.update() is not thread-safe
- **File:** MatchmakingState.kt ~L99
- Classic read-modify-write race on `_state.value = transform(_state.value)`.
- Multiple concurrent callers (OkHttp threads, IO coroutines) can lose updates.
- **Fix:** Use `MutableStateFlow.update {}` (built-in atomic CAS).

### C2. [CRITICAL] [DONE] Shared mutable state with no synchronization
- **File:** MatchmakingService.kt ~L36-44
- `webSocket`, `reconnectJob`, `reconnectAttempt`, `manualDisconnect`, `lastLobbyId`, `stunJob`, `stunCompleted`, `localhostProxy` -- all plain vars accessed from UI thread, OkHttp threads, and IO coroutines.
- `webSocket` TOCTOU: set to null in `onClosed` (OkHttp thread) while `send()` reads from UI thread.
- **Fix:** `@Volatile` for simple flags; `Mutex` or single-threaded dispatcher for complex operations.

### C3. [HIGH] [DONE] PeerProxy constructor binds sockets eagerly -- BindException crash
- **File:** LocalhostProxy.kt ~L104-105
- Socket bound in constructor. If previous proxy not fully shut down, `BindException` crashes the handleMessage path (OkHttp thread), killing the WebSocket.
- `localhostProxy?.shutdown()` is async -- socket close may not complete before new bind.
- **Fix:** Try/catch around addPeer, or make socket creation lazy in `run()`.

### C4. [MEDIUM] [DONE] manualDisconnect not @Volatile
- **File:** MatchmakingService.kt ~L103-108
- `disconnect()` sets `manualDisconnect = true` on UI thread; `onClosed` reads it on OkHttp thread.
- Without @Volatile, the OkHttp thread may not see the write, causing unwanted reconnection.
- **Fix:** `@Volatile`.

### C5. [MEDIUM] [DONE] No max reconnect attempts
- `scheduleReconnect` retries forever if server is unreachable. Battery drain, log spam.
- **Fix:** Cap at ~15 attempts, then set status to DISCONNECTED.

### C6. [MEDIUM] [DONE] reconnectAttempt exponential overflow
- `1L shl reconnectAttempt` when attempt >= 63 wraps to 0. Delay collapses to tight spin.
- **Fix:** `reconnectAttempt.coerceAtMost(20)`.

### C7. [MEDIUM] [DONE] handleMessage exceptions kill WebSocket
- Any uncaught exception in `handleMessage` (OkHttp callback thread) kills the connection.
- **Fix:** Wrap all of `handleMessage` in try/catch.

### C8. [MEDIUM] [DONE] No keepalive for relay connections
- **File:** LocalhostProxy.kt ~L115-118
- Keepalive only sent for direct connections. Relay connections via matchmaking server have no keepalive.
- Game idle periods can cause relay NAT or session timeout.
- **Fix:** Send relay keepalives too.

### C9. [MEDIUM] [DONE] Proxy stays alive after game ends -- no cleanup signal
- When WebSocket drops, `localhostProxy` is NOT shut down. No game-end detection to clean up.
- **Fix:** Add game-end detection (JNI signal or state polling) to call `shutdown()`.

### C10. [MEDIUM] [DONE] LocalhostProxy.shutdown() doesn't join jobs
- `job.cancel()` is non-blocking. Old coroutines may still hold socket references.
- Combined with C3, re-creating proxies races with old coroutines.
- **Fix:** `cancelAndJoin()` or close sockets first.

### C11. [MEDIUM] [DONE] Multiple CONNECTIVITY_CHECK_GO not deduplicated
- Each one launches a new connectivity check. Prior checks not cancelled.
- **Fix:** Cancel previous check job before launching new one.

### C12. [LOW] [DONE] PlayGamesAuth.getServerAuthCode has no timeout
- If Play Services hangs, coroutine blocks indefinitely. "Connecting..." forever.
- **Fix:** `withTimeoutOrNull(5000)`.

---

## D. Matchmaking/Relay Server (Rust)

### D1. [CRITICAL] [DONE] Disconnect cleanup doesn't broadcast LOBBY_UPDATE
- **File:** ws_handler.rs ~L498-507
- When non-host disconnects without LEAVE, remaining players are never notified.
- Compare with explicit LeaveLobby handler which calls `broadcast_lobby_update()`.
- **Fix:** Add broadcast_lobby_update after removing player when lobby not empty.

### D2. [CRITICAL] [DONE] Host disconnect leaves lobby orphaned
- When host disconnects, lobby persists with stale `host_player_id`. Nobody can start.
- No host migration, no dissolution, no notification.
- **Fix:** Dissolve lobby on host disconnect (notify and remove all players), or migrate host.

### D3. [CRITICAL] [DONE] No WebSocket read timeout -- idle connections persist forever
- **File:** ws_handler.rs ~L203-206
- `ws_rx.next().await` has no timeout. No ping/pong heartbeat.
- Client that completes handshake but never sends data holds resources forever.
- **Fix:** `tokio::time::timeout(Duration::from_secs(120), ws_rx.next())`.

### D4. [CRITICAL] [DONE] No timeout on WebSocket send + unbounded channel
- **File:** ws_handler.rs ~L169, ~L172-182
- `mpsc::unbounded_channel` per connection. If client stops reading, `ws_tx.send()` blocks and messages queue unboundedly.
- A malicious client can cause unbounded memory growth.
- **Fix:** Use bounded `mpsc::channel(256)`. Drop slow clients.

### D5. [MEDIUM] [DONE] Stale lobbies never reaped
- Relay sessions reaped at 2hrs, but lobbies in Starting/InGame state persist forever if host crashes without sending MatchResult.
- **Fix:** Add lobby reaper to periodic_tasks (e.g. 4hr max).

### D6. [MEDIUM] [DONE] No LobbyState::Holepunching timeout
- **File:** ws_handler.rs ~L1328
- Lobby enters Holepunching, waits for connectivity results. If a player disconnects or never responds, lobby stuck forever.
- **Fix:** Timestamp on entering Holepunching, revert to Waiting after 30s.

### D7. [MEDIUM] [DONE] No global connection cap
- Per-IP rate limit exists, but no global concurrent connection limit.
- Botnet with many IPs can exhaust resources.
- **Fix:** Global connection counter, reject at capacity.

### D8. [MEDIUM] [DONE] max_players not validated
- Client can set 0 or 255. Game/mission/mode strings unbounded length.
- **Fix:** Validate 2..=8, string length caps.

### D9. [MEDIUM] [DONE] Callsign not validated at auth
- No length/content check. Multi-MB callsign stored in session, lobby, broadcast to all.
- **Fix:** Max 20 chars, printable ASCII.

### D10. [MEDIUM] [DONE] game_started stat drifts on aborted starts
- `game_started()` called, then lobby reverts to Waiting on relay limit hit, but `game_ended()` not called.
- `current_in_game` counter drifts up.
- **Fix:** Call `game_ended()` in relay-limit rollback.

### D11. [MEDIUM] [DONE] Relay token collision risk
- `Uuid::new_v4().as_u128() as u32` -- 32-bit truncation has birthday-problem collision.
- ~65K sessions gives ~50% collision chance. Overwrites existing session.
- **Fix:** Check for existing token before insert, or use full UUID.

### D12. [MEDIUM] [DONE] Relay address learning allows session hijacking
- Any IP knowing a valid 32-bit relay token can auto-register into a free slot.
- Token brute-forceable or interceptable from unencrypted WebSocket.
- **Fix:** Pre-register expected source addresses, reject unknowns.

### D13. [MEDIUM] [DONE] Player can join lobby while already in another
- `session.lobby_id` overwritten to new lobby without removing from old lobby's player list.
- **Fix:** Implicitly leave old lobby first, or reject.

### D14. [MEDIUM] [DONE] recv_from error in relay loop exits permanently
-  **File:** relay.rs ~L63
- `?` propagates error, relay task exits and never restarts.
- Transient OS error permanently kills relay.
- **Fix:** Log and continue instead of propagating.

### D15. [LOW] [DONE] Admin token not constant-time compared
- **File:** http_api.rs ~L171
- Timing side-channel theoretically possible. Low practical risk.

### D16. [LOW] [DONE] kicked_players set grows unbounded
- No cap or cleanup. Long-lived lobbies accumulate kicked UUIDs.

### D17. [LOW] [DONE] STUN allowlist not ref-counted for shared IPs
- Multiple players behind same NAT share IP. Removing one removes allowlist for all.

### D18. [LOW] [DONE] No rate limit on LIST_LOBBIES
- Full lobby scan + serialization on every request. CPU amplification.

---

## Priority Summary

| Severity | LAN Lobby | Direct IP | Matchmaking Client | Matchmaking Server |
|----------|-----------|-----------|--------------------|--------------------|
| CRITICAL | A1, A2 | -- | C1, C2 | D1, D2, D3, D4 |
| HIGH | -- | -- | C3 | -- |
| MEDIUM | A3-A6 | B1, B2 | C4-C11 | D5-D14 |
| LOW | A7-A11 | B3-B5 | C12 | D15-D18 |

### Recommended fix order

1. **Quick wins (trivial, high impact):**
   - A1: @Volatile annotations (1 line each)
   - A2: lobbyId validation in handleStart (1 line)
   - C1: StateFlow.update{} instead of manual read-modify-write (1 line)
   - C4: @Volatile on manualDisconnect (1 word)
   - C6: coerceAtMost(20) on reconnect attempt (1 line)
   - D10: game_ended() call in rollback (1 line)

2. **Server stability (prevents stuck states and leaks):**
   - D1: broadcast on disconnect
   - D2: host disconnect handling
   - D3: WebSocket read timeout
   - D4: bounded channels
   - D5: lobby reaper
   - D6: Holepunching timeout
   - D14: relay loop error handling

3. **Client robustness (prevents crashes and races):**
   - C2: @Volatile / mutex on shared state
   - C3: BindException handling in PeerProxy
   - C7: try/catch in handleMessage
   - C9: proxy cleanup on game end
   - C10: shutdown() join jobs

4. **LAN lobby completeness:**
   - A3-A4: lobbyId validation on remaining handlers
   - A5: callsign normalization
   - A6: host-side stale joiner pruning

5. **Nice to have:**
   - B1-B2: Join by IP validation and game selector
   - D7-D9: input validation and connection caps
   - D11-D12: relay token and address hardening
