# Phase C1: Matchmaking Client -- Minimum Viable Connection

**STATUS: COMPLETE**

## Result

All items in scope were implemented and tested end-to-end:
- Android client connects via WebSocket to the Rust matchmaking server
- Authenticates with dev token (skip_gpgs_verify mode)
- Receives and displays AUTH_OK, MOTD, SERVER_STATUS, LOBBY_LIST
- Disconnect/reconnect flow works
- Back navigation to setup screen works

## Files created/modified

1. **Dependencies**: OkHttp (WebSocket), kotlinx-serialization (JSON)
2. **NetworkConstants.kt**: server URL, protocol version
3. **NetworkProtocol.kt**: message data classes for the subset we need:
   - Client: AUTHENTICATE
   - Server: AUTH_OK, AUTH_FAIL, SERVER_STATUS, LOBBY_LIST,
     FRIEND_LIST_RESP, MOTD, ERROR
4. **MatchmakingState.kt**: StateFlow-based state for Compose
5. **MatchmakingService.kt**: OkHttp WebSocket lifecycle, auth, message dispatch
6. **MultiplayerScreen.kt**: basic Compose screen showing connection status,
   online players, lobby count, server browser list
7. **SetupActivity.kt**: add "Multiplayer" navigation button

## Not in scope (later phases)

- Google Play Games sign-in (use dev token for now)
- Lobby joining/creation
- Friends tab
- LAN discovery
- NAT traversal / STUN
- Chat/messaging

## Server URL

- Emulator: `ws://10.0.2.2:9000/ws` (Android emulator host loopback alias)
- Local device on same WiFi: `ws://<host-ip>:9000/ws`
- Production: `wss://matchmaking.dxxredux.com/ws` (future)

## Testing approach

1. Start the Rust server locally: `cd server && cargo run`
2. Deploy APK to emulator
3. Tap "Multiplayer" button
4. Verify: connects, authenticates, shows online count + lobby list
