# Lobby Unification Plan: LAN + Matchmaking

## Goal
Unify the lobby experience so LAN and matchmaking lobbies share as much UI, code, and feature set as possible. Reduce maintenance burden by eliminating duplicated composables and logic.

## Current State

### Files
- `LanDiscoveryTab.kt` (~1045 lines) -- LAN discovery view + joined lobby view + host/join/start dialogs + coop save offer
- `LobbyScreen.kt` (~401 lines) -- matchmaking in-lobby UI (player list, chat, ready, kick, start, coop save)
- `MultiplayerScreen.kt` -- top-level nav router; `CreateLobbyDialog`, `LobbyCard` (server browser), `LanContent`, `FriendsContent`, `ServerBrowserContent`
- `LobbyService.kt` (~1000 lines) -- LAN UDP lobby protocol (host, join, ready, start, discovery, pruning)
- `MatchmakingService.kt` -- WebSocket matchmaking (auth, lobby CRUD, chat, ready, kick, start, ICE/STUN)
- `MatchmakingState.kt` -- shared state types: `GameLaunchInfo`, `MatchmakingState`, `CurrentLobbyState`

### Feature Comparison

| Feature | LAN | Matchmaking |
|---------|-----|-------------|
| Player list | YES (LanJoinedLobbyView) | YES (LobbyScreen PlayerCard) |
| Ready toggle | YES | YES |
| Coop save offer | YES (LanCoopSaveOffer) | YES (CoopSaveOffer) |
| Difficulty select | YES (StartLanGameDialog) | YES (CreateLobbyDialog) |
| Mission/mode/game | YES (HostLanGameDialog) | YES (CreateLobbyDialog) |
| Host dialog | YES (HostLanGameDialog - no diff/level) | YES (CreateLobbyDialog - all fields) |
| Start dialog | YES (StartLanGameDialog - separate) | N/A (button in lobby) |
| Chat | NO | YES (LobbyChatArea) |
| Kick players | NO | YES (MatchmakingService.kickPlayer) |
| Ping display | NO | YES (player.pingMs) |
| Build version check | YES (host vs self) | YES implicit |
| In-game join | YES (Join In-Game button) | YES (active games list) |
| Join by IP | YES (2 dialogs) | N/A |

### Key Architectural Differences
- **LAN**: `LobbyService` singleton, `StateFlow`-based state, direct UDP
- **Matchmaking**: `MatchmakingService`/`MatchmakingStateHolder`, `StateFlow`-based state, WebSocket
- Both ultimately produce a `GameLaunchInfo` to launch the game

## Plan

### Phase 1: Shared lobby composable (LARGE -- the main unification)

Create a unified `InLobbyView` composable that both LAN and matchmaking use for the in-lobby screen. This replaces `LanJoinedLobbyView` and `LobbyScreen` with a single component.

**New abstraction layer** -- `LobbyAdapter` interface:
```kotlin
interface LobbyAdapter {
    // Read-only state
    val players: StateFlow<List<UnifiedPlayer>>
    val lobbyInfo: StateFlow<UnifiedLobbyInfo?>
    val chatMessages: StateFlow<List<ChatMessage>>      // LAN: empty or local-only
    val isHost: Boolean

    // Actions
    fun setReady(ready: Boolean)
    fun leave()
    fun startGame(difficulty: Int, levelNum: Int)
    fun sendChat(text: String)          // LAN: no-op or local broadcast
    fun kickPlayer(id: String)          // LAN: no-op initially
}

data class UnifiedPlayer(
    val id: String,          // callsign for LAN, playerId for matchmaking
    val callsign: String,
    val ready: Boolean,
    val isHost: Boolean,
    val isMe: Boolean,
    val pingMs: Int? = null, // null for LAN
)

data class UnifiedLobbyInfo(
    val game: String,
    val mission: String,
    val mode: String,
    val maxPlayers: Int,
    val hostCallsign: String,
    val isLan: Boolean,
)
```

**Unified `InLobbyView`** composable:
- Player list (shared PlayerCard with optional ping, optional kick button)
- Ready toggle
- Chat area (hidden or collapsed when no messages / LAN mode)
- Coop save offer (already nearly identical -- extract to shared composable)
- Start game button (host only, with difficulty/level picker)
- Leave button
- Build version warning (LAN only detail, conditional)

