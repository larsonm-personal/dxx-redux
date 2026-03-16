# Phase C2.5 Create Lobby UI + C7 Messaging + Connection Info Plan

## Status: COMPLETE

## Goal
Add Create Lobby UI, in-lobby player messaging (C7), and CONNECTION_INFO
handling so the client can exercise more server functionality in live tests.
Add server integration tests covering multi-client messaging in lobby context.

## Changes (all complete)

### 1. NetworkProtocol.kt
- Added SendMessageMsg (client -> server)
- Added MessageReceivedMsg, MessageSentMsg (server -> client) parsed types
- Added ConnectionInfoMsg, PeerConnectionInfoMsg parsed types
- Added to ServerMessage sealed class and parse()

### 2. MatchmakingState.kt
- Added ChatMessage data class
- Added chatMessages: List<ChatMessage> to state (ring buffer, 50 messages)
- Added connectionInfo: List<PeerConnectionInfoMsg>

### 3. MatchmakingService.kt
- sendMessage(targetPlayerId, text) method
- sendLobbyChat(text) -- broadcasts to all lobby players
- Handle MESSAGE_RECEIVED -> append to chatMessages
- Handle MESSAGE_SENT -> delivery confirmation (no UI action)
- Handle CONNECTION_INFO -> store in state
- Clear chatMessages/connectionInfo on disconnect, leave, kick, close, failure

### 4. MultiplayerScreen.kt
- Added "Create Lobby" button next to Refresh/Disconnect
- CreateLobbyDialog: game (d1/d2 toggle), mission, mode (anarchy/coop toggle), max players

### 5. LobbyScreen.kt
- Added LobbyChatArea: message list (LazyColumn) + text input + send button
- Auto-scroll on new messages
- Messages prefixed with callsign, own messages colored differently

### 6. Server integration tests (2 new, total 50 integration tests)
- test_lobby_chat_flow: two players in lobby exchange messages bidirectionally
- test_create_lobby_details: create d1 lobby, verify listing has correct fields
