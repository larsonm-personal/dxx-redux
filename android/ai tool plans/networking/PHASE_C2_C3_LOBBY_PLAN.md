# Phase C2 Completion + C3 Lobby Screen Plan

## Status: COMPLETE

## Goal
Complete Phase C2 (server browser with join button + ping) and implement
Phase C3 (LobbyScreen with player list, ready toggle, host controls).
Add live client+server integration tests.

## Changes Required

### 1. NetworkProtocol.kt -- add KICKED message type
- Add `KickedMsg` data class (server sends ERROR with code "KICKED")
  - Actually, kicked players receive an ERROR message with code "KICKED"
    followed by WebSocket close. No separate KICKED message type needed.
    We'll handle it via the existing ErrorMsg path.

### 2. MatchmakingState.kt -- add lobby state tracking
- Add `currentLobby: CurrentLobbyState?` field
- `CurrentLobbyState` data class: lobbyId, players (List<LobbyPlayerInfo>),
  isHost flag, hostPlayerId
- Add `MultiplayerNav` enum: BROWSER, LOBBY (controls which sub-screen shows)
- Add `nav: MultiplayerNav` field

### 3. MatchmakingService.kt -- add lobby actions
- `joinLobby(lobbyId, code?)` -- send JOIN_LOBBY, expect LOBBY_UPDATE
- `leaveLobby()` -- send LEAVE_LOBBY, clear currentLobby
- `setReady(ready)` -- send READY
- `startGame()` -- send START_GAME (host only)
- `kickPlayer(playerId)` -- send KICK_PLAYER (host only)
- Update `handleMessage` for LOBBY_UPDATE: populate currentLobby state
- Update `handleMessage` for GAME_STARTING: log + keep for future use
- Handle ERROR with code containing "kick" -> navigate back to browser

### 4. MultiplayerScreen.kt -- add join button
- Add "Join" button to LobbyCard (visible when lobby.joinable && !lobby.hasCode)
- Add join-with-code dialog for code-protected lobbies
- When currentLobby is set, delegate to LobbyScreen instead of browser
- Add ping display in LobbyCard (lobby.hostPingMs)

### 5. LobbyScreen.kt -- new file
- Player list showing callsign, ready status, ping
- Ready toggle button
- Leave button (returns to browser)
- Host-only: Start Game button, Kick player button
- Reactive updates via LOBBY_UPDATE messages
- Handle GAME_STARTING (show "Game starting..." toast/message)
- Handle disconnect (return to browser)

### 6. Integration Tests
- Add `android/tests/test_multiplayer_live.ps1`:
  Start Rust server, connect via OkHttp in a JVM test or script,
  validate auth + lobby create + join + ready + leave flow
- Add JVM unit test for NetworkProtocol serialization roundtrip

### 7. Server-side
- No server changes needed -- all required message handlers already exist
  (JOIN_LOBBY, LEAVE_LOBBY, READY, START_GAME, KICK_PLAYER, LOBBY_UPDATE
   broadcast are all implemented and tested)

## File-by-file changes

| File | Action |
|------|--------|
| NetworkProtocol.kt | Add StartGameMsg, KickPlayerMsg client messages |
| MatchmakingState.kt | Add CurrentLobbyState, MultiplayerNav, nav field |
| MatchmakingService.kt | Add joinLobby, leaveLobby, setReady, startGame, kickPlayer; update handleMessage |
| MultiplayerScreen.kt | Add Join button, ping display, nav dispatch to LobbyScreen |
| LobbyScreen.kt | New: player list, ready toggle, leave, host controls |
| test_multiplayer_live.ps1 | New: live test script |
