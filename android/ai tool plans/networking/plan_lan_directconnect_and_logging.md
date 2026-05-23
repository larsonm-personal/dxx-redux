# Plan: Fix LAN Direct Connect, Missing Logs, and Overlay Auto-Show

Three bugs from LAN coop testing session.

## Bug 1: LAN direct connection fails in-game

**Root cause**: `best_candidate_addr()` in ws_handler.rs prefers srflx > observed > host.
For DirectLan, both peers share the same public IP, so srflx points at the NAT's external
address. The game client then tries to send UDP to the public NAT address, which fails
(hairpin NAT). The connectivity check proved the host candidate works, but that info is lost.

**Fix**: Add a `best_candidate_addr_for_type()` that accepts the ConnectionType:
- DirectLan -> prefer host candidates (the LAN address)
- DirectUpnp -> prefer upnp candidates
- Everything else -> existing priority (srflx > observed > host)

Update the GAME_STARTING handler to call the new function with the computed connection_type.

Add unit tests: `test_best_addr_for_lan_prefers_host`, `test_best_addr_for_upnp_prefers_upnp`.

## Bug 2: Scrolling logs missing from exportable NetLog

**Root cause**: 28 `appendLog()` calls in MatchmakingService.kt lack corresponding
`NetLog.log()` calls. The UI log (in-memory list) and file log (NetLog) are independent.

**Fix**: Add `NetLog.log(category, message)` next to every `appendLog()` that lacks one.

Categories:
- LOBBY: join, leave, start, kick
- SOCIAL: friend request, accept, remove, block, join friend game
- SERVER: MOTD, lobby list, server status, maintenance warning
- CONNECT: re-join, unknown msg type
- AUTH: PoW challenge
- ERROR: connectivity check fail, bad peer addr, internal error
- GAME: rate limited, version rejected

## Bug 3: Network overlay not auto-showing in pilot select / connecting views

**Root cause**: NetworkEventsOverlay only toggles via manual admin tray tap. The overlay
polling loop in MainActivity has zero references to netEventsOverlay.

**Fix**: In `startOverlayPolling()`, add auto-show when:
- `gameStarted` is true AND
- NOT `inGame` (we're in menus -- pilot select, connecting, etc.) AND
- MatchmakingStateHolder.state.value has an active multiplayer session
  (gameLaunchInfo != null or status is CONNECTED with a currentLobby)

Auto-hide when:
- `inGame` becomes true (entering gameplay, player has control)
- MatchmakingState goes back to disconnected/no lobby

This way, during the blank connecting screen (pilot select flow), the overlay
shows automatically, giving visibility into network connection state.

## Execution order

1. Server fix (ws_handler.rs) + tests
2. NetLog additions (MatchmakingService.kt)
3. Overlay auto-show (MainActivity.kt)
4. Build & lint both server and Android
