# Phase 5: Network Events Overlay

## Goal
Show connection status, NAT type, relay/direct indicators, latency in:
1. SetupActivity multiplayer screens (always visible)
2. In-game via admin tray toggle

## Design

### SetupActivity (Compose)
- New `NetworkEventsPanel` composable in `multiplayer/NetworkEventsPanel.kt`
- Shows: connection status, STUN addrs, peer candidates, NAT types, connectivity pairs, relay info, connection methods + latency
- Embedded in `ServerBrowserContent` and `LobbyScreen` above StatusLog
- Reads from `MatchmakingStateHolder.state` via collectAsState()
- Collapsible card so it doesn't dominate the screen

### MainActivity (in-game Canvas View)
- New `NetworkEventsOverlay` in `multiplayer/NetworkEventsOverlay.kt`
- Follows MultiplayerStatsOverlay pattern: Canvas-based, Handler polling, provider lambdas
- Shows: peer connection methods, relay/direct, latency estimates, NAT type
- LEFT-aligned (NetStats is right-aligned) to avoid overlap
- Toggled via admin tray

### Admin tray changes
- Add `ADMIN_NET_EVENTS = 8` constant in TouchOverlayView
- Increment itemCount from 8 to 9
- Add label "Net Events"
- Add toggle handler in MainActivity adminTrayCallback

## Files to create
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsPanel.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsOverlay.kt`

## Files to modify
- `TouchOverlayView.kt` - new constant, label, itemCount
- `MainActivity.kt` - create overlay, wire toggle, add to layout
- `MultiplayerScreen.kt` - embed NetworkEventsPanel in ServerBrowserContent
- `LobbyScreen.kt` - embed NetworkEventsPanel
