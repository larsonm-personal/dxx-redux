# Plan: Multiplayer Stats Overlay

## Status: IMPLEMENTED

## Overview
In-game overlay (toggled via admin tray) showing real-time network stats.

## Data Sources

1. **Packet counts**: Add counters to `LocalhostProxy.PeerProxy` (packets/bytes sent & received).
   Expose via `LocalhostProxy.getStats()`.

2. **Ping values**: Read `Netgame.players[i].ping` from the game engine via new JNI call
   `nativeGetMultiplayerPings()`. The game already sends P2P pings at 1/sec
   (UPID_P2P_PING/PONG in net_udp.c), so no extra pings are needed.

3. **Connection type per peer**: From `MatchmakingState.gameLaunchInfo.peers[].isRelay`
   and `MatchmakingState.connectionInfo[].method`. The `PeerConnectionInfoMsg.method` field
   contains the winning strategy: "direct_host", "direct_srflx", "relay", etc.

## Display Elements

- **Packet count**: Total packets sent/received across all peers
- **Ping average**: Exponential moving average (EMA) with alpha tuned for ~10s half-life.
  Formula: `avg = alpha * new_sample + (1 - alpha) * avg`, where `alpha = 1 - exp(-dt/tau)`,
  tau ~= 10s. At 1 sample/sec, alpha ~= 0.095.
- **Ping graph**: 30-second moving line graph, 0-500ms Y-axis
  - Green: 0-250ms
  - Yellow: 250-400ms
  - Red: 400-500ms
  - Capped at 500ms (line stays at top)
  - Only starts populating when overlay opened
- **Connection type**: Show per-peer connection method (relay/direct/srflx/etc.)

## Implementation Plan

### 1. Add packet counters to LocalhostProxy
- `PeerProxy`: add `@Volatile var packetsSent/packetsReceived/bytesSent/bytesReceived`
- `LocalhostProxy.getStats()`: returns list of per-peer stats
- Increment in `forwardLocalToReal()` and `forwardRealToLocal()`

### 2. Add JNI for ping values
- `nativeGetMultiplayerPings()` in jni_main.c: returns int array of player pings
- Reads `Netgame.players[i].ping` for all 8 player slots
- Must also be added to d1/ (shared header or duplicate)

### 3. Create MultiplayerStatsOverlay.kt
- Custom `View` added to the FrameLayout in MainActivity (same layer as overlayContainer)
- Positioned bottom-right or top-right (avoid conflict with touch controls)
- Semi-transparent background
- Canvas-drawn: text for stats, line graph for ping history
- Toggle visibility via touch overlay button or admin tray action

### 4. Overlay lifecycle
- When shown: start 1Hz polling of JNI ping values and proxy stats
- Maintain 30-element ring buffer of ping samples for graph
- Calculate EMA ping with ~10s decay
- When hidden: stop polling, clear graph data

### 5. Wire into MainActivity
- Add the view to the frame layout
- Add toggle mechanism (admin tray, or dedicated button on touch overlay)
- Feed connection type from MatchmakingState

## Files Changed
- `LocalhostProxy.kt` -- packet counters and getStats()
- `jni_main.c` (android/app/src/main/cpp/) -- nativeGetMultiplayerPings JNI
- `d2/main/net_udp.c` -- expose ping array accessor if needed
- `d1/main/net_udp.c` -- same for d1
- `MultiplayerStatsOverlay.kt` (NEW) -- the overlay view
- `MainActivity.kt` -- add overlay to frame, wire toggle
- `TouchOverlayView.kt` -- add toggle button or admin tray entry
