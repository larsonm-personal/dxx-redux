# Phase 2.5 Server Implementation Plan

## Goal
Implement the post-auth welcome bundle and client session features:
stable player identity, welcome bundle, ping measurement, player messaging,
and active game listing.

## Changes

### 1. protocol.rs -- new messages and fields
- ServerMessage::ServerStatus { online_players, active_games_count, total_games_played, your_ping_ms }
- ServerMessage::MessageReceived { from_player_id, from_callsign, text }
- ServerMessage::MessageSent { target_player_id }
- ClientMessage::SendMessage { target_player_id, text }
- LobbyInfo: add host_ping_ms: Option<u32>
- ActiveGameInfo struct: host_callsign, mission, mode, player_count, duration_secs

### 2. db.rs -- stable identity
- find_or_create_player_by_gpgs(gpgs_player_id, callsign) -> Uuid
  Look up by gpgs_player_id; if not found, create with new UUID
- is_blocked(player_id, target_id) -> bool
  Check if target has blocked player

### 3. rate_limit.rs -- message rate limit
- Add player_messages: DashMap<Uuid, VecDeque<Instant>>
- check_player_message(player_id) -> bool (5 per 60s)
- Add to cleanup

### 4. ws_handler.rs -- welcome bundle + messaging
- PlayerSession: add ping_ms: Option<u32>
- Auth handler: use find_or_create_player_by_gpgs instead of Uuid::new_v4()
- After AUTH_OK + MOTD: send SERVER_STATUS, LOBBY_LIST, FRIEND_LIST_RESP
- Handle SendMessage: validate text, rate limit, check blocked, send to target
- Include host_ping_ms in LobbyInfo when building lobby list/updates

### 5. Integration tests
- test_welcome_bundle
- test_player_message
- test_player_message_rate_limit
- test_player_message_blocked
- test_stable_player_id