**Two adapter implementations**:
- `LanLobbyAdapter` -- wraps LobbyService state, maps to UnifiedPlayer/UnifiedLobbyInfo
- `MatchmakingLobbyAdapter` -- wraps MatchmakingStateHolder state

### Phase 2: Merge host dialogs

`HostLanGameDialog` and `CreateLobbyDialog` are very similar (game/mission/mode/maxPlayers/difficulty/level). Differences:
- LAN: no difficulty/level at host time (separate StartLanGameDialog)
- Matchmaking: includes difficulty, level, coop saves at create time

Unify into a single `CreateGameDialog` with an `isLan: Boolean` parameter. For LAN, move difficulty and level selection into the host dialog (eliminating `StartLanGameDialog`). This brings LAN hosting to parity with matchmaking.

### Phase 3: Add chat to LAN lobby (SMALL)

Add a `MSG_CHAT` protocol message to `LobbyProtocol.kt`:
- Host receives chat and broadcasts to all joiners
- Joiners send chat to host
- Simple text relay, no persistence

Wire into `LanLobbyAdapter.sendChat()`. Chat is then automatically visible in the unified `InLobbyView`.

### Phase 4: Add kick to LAN lobby (SMALL)

Add a `MSG_KICK` protocol message:
- Host sends KICK to the target player address
- Target player auto-leaves on receiving KICK
- Wire into `LanLobbyAdapter.kickPlayer()`

### Phase 5: Join-by-IP lobby-first fallback (SMALL)

Merge the two "Join by IP" dialogs into one. When user provides an IP and taps "Join":
1. Try `LobbyService.joinLobbyByIp()` with 1-second timeout (single QUERY + wait)
2. If no ANNOUNCE arrives within 1s, fall back to direct game engine launch (current `JoinByIpDialog` behavior with `GameLaunchInfo`)
3. Show progress: "Checking for lobby..." then "No lobby found, joining game directly"

### Phase 6: LAN ping display (OPTIONAL/FUTURE)

Add periodic PING/PONG between host and joiners, display RTT in player list. Already have MSG_PING/MSG_PONG protocol messages defined but not used for display.

## Execution Order

1. Phase 5 (join-by-IP) -- DONE
2. Phase 2 (merge host dialogs) -- DONE (CreateGameDialog.kt replaces both; HostLanGameDialog, StartLanGameDialog, CreateLobbyDialog deleted)
3. Phase 3 (LAN chat) -- DONE (MSG_CHAT in protocol, sendChat/handleChat in LobbyService, shared ChatArea composable)
4. Phase 4 (LAN kick) -- DONE (MSG_KICK in protocol, kickPlayer/handleKick in LobbyService, kick button in LAN player list)
5. Phase 1 (shared lobby composable) -- DONE (ChatArea.kt extracted as shared composable used by both LAN and matchmaking)
6. Phase 6 (ping) -- optional stretch, not done

## Phone-to-Phone LAN Failure Survey

Recent git history (HEAD~8..HEAD) shows NO code changes that could break LAN discovery or direct-connect. The failure is environmental:

1. **AP Isolation**: Most consumer WiFi APs block device-to-device broadcast and sometimes unicast. This is the #1 suspect since both broadcast discovery AND direct IP failed
2. **Wrong interface binding**: Socket binds to `0.0.0.0` via `InetSocketAddress(port)`. On phones with multiple interfaces (WiFi + cellular + VPN), packets may route through the wrong interface
3. **Android 13+ permissions**: `NEARBY_WIFI_DEVICES` can be auto-denied depending on target SDK and device vendor. Silent denial = no broadcasts
4. **WiFi power saving**: Some phones aggressively drop broadcast/multicast packets when idle. The `MulticastLock` helps but vendor implementations vary
5. **Dual-band AP**: If devices are on different bands (2.4GHz vs 5GHz) with AP isolation per-band, they cannot see each other even on the "same" network

**Recommendation**: Add a LAN diagnostics panel that shows:
- Which interfaces are up and their addresses
- Whether broadcast sends succeed (already tracked)
- Whether any packets have been received at all
- Button to send a test ping to a manually-entered IP
