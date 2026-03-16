use std::sync::Arc;

use tracing::info;
use uuid::Uuid;

use crate::db::FriendRow;
use crate::lobby::Presence;
use crate::protocol::{FriendInfo, InGameDetails, ServerMessage};
use crate::ServerState;

/// Handle FRIEND_REQUEST: look up target by callsign, create pending
/// friendship, notify target if online.
pub fn handle_friend_request(
    state: &Arc<ServerState>,
    from_player_id: Uuid,
    from_callsign: &str,
    target_callsign: &str,
) -> ServerMessage {
    if !state.rate_limiter.check_callsign_search(from_player_id) {
        return ServerMessage::RateLimited {
            retry_after_ms: 60_000,
        };
    }

    let target_id = match state.db.find_player_by_callsign(target_callsign) {
        Ok(Some(id)) => id,
        Ok(None) => {
            return ServerMessage::Error {
                code: "PLAYER_NOT_FOUND".into(),
                message: format!("No player with callsign '{target_callsign}'"),
            };
        }
        Err(e) => {
            return ServerMessage::Error {
                code: "DB_ERROR".into(),
                message: e.to_string(),
            };
        }
    };

    if target_id == from_player_id {
        return ServerMessage::Error {
            code: "INVALID_REQUEST".into(),
            message: "Cannot friend yourself".into(),
        };
    }

    if let Err(e) = state.db.add_friend_request(&from_player_id, &target_id) {
        return ServerMessage::Error {
            code: "DB_ERROR".into(),
            message: e.to_string(),
        };
    }

    info!(from = %from_player_id, to = %target_id, "friend request sent");

    // If the target is online, push a notification (caller handles send)
    // Return success to the requester
    ServerMessage::FriendRequestReceived {
        from_player_id,
        from_callsign: from_callsign.to_string(),
    }
}

/// Build the FRIEND_LIST_RESP for a given player, enriched with live
/// presence data from the sessions map.
pub fn build_friend_list(state: &Arc<ServerState>, player_id: &Uuid) -> ServerMessage {
    let friends = match state.db.get_friends(player_id) {
        Ok(f) => f,
        Err(e) => {
            return ServerMessage::Error {
                code: "DB_ERROR".into(),
                message: e.to_string(),
            };
        }
    };

    let mut infos = Vec::with_capacity(friends.len());
    for FriendRow {
        friend_id,
        callsign,
        status: db_status,
    } in &friends
    {
        let (presence_str, details, last_seen) =
            if let Some(session) = state.sessions.get(friend_id) {
                let p = &session.presence;
                let details = match p {
                    Presence::InGame {
                        lobby_id,
                        mission,
                        player_count,
                    } => {
                        // Look up lobby for joinable/max_players
                        state.lobbies.get(lobby_id).map(|lobby| InGameDetails {
                            lobby_id: *lobby_id,
                            mission: mission.clone(),
                            player_count: *player_count,
                            max_players: lobby.max_players,
                            joinable: lobby.is_joinable(),
                        })
                    }
                    _ => None,
                };
                (p.as_str().to_string(), details, None)
            } else {
                ("offline".to_string(), None, Some("unknown".to_string()))
            };

        infos.push(FriendInfo {
            player_id: *friend_id,
            callsign: callsign.clone(),
            status: db_status.clone(),
            presence: presence_str,
            in_game_details: details,
            last_seen,
        });
    }

    ServerMessage::FriendListResp { friends: infos }
}
