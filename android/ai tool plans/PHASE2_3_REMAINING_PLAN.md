# Phase 2+3 Remaining Server Features Plan

STATUS: COMPLETED

All items implemented and tested. 38 integration tests pass, clippy clean, fmt clean.

## 1. Predictive Port Allocation (Phase 2 remaining) -- DONE

When a client reports `nat_type: "symmetric"` in their STUN_RESULT, and they
have exactly 2 srflx candidates (from querying two STUN servers), the server
can detect sequential port allocation and generate predicted candidates
automatically.

### Algorithm
- When STUN_RESULT arrives with nat_type containing "symmetric":
  - Find all srflx candidates for this player
  - If exactly 2 srflx candidates with same IP but different ports:
    - Parse ports P1 and P2
    - Compute delta = P2 - P1
    - If |delta| <= 10 (sequential or near-sequential):
      - Predict next ports: P2+delta, P2+2*delta (2 predicted candidates)
      - Inject these as "predicted" type candidates into the player's list
      - Include them in PEER_CANDIDATES broadcast to other players

### Files to modify
- server/src/ws_handler.rs: Add `generate_predicted_candidates()` function,
  call it in StunResult handler after storing candidates

## 2. Verified-Only Lobby Setting (Phase 3 remaining) -- DONE

Hosts can create lobbies that only allow GPGS-verified players (not dev-mode
identities).

### Changes
- protocol.rs: Add `#[serde(default)] verified_only: bool` to CreateLobby
- protocol.rs: Add `verified_only: bool` to LobbyInfo
- lobby.rs: Add `verified_only: bool` to Lobby struct and `new()` params
- ws_handler.rs: 
  - Add `gpgs_verified: bool` to PlayerSession (true when real GPGS, false for dev mode)
  - Pass verified_only to Lobby::new() in CreateLobby handler
  - In JoinLobby/JoinFriendGame: check verified_only and reject unverified players
  - Include verified_only in build_lobby_list
- http_api.rs: Include verified_only in status endpoint LobbyInfo

## 3. Integration Tests -- DONE (5 new tests)
- test_predictive_port_candidates: sequential symmetric NAT gets predicted candidates
- test_predictive_port_skipped_for_random_nat: random symmetric NAT gets NO predicted candidates
- test_verified_only_lobby_rejected: unverified player rejected from verified-only lobby
- test_verified_only_in_listing: lobby listing includes verified_only flag
- test_verified_only_friend_join_rejected: unverified friend rejected via JOIN_FRIEND_GAME
