# 5 Multiplayer Bugfixes -- ALL DONE

## Bug 1: Net events overlay stuck at "Status: DISCONNECTED"
- Overlay reads MatchmakingStateHolder.state.value.status
- Status should be CONNECTED after WebSocket auth succeeds
- Likely caused by Bug #2 (activityRef not set => no game state updates => server drops lobby)
- Also could be caused by Bug #5 (wrong server URL => WS never connects)
- STATUS: FIXED by Bug #2 and Bug #5 fixes; may need further investigation if still occurs

## Bug 2: Game disappears from lobby list when non-host player leaves
- ROOT CAUSE: activityRef in MatchmakingService points to SetupActivity
- MainActivity.onCreate() never calls MatchmakingService.setActivity(this)
- pollAndSendGameState() checks `if (activity !is MainActivity) return` and silently exits
- UPDATE_GAME_STATE messages never sent to server, lobby becomes stale
- FIX: Added MatchmakingService.setActivity(this) in MainActivity.onCreate
- STATUS: DONE

## Bug 3: EPERM crash in PeerProxy.keepalive
- keepalive() catches SocketException but only breaks on "closed"
- EPERM causes SocketException with "Operation not permitted" message
- Code logged warning and retried forever instead of breaking
- FIX: Break on EPERM/permission errors in keepalive, forwardLocalToReal, forwardRealToLocal
- STATUS: DONE

## Bug 4: Zero net logs despite logging enabled
- NetLog.init(this) only called in SetupActivity.onCreate()
- When MainActivity starts, NetLog.enabled is false, writer is null
- All NetLog.log() calls silently return
- FIX: Added NetLog.init(this) in MainActivity.onCreate
- STATUS: DONE

## Bug 5: Server URL pre-populated with 10.0.2.2
- DEFAULT_SERVER_URL is "wss://10.0.2.2:9000/ws" (emulator-only)
- Text field initialized from state.serverUrl which defaults to this
- Recent URLs were saved in RecentAddressPrefs but not used as default
- FIX: Use most recent URL as pre-populated value if available (recentUrls.firstOrNull())
- STATUS: DONE
