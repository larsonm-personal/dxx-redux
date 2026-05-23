# Plan: Multiplayer & Controller Fixes

**STATUS: ALL PHASES IMPLEMENTED** -- D1+D2 builds clean, code quality passes.

Six issues across controller mapping, LAN multiplayer navigation, and backgrounding.

## Issues

1. Triggers (mapped to half-axis Slide Up/Down) also fire primary/secondary weapon
2. No reliable way to identify trigger axes -- support half or full axis for ALL axes
3. LAN scanning screen doesn't trap "back" to go back to multiplayer overview
4. Multiplayer base screen doesn't trap "back" to go back to launcher
5. Clicking "Join" in LAN mode doesn't do anything visible (no lobby screen transition)
6. Hosted game in LAN disappears when app is backgrounded

## Phase 1: Controller Half-Axis / Trigger Mapping Fixes

**Root cause:** When triggers are remapped from button functions to half-axis, the
config generation may leave stale button-function entries. Additionally, BUTTON_CONTROLS
maps "LT"->19 and "RT"->21 (axis-button synthetic indices), so when buildJoyPairs
processes RT with a half-axis function, it adds RT to handledAsHalfAxis, but if there's
also a prior stale binding for button 19/21 at a button kc_index, it could survive.

### Step 1.1: Audit and fix buildJoyPairs mutual exclusivity

Verify that when controlId (e.g. "RT") is in handledAsHalfAxis, the second loop
properly skips it. Current code: `if (controlId in handledAsHalfAxis) continue` --
this skips both the BUTTON_CONTROLS path and the AXIS_CONTROLS path. This should
already prevent RT from emitting button 21 at kc_index 0 (Fire Primary).

However: if an EARLIER config had "RT" -> "Fire Primary", that wrote kc_index 0 = 21.
A NEW config with "RT" -> "Slide Up" (half-axis) no longer writes kc_index 0 at all.
Since kconfig_fill_joy_settings starts from 0xFF, kc_index 0 will be 0xFF (unbound).
So the C side should be clean.

The bug must be elsewhere. Check if the DEFAULT bindings include RT as a button
AND the new config adds RT as half-axis, but the defaults file has "RT": "Accelerate"
which IS in HALF_AXIS_MAP. The default.json maps RT -> Accelerate, LT -> Reverse.
These are already half-axis functions ("Accelerate" -> Pair("Throttle", false)).

So the default config already uses half-axis for triggers. The user's custom config
maps triggers to Slide Up/Down which are also in HALF_AXIS_MAP. The generation should
be correct.

Possible actual cause: the user has an OLD pilot file with embedded KeySettings from
before the half-axis system was implemented, and the pilot patcher isn't re-patching
on config change. Or the pilot patcher is patching correctly but the game re-reads
defaults on load.

ACTION: Add debug logging in saveConfig() to dump the full byte array after generation.
Add a config version bump that forces re-patching of pilot files.

### Step 1.2: Support half-axis or full-axis for ALL analog axes

Use InputDevice.MotionRange as UI hint:
- min=0,max=1 -> suggest half-axis options first (triggers)
- min=-1,max=1 -> suggest full-axis options first (sticks)
- Offer BOTH options regardless

The buildJoyPairs logic already handles this generically (checks HALF_AXIS_MAP + AXIS_CONTROLS).
No C changes needed.

Files:
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt

## Phase 2: Back Button Navigation

Add BackHandler to each nav state in MultiplayerScreen:
- BROWSER: BackHandler(onBack = onBack) -> returns to launcher
- LAN: BackHandler -> goes to BROWSER
- FRIENDS: BackHandler -> goes to BROWSER
- LOBBY: BackHandler -> calls leave lobby, goes to BROWSER

Files:
- android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt

## Phase 3: LAN Join Flow

### Step 3.1: Add JOIN_ACK message type

Host responds to JOIN with JOIN_ACK containing lobby details.
Protocol addition: MSG_JOIN_ACK, buildJoinAck()

### Step 3.2: Track joined lobby state

Add to LobbyService: joinedLobbyId, joinedHostAddr, joinedLobbyInfo StateFlows.
Set on JOIN_ACK, cleared on LEAVE or timeout.

### Step 3.3: LAN lobby screen

When joinedLobbyId is non-null, show player list, Ready/Unready, Leave.
Detect host disconnect if PLAYER_LIST stops arriving.

### Step 3.4: Navigate on successful join

In LanContent, observe joinedLobbyId. When set, show lobby view.
When cleared, return to discovery.

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt

## Phase 4: Multiplayer Backgrounding

### Step 4.1: Foreground service

Create MultiplayerForegroundService with persistent notification.
Add FOREGROUND_SERVICE + FOREGROUND_SERVICE_CONNECTED_DEVICE permissions.
Started on host/join LAN/online lobby. Stopped on leave/game-end.

### Step 4.2: Suppress pause during multiplayer

In MainActivity.onStop(), skip Escape injection when multiplayer active.
Add nativeOnPauseMultiplayer() or flag check in nativeOnPause().

### Step 4.3: Surface-less operation

Verify multiplayer network code runs without render surface.

Files:
- android/app/src/main/AndroidManifest.xml
- NEW: android/app/src/main/java/com/dxxredux/app/MultiplayerForegroundService.kt
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt
- android/app/src/main/cpp/android_input.c

## Phase 5: Verification

1. Controller: triggers as half-axis -> no weapon fire; as button -> fire works
2. Back button: system back from each MP screen navigates correctly
3. LAN join: host+join, lobby screen shows, player list, ready/leave
4. Background: host/join, Home, notification+session alive, resume ok
5. Build D1+D2, run android/run-code-quality.ps1 --fix
