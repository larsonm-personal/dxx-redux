# Phase 3 Completion: Identity and Anti-Abuse

## Goal

Complete the remaining Phase 3 items from NETWORKING_PLAN.md:
1. Google Play Games identity verification (identity.rs)
2. Kicked-player rejoin prevention
3. Lobby codes for invite-only sessions

Deferred to later:
- Proof-of-work fallback (complex client-side, not needed for initial release)
- "Verified only" lobby setting (needs identity.rs deployed + real GPGS creds first)

## Changes

### 1. identity.rs -- GPGS Token Verification

New module `server/src/identity.rs`:

```rust
pub enum VerifyResult {
    Ok { gpgs_player_id: String },
    DevMode { identity_key: String },  // when skip_verify is on
    Failed { reason: String },
}

pub async fn verify_gpgs_token(
    client_id: &str,
    client_secret: &str,
    auth_code: &str,
) -> VerifyResult
```

Flow:
1. POST https://oauth2.googleapis.com/token with authorization_code grant
2. Parse response for access_token
3. GET https://www.googleapis.com/games/v1/players/me with Bearer access_token
4. Extract playerId field
5. Return VerifyResult::Ok { gpgs_player_id }

On any error: return VerifyResult::Failed with descriptive reason.

### 2. config.rs -- New field

- `skip_gpgs_verify: bool` from SKIP_GPGS_VERIFY env var (default: false)
- When true, use play_games_token directly as identity key (current behavior)
- Integration tests set this to true

### 3. ws_handler.rs -- Wire identity verification

In AUTHENTICATE handler:
- If skip_gpgs_verify: use play_games_token as identity key (current behavior)
- If not skip: call identity::verify_gpgs_token()
- On failure: send AuthFail and disconnect
- On success: pass gpgs_player_id to find_or_create_player_by_gpgs

### 4. lobby.rs -- Kicked players set

Add `kicked_players: HashSet<Uuid>` to Lobby struct.
Populated in remove_player when called from KickPlayer handler path.

### 5. ws_handler.rs -- Kicked-player rejoin prevention

In JoinLobby handler: check lobby.kicked_players.contains(&player_id)
In JoinFriendGame handler: same check
Send specific error code "KICKED_FROM_LOBBY" so client can show appropriate message.

### 6. protocol.rs -- Lobby code support

Add to CreateLobby: `lobby_code: Option<String>`
Add to JoinLobby: `lobby_code: Option<String>`
Add to Lobby: `code: Option<String>`
Add to LobbyInfo: `has_code: bool` (don't leak the actual code)
Add to JoinFriendGameResp: already has lobby_id, friend-join bypasses code

### 7. ws_handler.rs -- Lobby code logic

CreateLobby: store code on lobby
ListLobbies: include has_code in LobbyInfo
JoinLobby: verify code matches if lobby has one
JoinFriendGame: bypass code check (friends can always join)

### 8. lib.rs -- Register identity module

Add `pub mod identity;`

### 9. Integration tests

- test_gpgs_dev_mode: verify skip_gpgs_verify works (existing tests implicitly do this)
- test_kicked_player_rejoin_prevention: kick player, verify they can't rejoin
- test_lobby_code_required: create lobby with code, join without code fails, join with correct code succeeds
- test_lobby_code_bypass_friend: friend can join coded lobby without code
- test_lobby_code_listing: coded lobbies show has_code=true but not the code itself

## Files Modified

| File | Changes |
|------|---------|
| server/src/identity.rs | New: GPGS token verification |
| server/src/config.rs | Add skip_gpgs_verify field |
| server/src/lib.rs | Add pub mod identity |
| server/src/ws_handler.rs | Wire identity, kicked rejoin, lobby codes |
| server/src/lobby.rs | Add kicked_players, code fields |
| server/src/protocol.rs | Add lobby_code fields, has_code |
| server/tests/integration.rs | New tests, update TestServer config |
