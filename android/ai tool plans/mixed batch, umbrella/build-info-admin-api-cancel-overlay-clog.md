# Plan: 6 Issues - Build Info, Admin API, Cancel, URL Rewrite, Overlay, C Logging

## Issue 1: Build info in netlog first line
Write BuildInfo fields (commit count, hash, date, time, build type) on the "Log started"
line in NetLog.openLog(). This lets us verify which build produced the log.
- Files: NetLog.kt

## Issue 2: Rust admin API on separate port
Split admin routes (/api/v1/admin/*) onto a dedicated HTTP port.
New config fields: `admin_http_listen_addr` (optional SocketAddr) and
`admin_http_enabled` (bool, default false). When enabled, admin routes are served
on the admin port only and removed from the public HTTP router. When disabled,
admin routes are not served at all (current behavior stays on the public port only
if admin_http_enabled is absent or false AND admin_http_listen_addr is absent).
Actually simpler: if `admin_http_listen_addr` is set, start an admin-only HTTP
server on that port and remove admin routes from the public router. If not set,
keep current behavior (admin routes on public port, gated by bearer token).
- Files: server/src/config.rs, server/src/http_api.rs, server/src/main.rs,
  server/server_config.json5.template

## Issue 3: Cancel connection button + friends layout
Add "Cancel" button visible during CONNECTING and RECONNECTING states in
MultiplayerScreen.kt. On press, call MatchmakingService.disconnect().
Move the friends button to a second Row when status is CONNECTED (to fix
portrait-mode crowding).
- Files: MultiplayerScreen.kt

## Issue 4: URL rewriting for bare LAN IPs
Current normalizeServerUrl() adds `wss://` to bare IPs. For private addresses,
this is wrong -- the LAN server typically has no TLS. Change to:
- If user types bare IP (no scheme), check if it's private/site-local.
  If private, default to `ws://` (not `wss://`).
  If public, default to `wss://`.
- This ensures `192.168.88.122` -> `ws://192.168.88.122:9000/ws` (no TLS, LAN)
  while `myserver.com` -> `wss://myserver.com:443/ws` (TLS, public).
- Files: MatchmakingService.kt

## Issue 5: Overlay on player-select screen (round 5)
Root cause: `mpConnecting = mpState.gameLaunchInfo != null && !inGame`.
During the "select up to 4 players" screen, `nativeIsInGame()` returns false
(Screen_mode is SCREEN_MENU, not SCREEN_GAME). So `!inGame` is true. And
`gameLaunchInfo` should be non-null (the game was launched with MP info).
Wait -- actually, when the game engine starts, does gameLaunchInfo stay set?
Let me check: gameLaunchInfo is set in MatchmakingState. It's set when the
game starts and cleared in disconnect(). So during game it should be non-null.
But is it non-null on the JOINER's side? The joiner gets gameLaunchInfo from
the StartGameNow message. Yes, it should remain.
Hypothesis: the issue is that `gameLaunchInfo` gets cleared too early, or
`inGame` returns something unexpected from the catch block.
Alternative hypothesis: the game engine launches, gameStarted=true, but nativeIsInGame()
throws because the engine isn't fully initialized yet. The catch block has:
```
if (mpState2.gameLaunchInfo != null || netEventsManualToggle) {
    netEventsOverlay?.show()
}
```
This should show the overlay. So the catch block would also show it.
BUT: the catch block sets `touchOverlay.isActive = false` and other things. The
netEventsOverlay?.show() should still work though.
Wait -- another possibility: is the `gameStarted` flag actually TRUE during the
connecting phase? Let me check when gameStarted is set.
Actually, the problem may be even simpler: `gameLaunchInfo` is on `MatchmakingState`,
but the game was started from the LAN tab (not the matchmaking lobby). The LAN
launch flow sets gameLaunchInfo differently through LobbyService. Let me check.
Looking at the log: the user connected via matchmaking, joined a lobby, and game
started with `[GAME] Starting: d2 mission=d2 slot=1 peers=1`. This sets
gameLaunchInfo. So it should be non-null.
ACTUALLY -- I need to check: does gameLaunchInfo persist AFTER the game
engine starts? In the flow: game starts -> gameLaunchInfo set -> user navigated
away from Compose UI -> C engine running -> gameStarted=true -> poller runs ->
checks mpState.gameLaunchInfo. Is MatchmakingState still holding gameLaunchInfo?
It should be, unless something cleared it.
Let me add more diagnostic: log when the overlay show/hide decisions are made
so we can trace what's happening. Actually better: the fix should be
more robust. Instead of relying solely on gameLaunchInfo, also check
`nativeIsHostSelectingPlayers()` (which we KNOW works, since startGameButton
appears). Show the overlay whenever:
- mpConnecting (gameLaunchInfo != null && !inGame), OR
- hostSelecting (nativeIsHostSelectingPlayers()), OR
- netEventsManualToggle
Also: on the JOINER side, during the "connecting to host" phase, the engine
is running but nativeIsInGame() returns false. So mpConnecting should catch it.
Let me ALSO add a dedicated native check: `nativeIsNetworkJoining()` that
returns true when the player is in the select-players/sync screen as a joiner.
Actually that's complex. Simpler approach: change the overlay logic to NOT auto-hide
during MP connections at all. Instead:
- When NOT in a multiplayer game, auto-hide is fine
- When in a multiplayer game (gameLaunchInfo != null), only hide if
  the user manually toggled off
This means: once an MP game starts, the overlay stays visible until manual toggle-off.
- Files: MainActivity.kt

## Issue 6: Zero C engine logging (round 5)
The JNI bridge chain: MPDIAG macro -> android_net_log() -> JNI -> netLogFromNative()
-> NetLog.log() + appendLog(). All pieces exist. The build compiles.
Possible failure points:
a) JNI method lookup fails silently (GetMethodID returns NULL -> mid check fails)
b) g_activity type doesn't match (shouldn't happen)
c) An exception is thrown during CallVoidMethod and not cleared
d) The function just isn't being called (code path not reached)

Best approach: add logcat debug logging to android_net_log() itself so we can
trace whether it's being called and whether the JNI call succeeds or fails.
This will definitively diagnose the issue.
- Files: android/app/src/main/cpp/shared/android_net_log.c

## Files Modified
- android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt
- server/src/config.rs
- server/src/http_api.rs
- server/src/main.rs
- server/server_config.json5.template
- android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt
- android/app/src/main/cpp/shared/android_net_log.c
