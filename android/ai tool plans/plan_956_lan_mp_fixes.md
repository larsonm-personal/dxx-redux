# Plan 956: LAN Multiplayer Fixes

Four interrelated issues with LAN multiplayer between two Android phones.

## Issue Summary

1. Two players with the same name: joining silently fails (no error)
1a. Default callsign is always "Player" with no randomization or persistence
2. Ready button has no effect; host can't start game; client never launches
3. Host gets stuck on "select up to 4 players" screen with no way out
4. No LAN-related logging appears in the downloadable netlog

## Root Cause Analysis

### Issue 1 & 2: The Core Logic Bug

The Kotlin LAN lobby (`LobbyService.kt`) manages a player list. In `hostLobby()` (line ~175),
the host adds itself to `_hostedLobbyPlayers` with its own callsign:

```kotlin
_hostedLobbyPlayers.value = listOf(LanPlayer(callsign = callsign, address = "127.0.0.1", ready = false))
```

When a joiner sends a JOIN packet, `handleJoin()` checks for duplicates by callsign:

```kotlin
if (current.any { it.callsign == callsign }) {
    // re-sends ACK without adding the player
}
```

If both players have the same default callsign "Player", the joiner's JOIN matches the host's
own entry. The joiner receives a JOIN_ACK (so the UI transitions to the lobby view), but they
are never actually added to the player list. The PLAYER_LIST broadcast still has only 1 entry
(the host). Consequence: Ready has no effect (the joiner's callsign isn't in the host's list
to update), and START is never sent to the joiner (they aren't in the player list).

Even with different names, the host marks itself `ready = false` but has no Ready button
in the hosting view. The host IS always shown as "Not Ready" in the player list.

### Issue 3: Stuck on Select Players

The host's game engine enters `net_udp_select_players()` which is a modal `newmenu_do1()`.
On Android, ESC (Back button) should close it, but there may be event consumption issues.
The workaround is a dedicated "Return to Launcher" overlay button.

### Issue 4: No LAN Logging

The Kotlin LAN lobby uses `Log.d/i/w` (logcat only), not `NetLog.log()` (written to
downloadable files). The C engine netlog requires `-netlog` arg which isn't enabled by default.

---

## Phase A: Duplicate Name Handling + Random Callsign

### A1. Fix handleJoin() duplicate-name logic (LobbyService.kt)

The duplicate check should compare addresses, not just callsigns:
- If a player with the same callsign AND same address exists: re-send ACK (idempotent retry)
- If a player with the same callsign but DIFFERENT address exists: send JOIN_REJECT with
  reason "duplicate callsign"
- Use case-insensitive comparison (matches C engine's `d_stricmp`)

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt -- handleJoin()

### A2. Surface JOIN_REJECT reason to user

In handleJoinReject(), update diagnostics StateFlow so the user sees the reason.

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt -- handleJoinReject()

### A3. Add DUMP_DUPNAME in C engine (defense-in-depth)

Add duplicate-callsign check in net_udp_add_player() for when different IPs send the same name.
Provides an error message at the engine level if the Kotlin lobby check is somehow bypassed.

Files:
- d2/main/multi.h, d1/main/multi.h -- DUMP_DUPNAME constant
- d2/main/net_udp.c, d1/main/net_udp.c -- net_udp_add_player()
- d2/main/text.h, d1/main/text.h -- NET_DUMP_STRINGS macro
- d2/main/net_udp.c, d1/main/net_udp.c -- net_udp_process_dump()

### A4. Random callsign on first launch + persistence

Save/load callsign from SharedPreferences("dxx_prefs", "mp_callsign"). On first launch
(no saved value), generate "Player" + "%02d" random 00-99 (8 chars = CALLSIGN_LEN).
When user edits in the Multiplayer screen, persist the new value.

Files:
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt -- mpCallsign init
- android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt -- default callsign
- android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt -- callsign TextField persistence

## Phase B: Fix Ready/Start Game Flow

### B1. Host auto-marks as ready

In hostLobby(), set the host player's `ready = true`. The host doesn't need a Ready button --
they control Start Game directly.

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt -- hostLobby()

### B2. Add redundant sends for START (belt and suspenders for internet games)

Send START packet 3 times with 200ms spacing. Not the real fix (see A1) but useful for
unreliable networks.

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt -- startGame()

### B3. Close lobby socket before game engine launch

Call LobbyService.stopHosting()/stopDiscovery() before launching MainActivity to free port
42400 and prevent interference with engine sockets.

Files:
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt -- launchMultiplayerGame()

## Phase C: Return to Launcher

### C1. Add META_RETURN_TO_LAUNCHER action

New meta action ID (1031) that pushes SDL_QUIT instead of injecting key sequences.
Handled as a special case in meta_action_dispatch().

Files:
- android/app/src/main/cpp/shared/android_meta_actions.h -- #define
- android/app/src/main/cpp/shared/android_meta_actions.c -- special-case handler
- android/app/src/main/java/com/dxxredux/app/TouchBindings.kt -- Kotlin constant + label

### C2. Add overlay Exit button

Add a persistent small "Exit" / "X" button in the TouchOverlayView that dispatches
META_RETURN_TO_LAUNCHER. Always visible when the overlay is active. Positioned in top-left
to avoid conflict with game controls.

Files:
- android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt

## Phase D: Comprehensive Logging

### D1. Add NetLog.log() calls in LobbyService

Add `NetLog.log("LAN", ...)` in every handler and outgoing send function.

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt

### D2. Enable C engine netlog by default on Android

Set GameArg.LogNetTraffic = 1 on Android builds so the engine's netlog.txt is always written.

Files:
- d2/misc/args.c, d1/misc/args.c

### D3. Add net_log_comment() calls in key C engine functions

Add application-level log comments at: net_udp_auto_host, net_udp_auto_join,
net_udp_add_player, net_udp_select_players, net_udp_start_poll (join/leave/auto-start),
net_udp_send_sync, net_udp_read_sync_packet, net_udp_do_join_game,
net_udp_wait_for_sync, net_udp_level_sync. Mirror all in D1.

Files:
- d2/main/net_udp.c, d1/main/net_udp.c

---

## Status

- [x] Phase A1: Fix handleJoin() duplicate logic
- [x] Phase A2: Surface JOIN_REJECT reason
- [x] Phase A3: DUMP_DUPNAME in C engine
- [x] Phase A4: Random callsign + persistence
- [x] Phase B1: Host auto-ready
- [x] Phase B2: Redundant START sends
- [x] Phase B3: Close lobby socket before launch
- [x] Phase C1: META_RETURN_TO_LAUNCHER action
- [x] Phase C2: Overlay Exit button
- [x] Phase D1: NetLog in LobbyService
- [x] Phase D2: Enable C netlog on Android
- [x] Phase D3: net_log_comment() in C engine
- [x] Build verification (assembleDebug passed, no new warnings)

---

## Phase 2: Follow-up Fixes (from testing)

### Phase E: Code Hygiene

#### E1. Add TXT_NET_GAME_DUPNAME constant
Replace the hardcoded "Duplicate callsign" string in NET_DUMP_STRINGS with a proper TXT_ constant
matching the existing pattern (all other dump strings use TXT_ constants via dxx_gettext).
- d1/main/text.h: add `#define TXT_NET_GAME_DUPNAME dxx_gettext(621, "Duplicate callsign")` after
  index 620, bump N_TEXT_STRINGS from 621 to 622
- d2/main/text.h: add `#define TXT_NET_GAME_DUPNAME dxx_gettext(644, "Duplicate callsign")` after
  index 643, N_TEXT_STRINGS stays at 649 (spare slots exist)
- Both text.h: replace `"Duplicate callsign"` with `TXT_NET_GAME_DUPNAME` in NET_DUMP_STRINGS macro

#### E2. Wrap Android debug logging with #ifdef __ANDROID__
Mark the net_log_comment calls added in Phase D3 as Android port debug additions.
- d1 and d2 net_udp.c: wrap each newly-added net_log_comment call block with
  `#ifdef __ANDROID__` / `#endif /* android port: LAN debug logging */` at:
  - net_udp_auto_join, net_udp_auto_host, net_udp_add_player (dupname path),
    net_udp_start_game, net_udp_process_dump
- Do NOT wrap DUMP_DUPNAME, TXT_NET_GAME_DUPNAME, or the duplicate callsign rejection
  logic -- those are functional for all platforms
- args.c changes are already properly wrapped

### Phase F: Build Version Mismatch Warning

#### F1. Add "build" field to lobby protocol
- LobbyProtocol.kt: add `json.put("build", BuildInfo.GIT_COMMIT_COUNT)` to buildAnnounce()
  and buildJoinAck()

#### F2. Parse build in discovery + join
- LobbyService.kt: extract "build" from ANNOUNCE and JOIN_ACK messages, store in
  LanLobbyEntry and joined lobby state

#### F3. Show version mismatch warning in UI
- LanDiscoveryTab.kt: in the discovered lobby list, show warning text when entry.build
  differs from local build. In joined lobby view, show persistent warning banner.
  Non-blocking -- user can still join and play.

### Phase G: UI Fixes

#### G1. Fix portrait button layout
"Back" and "Join by IP" buttons are smashed against the right side in portrait mode.
- MultiplayerScreen.kt LanContent: move "Back to Lobbies" and "Back" from title Row to
  a second Row below the heading
- LanDiscoveryTab.kt: move "Join by IP" to a second Row below "Stop Scanning" / "Host LAN Game"

#### G2. Add (self) tag to player names
In both the host and joiner player lists, label the local player with "(self)".
- LanJoinedLobbyView: display "${player.callsign} (self)" when player.callsign == callsign
- Host view: display "${p.callsign} (self)" when p.callsign == callsign

### Phase H: Critical Bugs

#### H1. Fix NetLog.init() not called on cold start
When "enable logging" is pre-set to ON from a previous run, no log file is present in the
log files list. Root cause: NetLog.init() is never called anywhere. The `enabled` field
starts as `false` and `writer` starts as `null`, so all NetLog.log() calls are no-ops.
- SetupActivity.kt: add NetLog.init(this) early in onCreate()

#### H2. Fix Ready button NetworkOnMainThreadException
Joined player clicking "Ready" doesn't update the host's view. Root cause: setReady() calls
sendTo() directly from the Compose UI thread. DatagramSocket.send() throws
NetworkOnMainThreadException, caught and silently swallowed by the catch block.
- LobbyService.kt setReady(): wrap sendTo() in scope?.launch(Dispatchers.IO)
- Also fix leaveLanLobby() and leaveLobby() which have the same threading bug

### Phase I: Callsign Length Enforcement

#### I1. Enforce 8-char limit in callsign text field
The engine uses CALLSIGN_LEN=8. CallsignPrefs.save() truncates silently, but the TextField
has no limit -- users can type arbitrarily long names without visual feedback.
- MultiplayerScreen.kt: in the callsign OutlinedTextField, filter onValueChange to
  `{ callsign = it.take(CallsignPrefs.MAX_LEN) }` (requires making MAX_LEN internal or public)

### Phase J: Verification

- Build verification: assembleDebug must pass
- Desktop build still compiles (d1/d2 changes guarded or universal)

---

## Phase 2 Status

- [x] Phase E1: TXT_NET_GAME_DUPNAME constant
- [x] Phase E2: #ifdef __ANDROID__ on debug logging
- [x] Phase F1: Add build field to lobby protocol
- [x] Phase F2: Parse build in discovery + join
- [x] Phase F3: Version mismatch warning UI
- [x] Phase G1: Portrait button layout fix
- [x] Phase G2: (self) tag on player names
- [x] Phase H1: NetLog.init() on cold start
- [x] Phase H2: Ready button threading fix
- [x] Phase I1: 8-char callsign limit in TextField
- [x] Phase J: Build verification

---

## Phase 3: Game Launch + Lobby Robustness (from testing)

### Findings
1. Exit overlay button missing on menu screens -- overlay gated by nativeIsInGame() which
   requires Screen_mode==SCREEN_GAME && Game_wind is front. Menus return false, overlay GONE.
2. Client never receives START -- initial sendTo burst in startGame() runs on UI thread,
   same NetworkOnMainThreadException bug as fixed in setReady.
3. No joined lobby timeout -- _joinedLobby never cleared when host disappears.
4. Need more debugging around client START handling.

### Phase K: Always-visible Exit Button

Add a separate small EXIT button view in MainActivity, always visible on the SDL surface.
Similar to SkipButtonView but positioned top-left. Fires META_RETURN_TO_LAUNCHER.

#### K1. Create ExitButtonView
Circular button, top-left corner, always visible. On tap: SDL_QUIT via meta action.

#### K2. Add to MainActivity layout
Add exitButton to FrameLayout. Always VISIBLE (no polling gate).

### Phase L: Fix START Packet Delivery

#### L1. Move initial START sends to IO dispatcher
Wrap first sendTo burst in scope?.launch(Dispatchers.IO).

#### L2. Add START delivery logging
NetLog.log before/after sends and on receipt. Diagnostic for future issues.

### Phase M: Joined Lobby Timeout

#### M1. Track last-seen time from host
Update lastHostSeenMs on PLAYER_LIST, ANNOUNCE from host.

#### M2. Joined-lobby liveness check
In prune loop, check if host unseen for >15s. If stale, auto-leave with diagnostic.

### Phase N: Verification
- assembleDebug
- run-code-quality.ps1

## Phase 3 Status

- [x] Phase K1: ExitButtonView
- [x] Phase K2: Add to MainActivity
- [x] Phase L1: Fix START threading
- [x] Phase L2: START delivery logging
- [x] Phase M1: Track host last-seen time
- [x] Phase M2: Joined lobby liveness check
- [x] Phase N: Build verification
