# Plan: Address Caching + Overlay Toggle Fixes

## Issues
1. Matchmaking server connect screen needs address caching (last 5, most recent, deduplicated)
2. Net events overlay briefly shows then disappears on toggle (should be persistent on/off)
3. Net events overlay doesn't show during connecting/player select screens
4. net_log_comment "always log" -- unwind redundancy, keep overlay bridge
5. No C-side log entries appearing in NetLog files (explanation: build not yet deployed)

## Changes

### 1. Generic RecentAddressPrefs + Matchmaking URL Caching
- Refactor `RecentIpsPrefs` (LanDiscoveryTab.kt) into a generic `RecentAddressPrefs` class
  - Parameterized by SharedPreferences key
  - Same logic: last 5, newest first, deduplicated
- Create instances:
  - `RecentAddressPrefs("recent_lan_ips")` -- for LAN Join-by-IP dialogs (existing behavior)
  - `RecentAddressPrefs("recent_server_urls")` -- for matchmaking server URL
  - Third key can be added later for a Direct Internet IP mode if needed
- Place the generic class in a shared location (e.g. RecentAddressPrefs.kt in multiplayer/)
- Update LanDiscoveryTab.kt: replace `RecentIpsPrefs` with generic instance
- Update MultiplayerScreen.kt:
  - Load recent URLs on composable init
  - Save URL on successful connect
  - Show clickable suggestions below the URL field (reusable `RecentSuggestions` composable)
- Move `RecentIpSuggestions` composable to the shared file, generalized as `RecentSuggestions`

### 2. Fix Overlay Toggle Behavior (Issues 2 + 3)
Root cause: polling loop (100ms) has `else if (inGame) { netEventsOverlay?.hide() }` which
immediately overrides a manual toggle-on.

Fix in MainActivity.kt:
- Add field `private var netEventsManualToggle = false`
- Toggle handler: flip flag, call show/hide accordingly
- Polling loop: `if (mpConnecting || netEventsManualToggle) show() else hide()`
  - During connecting: always shown (auto-show) via mpConnecting
  - During in-game: shown only if user toggled on
- Reset `netEventsManualToggle = false` when `!gameStarted` (game exited entirely)

### 3. Clean Up net_log_comment Android Bridge (Issue 4)
Background: `GameArg.LogNetTraffic` is ALWAYS 1 on Android (hardcoded in d1/d2 args.c).
So the unconditional `android_net_log` call BEFORE the check is functionally redundant --
the code never returns early on Android. But the bridge IS needed because PHYSFS file writes
don't reach Kotlin.

Fix: Move `android_net_log` call AFTER the LogNetTraffic check with a clear comment.
Functionally identical on Android (LogNetTraffic is always 1), but cleaner structure.
Apply to both d1/main/net_udp.c and d2/main/net_udp.c.

### 4. Issue 5 Explanation
C-side log entries (MPDIAG, NETLOG) not appearing in the NetLog file is most likely because
the test was run with an APK built BEFORE the C-side bridge changes. Once the latest build
is deployed to the phones, entries should appear.

## Files Modified
- android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt (NEW)
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt
- d1/main/net_udp.c
- d2/main/net_udp.c
