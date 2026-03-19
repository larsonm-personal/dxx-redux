use std::net::SocketAddr;
use std::sync::Arc;
use std::time::Duration;

use axum::extract::ws::{Message, WebSocket};
use axum::extract::{ConnectInfo, State, WebSocketUpgrade};
use axum::response::IntoResponse;
use axum::routing::get;
use axum::Router;
use futures_util::{SinkExt, StreamExt};
use tokio::sync::mpsc;
use tracing::{debug, info, warn};
use uuid::Uuid;

use crate::db::{MatchPlayerData, MatchResultData};
use crate::friends;
use crate::identity;
use crate::lobby::{Lobby, LobbyState, Presence};
use crate::pow;
use crate::protocol::*;
use crate::ServerState;

/// Wrapper around bounded mpsc::Sender that provides fire-and-forget send.
/// Drops messages silently if the channel is full (slow client backpressure).
#[derive(Debug, Clone)]
pub struct BoundedSender(mpsc::Sender<ServerMessage>);

impl BoundedSender {
    #[allow(clippy::result_large_err)]
    pub fn send(&self, msg: ServerMessage) -> Result<(), mpsc::error::TrySendError<ServerMessage>> {
        self.0.try_send(msg)
    }
}

/// Build a LOBBY_UPDATE message from the current lobby state.
fn build_lobby_update(lobby: &Lobby) -> ServerMessage {
    let players = lobby
        .players
        .iter()
        .map(|p| LobbyPlayerInfo {
            player_id: p.player_id,
            callsign: p.callsign.clone(),
            ready: p.ready,
            ping_ms: p.ping_ms,
            connection_type: p.connection_type.as_str().to_string(),
        })
        .collect();
    ServerMessage::LobbyUpdate {
        lobby_id: lobby.id,
        players,
    }
}

/// Broadcast a message to all players in a lobby.
fn broadcast_to_lobby(lobby: &Lobby, state: &ServerState, msg: &ServerMessage) {
    for p in &lobby.players {
        if let Some(session) = state.sessions.get(&p.player_id) {
            let _ = session.tx.send(msg.clone());
        }
    }
}

/// Broadcast LOBBY_UPDATE to all players in a lobby.
fn broadcast_lobby_update(lobby: &Lobby, state: &ServerState) {
    let msg = build_lobby_update(lobby);
    broadcast_to_lobby(lobby, state, &msg);
}

/// Build the list of waiting lobbies for LOBBY_LIST responses.
fn build_lobby_list(state: &ServerState) -> Vec<LobbyInfo> {
    state
        .lobbies
        .iter()
        .filter(|entry| entry.value().state == LobbyState::Waiting)
        .map(|entry| {
            let l = entry.value();
            let host_ping_ms = state
                .sessions
                .get(&l.host_player_id)
                .and_then(|s| s.ping_ms);
            LobbyInfo {
                lobby_id: l.id,
                host_callsign: l.host_callsign.clone(),
                game: l.game.clone(),
                mission: l.mission.clone(),
                mode: l.mode.clone(),
                player_count: l.player_count(),
                max_players: l.max_players,
                joinable: l.is_joinable(),
                host_ping_ms,
                has_code: l.code.is_some(),
                verified_only: l.verified_only,
            }
        })
        .collect()
}

/// Build a list of active (in-progress) games, capped at 20.
fn build_active_game_list(state: &ServerState) -> Vec<ActiveGameInfo> {
    let now = chrono::Utc::now();
    state
        .lobbies
        .iter()
        .filter(|entry| {
            matches!(
                entry.value().state,
                LobbyState::Starting | LobbyState::InGame
            )
        })
        .take(20)
        .map(|entry| {
            let l = entry.value();
            let elapsed = (now - l.created_at).num_seconds().max(0) as u64;
            ActiveGameInfo {
                host_callsign: l.host_callsign.clone(),
                mission: l.mission.clone(),
                mode: l.mode.clone(),
                player_count: l.player_count(),
                duration_secs: elapsed,
            }
        })
        .collect()
}

/// Per-player session state, stored in the shared sessions DashMap.
pub struct PlayerSession {
    pub player_id: Uuid,
    pub callsign: String,
    pub presence: Presence,
    /// Channel to send messages to this player's WebSocket task.
    pub tx: BoundedSender,
    /// Which lobby (if any) this player is in.
    pub lobby_id: Option<Uuid>,
    /// Measured ping to this player's WebSocket connection (ms).
    pub ping_ms: Option<u32>,
    /// True if the player authenticated via real GPGS verification.
    pub gpgs_verified: bool,
    /// Authentication method: "gpgs", "keypair", or "dev".
    pub auth_method: String,
}

/// Build the WebSocket router (separated for testability).
pub fn ws_router(state: Arc<ServerState>) -> Router {
    Router::new()
        .route("/ws", get(ws_upgrade))
        .with_state(state)
}

/// Run the WebSocket server.
pub async fn run(
    addr: SocketAddr,
    state: Arc<ServerState>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let app = ws_router(state);

    let listener = tokio::net::TcpListener::bind(addr).await?;
    info!(%addr, "WebSocket server listening");
    axum::serve(
        listener,
        app.into_make_service_with_connect_info::<SocketAddr>(),
    )
    .await?;
    Ok(())
}

async fn ws_upgrade(
    ws: WebSocketUpgrade,
    ConnectInfo(addr): ConnectInfo<SocketAddr>,
    State(state): State<Arc<ServerState>>,
) -> impl IntoResponse {
    // D7: global connection cap
    let max_conn = state.config.max_connections;
    if max_conn > 0 && state.sessions.len() >= max_conn {
        warn!(%addr, max_connections = max_conn, "connection cap reached");
        return (axum::http::StatusCode::SERVICE_UNAVAILABLE, "server full").into_response();
    }
    if !state.rate_limiter.check_ip_connection(addr.ip()) {
        warn!(%addr, "WebSocket upgrade rate-limited");
        return (axum::http::StatusCode::TOO_MANY_REQUESTS, "rate limited").into_response();
    }
    debug!(%addr, "WebSocket upgrade accepted");
    ws.on_upgrade(move |socket| handle_connection(socket, addr, state))
        .into_response()
}

async fn handle_connection(socket: WebSocket, addr: SocketAddr, state: Arc<ServerState>) {
    let (mut ws_tx, mut ws_rx) = socket.split();

    // Bounded channel for sending messages back to this client.
    // Backpressure: if the client falls 256 messages behind, we drop it.
    let (raw_tx, mut rx) = mpsc::channel::<ServerMessage>(256);
    let tx = BoundedSender(raw_tx);

    // Task: drain the outbound channel and send to the WebSocket
    let send_task = tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            let text = match serde_json::to_string(&msg) {
                Ok(t) => t,
                Err(e) => {
                    warn!(%e, "failed to serialize server message");
                    continue;
                }
            };
            // Timeout on send: if the client's TCP window is full for 10s, disconnect
            match tokio::time::timeout(Duration::from_secs(10), ws_tx.send(Message::Text(text.into()))).await {
                Ok(Ok(())) => {}
                Ok(Err(_)) => break,  // send error
                Err(_) => {
                    warn!("WebSocket send timed out, dropping connection");
                    break;
                }
            }
        }
    });

    // Authenticated player ID (set after AUTHENTICATE)
    let mut player_id: Option<Uuid> = None;
    let session_id = Uuid::new_v4(); // for rate limiting pre-auth

    // Pending keypair PoW state (set when a new keypair needs PoW before registration)
    struct PendingPow {
        challenge: String,
        callsign: String,
        platform: String,
        pubkey_hash: String,
    }
    let mut pending_pow: Option<PendingPow> = None;

    // Read loop -- timeout if no message (including pings) for 120s
    while let Some(msg_result) = match tokio::time::timeout(Duration::from_secs(120), ws_rx.next()).await {
        Ok(Some(r)) => Some(r),
        Ok(None) => None,       // stream ended
        Err(_) => {
            debug!(%addr, "WebSocket read timed out (120s idle)");
            None  // treat as disconnect
        }
    } {
        let msg = match msg_result {
            Ok(Message::Text(text)) => text,
            Ok(Message::Close(_)) => break,
            Ok(_) => continue,
            Err(e) => {
                debug!(%addr, %e, "WebSocket read error");
                break;
            }
        };

        // Rate-limit raw message rate
        if !state.rate_limiter.check_ws_message(session_id) {
            warn!(%addr, session = %session_id, "WebSocket message rate-limited");
            let _ = tx.send(ServerMessage::RateLimited {
                retry_after_ms: 1000,
            });
            continue;
        }

        let client_msg: ClientMessage = match serde_json::from_str(&msg) {
            Ok(m) => m,
            Err(e) => {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_MESSAGE".into(),
                    message: e.to_string(),
                });
                continue;
            }
        };

        match client_msg {
            ClientMessage::Authenticate {
                protocol_version,
                client_version,
                play_games_token,
                callsign,
                platform,
                auth_method,
                public_key,
                auth_timestamp,
                auth_signature,
            } => {
                // Version check first
                if protocol_version < MIN_CLIENT_PROTOCOL {
                    warn!(%addr, %client_version, %protocol_version, required = MIN_CLIENT_PROTOCOL, "client version rejected");
                    let _ = tx.send(ServerMessage::VersionRejected {
                        reason: format!(
                            "Your client is too old (v{client_version}). \
                             Please update to the latest version."
                        ),
                        required_version: MIN_CLIENT_PROTOCOL,
                        required_version_name: "1.0.0".into(),
                        current_server_version: CURRENT_PROTOCOL,
                        update_url: state.config.update_url.clone(),
                    });
                    break; // disconnect
                }

                // Validate callsign: max 20 chars, printable ASCII only
                let callsign = callsign.trim().to_string();
                if callsign.is_empty() || callsign.len() > 20
                    || !callsign.bytes().all(|b| (0x20..=0x7E).contains(&b))
                {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Invalid callsign (1-20 printable ASCII characters)".into(),
                    });
                    break;
                }

                if auth_method == "keypair" {
                    // -- Keypair authentication path --
                    let Some(ref pk) = public_key else {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "public_key required for keypair auth".into(),
                        });
                        break;
                    };
                    let Some(ts) = auth_timestamp else {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "auth_timestamp required for keypair auth".into(),
                        });
                        break;
                    };
                    let Some(ref sig) = auth_signature else {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "auth_signature required for keypair auth".into(),
                        });
                        break;
                    };

                    // Validate timestamp freshness (120s window)
                    let now = std::time::SystemTime::now()
                        .duration_since(std::time::UNIX_EPOCH)
                        .unwrap_or_default()
                        .as_secs();
                    if now.abs_diff(ts) > 120 {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "auth_timestamp too old or in the future".into(),
                        });
                        break;
                    }

                    // Verify Ed25519 signature of "callsign:timestamp"
                    let signed_message = format!("{callsign}:{ts}");
                    if !pow::verify_ed25519_signature(pk, signed_message.as_bytes(), sig) {
                        warn!(%addr, "keypair auth: invalid signature");
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Invalid signature".into(),
                        });
                        break;
                    }

                    let pubkey_hash = pow::hash_pubkey(pk);

                    // Check if this keypair is already registered
                    match state.db.find_player_by_keypair(&pubkey_hash) {
                        Ok(Some(pid)) => {
                            // Known key -- fast path, complete auth
                            if state.db.is_banned(&pid).unwrap_or(false) {
                                warn!(%pid, %addr, "banned keypair player attempted authentication");
                                let _ = tx.send(ServerMessage::AuthFail {
                                    reason: "Your account has been banned.".into(),
                                });
                                break;
                            }
                            // Update callsign
                            let _ = state.db.find_or_create_player_by_gpgs(
                                &format!("keypair:{pubkey_hash}"),
                                &callsign,
                            );
                            complete_auth(
                                &state, &tx, pid, &callsign, &platform, false, "keypair",
                                session_id, addr,
                            );
                            player_id = Some(pid);
                        }
                        Ok(None) => {
                            // New key -- require proof-of-work
                            let challenge = pow::generate_challenge();
                            info!(%addr, "keypair auth: new key, sending PoW challenge");
                            let _ = tx.send(ServerMessage::PowChallenge {
                                challenge: challenge.clone(),
                                difficulty: state.config.pow_difficulty,
                            });
                            pending_pow = Some(PendingPow {
                                challenge,
                                callsign,
                                platform,
                                pubkey_hash,
                            });
                        }
                        Err(e) => {
                            warn!(%e, "keypair DB lookup failed");
                            let _ = tx.send(ServerMessage::AuthFail {
                                reason: "Internal server error".into(),
                            });
                            break;
                        }
                    }
                } else {
                    // -- GPGS authentication path (existing) --
                    let token = play_games_token.unwrap_or_default();
                    let gpgs_id = if state.config.skip_gpgs_verify
                        || state.config.google_client_id.is_empty()
                    {
                        debug!(%addr, "GPGS verification skipped (dev mode)");
                        token.clone()
                    } else {
                        match identity::verify_gpgs_token(
                            &state.config.google_client_id,
                            &state.config.google_client_secret,
                            &token,
                        )
                        .await
                        {
                            identity::VerifyResult::Ok { gpgs_player_id } => gpgs_player_id,
                            identity::VerifyResult::Failed { reason } => {
                                warn!(%addr, %reason, "GPGS authentication failed");
                                let _ = tx.send(ServerMessage::AuthFail {
                                    reason: "Google Play Games verification failed".into(),
                                });
                                break;
                            }
                        }
                    };
                    let is_gpgs_verified =
                        !state.config.skip_gpgs_verify && !state.config.google_client_id.is_empty();

                    let pid = match state.db.find_or_create_player_by_gpgs(&gpgs_id, &callsign) {
                        Ok(id) => id,
                        Err(e) => {
                            warn!(%e, "failed to look up/create player");
                            let _ = tx.send(ServerMessage::AuthFail {
                                reason: "Internal server error".into(),
                            });
                            break;
                        }
                    };

                    if state.db.is_banned(&pid).unwrap_or(false) {
                        warn!(%pid, %addr, "banned player attempted authentication");
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Your account has been banned.".into(),
                        });
                        break;
                    }

                    let method = if is_gpgs_verified { "gpgs" } else { "dev" };
                    complete_auth(
                        &state,
                        &tx,
                        pid,
                        &callsign,
                        &platform,
                        is_gpgs_verified,
                        method,
                        session_id,
                        addr,
                    );
                    player_id = Some(pid);
                }
            }

            ClientMessage::PowSolution {
                challenge,
                solution,
            } => {
                let Some(ref pp) = pending_pow else {
                    let _ = tx.send(ServerMessage::Error {
                        code: "NO_PENDING_POW".into(),
                        message: "No proof-of-work challenge pending".into(),
                    });
                    continue;
                };
                if challenge != pp.challenge {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Challenge mismatch".into(),
                    });
                    break;
                }
                if !pow::verify_pow(&challenge, &solution, state.config.pow_difficulty) {
                    warn!(%addr, "PoW solution failed verification");
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Invalid proof-of-work solution".into(),
                    });
                    break;
                }

                // PoW valid -- register the keypair and complete auth
                let pid = match state
                    .db
                    .register_keypair_player(&pp.pubkey_hash, &pp.callsign)
                {
                    Ok(id) => id,
                    Err(e) => {
                        warn!(%e, "keypair registration failed");
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Internal server error".into(),
                        });
                        break;
                    }
                };

                if state.db.is_banned(&pid).unwrap_or(false) {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Your account has been banned.".into(),
                    });
                    break;
                }

                info!(%pid, callsign = %pp.callsign, "new keypair player registered via PoW");
                complete_auth(
                    &state,
                    &tx,
                    pid,
                    &pp.callsign,
                    &pp.platform,
                    false,
                    "keypair",
                    session_id,
                    addr,
                );
                player_id = Some(pid);
                pending_pow = None;
            }

            // All other messages require authentication
            _ => {
                let Some(pid) = player_id else {
                    let _ = tx.send(ServerMessage::Error {
                        code: "NOT_AUTHENTICATED".into(),
                        message: "Must authenticate first".into(),
                    });
                    continue;
                };
                handle_authenticated_message(&state, pid, client_msg, &tx).await;
            }
        }
    }

    // Cleanup on disconnect
    if let Some(pid) = player_id {
        // Remove from lobby if in one
        let lobby_id = state.sessions.get(&pid).and_then(|s| s.lobby_id);
        if let Some(lobby_id) = lobby_id {
            let should_remove = if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                let was_host = lobby.host_player_id == pid;
                lobby.remove_player(&pid);
                if lobby.players.is_empty() {
                    true
                } else if was_host {
                    // Host left: dissolve the lobby and notify remaining players
                    let kick_msg = ServerMessage::Error {
                        code: "HOST_DISCONNECTED".into(),
                        message: "The host has disconnected.".into(),
                    };
                    for p in lobby.players.iter() {
                        if let Some(sess) = state.sessions.get(&p.player_id) {
                            let _ = sess.tx.send(kick_msg.clone());
                        }
                        // Clear each player's lobby_id
                        if let Some(mut s) = state.sessions.get_mut(&p.player_id) {
                            s.lobby_id = None;
                        }
                    }
                    true // remove the orphaned lobby
                } else {
                    // Non-host left: broadcast updated player list
                    broadcast_lobby_update(&lobby, &state);
                    false
                }
            } else {
                false
            };
            if should_remove {
                state.lobbies.remove(&lobby_id);
                state.stats.lobby_closed();
            }
        }
        state.sessions.remove(&pid);
        state.stats.player_disconnected();
        // Remove client IP from STUN allowlist (ref-counted, D17 fix)
        let ip = addr.ip();
        let remove = state
            .stun_allowlist
            .get(&ip)
            .map(|c| *c <= 1)
            .unwrap_or(true);
        if remove {
            state.stun_allowlist.remove(&ip);
        } else if let Some(mut c) = state.stun_allowlist.get_mut(&ip) {
            *c -= 1;
        }
        let _ = state
            .db
            .log_connection_event(Some(&pid), "disconnect", None);
        info!(%pid, "player disconnected");
    }

    // Drop the sender so the send_task can drain remaining messages
    // and exit cleanly instead of being aborted mid-flight.
    drop(tx);
    let _ = send_task.await;
}

/// Pick the best candidate address for direct connection: prefer srflx, then host.
fn best_candidate_addr(candidates: &[crate::protocol::ConnectionCandidate]) -> String {
    candidates
        .iter()
        .find(|c| c.candidate_type == "srflx")
        .or_else(|| candidates.iter().find(|c| c.candidate_type == "host"))
        .map(|c| c.addr.clone())
        .unwrap_or_default()
}

/// Determine connection type between two lobby players based on their
/// NAT types and STUN results.
fn determine_connection_type(
    a: &crate::lobby::LobbyPlayer,
    b: &crate::lobby::LobbyPlayer,
) -> (crate::lobby::ConnectionType, Option<String>) {
    use crate::lobby::ConnectionType;

    let a_nat = a.nat_type.as_deref().unwrap_or("unknown");
    let b_nat = b.nat_type.as_deref().unwrap_or("unknown");

    // Extract srflx (server-reflexive) addrs from candidates for LAN check
    let a_srflx = a.candidates.iter().find(|c| c.candidate_type == "srflx");
    let b_srflx = b.candidates.iter().find(|c| c.candidate_type == "srflx");

    // Check for same public IP (likely LAN)
    if let (Some(ac), Some(bc)) = (a_srflx, b_srflx) {
        let a_ip = ac.addr.split(':').next().unwrap_or("");
        let b_ip = bc.addr.split(':').next().unwrap_or("");
        if !a_ip.is_empty() && a_ip == b_ip {
            return (
                ConnectionType::DirectLan,
                Some("same public IP detected".into()),
            );
        }
    }

    // Check for UPnP-mapped candidates (highest priority after LAN)
    let a_upnp = a.candidates.iter().any(|c| c.candidate_type == "upnp");
    let b_upnp = b.candidates.iter().any(|c| c.candidate_type == "upnp");
    if a_upnp || b_upnp {
        return (
            ConnectionType::DirectUpnp,
            Some("UPnP/PCP mapped port available".into()),
        );
    }

    // Both symmetric NAT -> check for predicted candidates, else relay
    if a_nat == "symmetric" && b_nat == "symmetric" {
        let a_predicted = a.candidates.iter().any(|c| c.candidate_type == "predicted");
        let b_predicted = b.candidates.iter().any(|c| c.candidate_type == "predicted");
        if a_predicted || b_predicted {
            return (
                ConnectionType::PredictedHolepunch,
                Some("symmetric NAT with predicted port candidates".into()),
            );
        }
        return (
            ConnectionType::Relay,
            Some("both symmetric NAT, no predicted candidates".into()),
        );
    }

    // One symmetric + one cone -> holepunch can work
    if a_nat == "symmetric" || b_nat == "symmetric" {
        let sym_has_predicted = if a_nat == "symmetric" {
            a.candidates.iter().any(|c| c.candidate_type == "predicted")
        } else {
            b.candidates.iter().any(|c| c.candidate_type == "predicted")
        };
        if sym_has_predicted {
            return (
                ConnectionType::PredictedHolepunch,
                Some("symmetric+cone with predicted ports".into()),
            );
        }
        return (
            ConnectionType::DirectHolepunch,
            Some("symmetric+cone, cone side initiates".into()),
        );
    }

    // Both cone NATs or both have srflx candidates
    if a_srflx.is_some() && b_srflx.is_some() {
        return (
            ConnectionType::DirectHolepunch,
            Some("both peers cone NAT".into()),
        );
    }

    // Not enough info
    (ConnectionType::Unknown, None)
}

/// Build CONNECTION_INFO messages for each player in a lobby, describing
/// how they are connected to each of their peers.
fn build_connection_info_for_lobby(
    lobby: &crate::lobby::Lobby,
    _state: &ServerState,
) -> Vec<(Uuid, ServerMessage)> {
    let mut result = Vec::new();
    for player in &lobby.players {
        let connections: Vec<PeerConnectionInfo> = lobby
            .players
            .iter()
            .filter(|p| p.player_id != player.player_id)
            .map(|peer| {
                let (conn_type, detail) = determine_connection_type(player, peer);
                PeerConnectionInfo {
                    peer_id: peer.player_id,
                    peer_callsign: peer.callsign.clone(),
                    method: conn_type.as_str().to_string(),
                    detail,
                    server_relay: conn_type.is_server_relayed(),
                    estimated_latency_ms: None, // populated once relay is active
                }
            })
            .collect();

        result.push((
            player.player_id,
            ServerMessage::ConnectionInfo { connections },
        ));
    }
    result
}

/// Send CONNECTION_INFO to all players in a lobby via their sessions.
fn send_connection_info(lobby: &crate::lobby::Lobby, state: &ServerState) {
    let messages = build_connection_info_for_lobby(lobby, state);
    for (pid, msg) in messages {
        if let Some(session) = state.sessions.get(&pid) {
            let _ = session.tx.send(msg);
        }
    }
}

/// Generate predicted port candidates for symmetric NATs with sequential allocation.
///
/// If a player has 2+ srflx candidates from the same IP with ports that differ by
/// a small delta, the NAT likely allocates ports sequentially. We predict the next
/// few ports and add them as "predicted" candidates.
fn generate_predicted_candidates(
    candidates: &[ConnectionCandidate],
    nat_type: &str,
) -> Vec<ConnectionCandidate> {
    if !nat_type.contains("symmetric") {
        return Vec::new();
    }
    // Already has predicted candidates (client-generated)
    if candidates.iter().any(|c| c.candidate_type == "predicted") {
        return Vec::new();
    }
    // Collect srflx candidates, extract IP and port
    let mut srflx_addrs: Vec<(&str, u16)> = Vec::new();
    for c in candidates.iter().filter(|c| c.candidate_type == "srflx") {
        if let Some((ip, port_str)) = c.addr.rsplit_once(':') {
            if let Ok(port) = port_str.parse::<u16>() {
                srflx_addrs.push((ip, port));
            }
        }
    }
    if srflx_addrs.len() < 2 {
        return Vec::new();
    }
    // All srflx must share the same IP
    let ip = srflx_addrs[0].0;
    if !srflx_addrs.iter().all(|&(a, _)| a == ip) {
        return Vec::new();
    }
    // Sort by port and compute deltas
    let mut ports: Vec<u16> = srflx_addrs.iter().map(|&(_, p)| p).collect();
    ports.sort();
    let delta = ports[ports.len() - 1] as i32 - ports[0] as i32;
    let step = delta / (ports.len() as i32 - 1);
    // Only predict if delta per step is small and positive (sequential)
    if !(1..=10).contains(&step) {
        return Vec::new();
    }
    let last_port = ports[ports.len() - 1] as i32;
    let mut predicted = Vec::new();
    for i in 1..=2 {
        let p = last_port + step * i;
        if p > 0 && p <= 65535 {
            predicted.push(ConnectionCandidate {
                candidate_type: "predicted".into(),
                addr: format!("{ip}:{p}"),
            });
        }
    }
    if !predicted.is_empty() {
        debug!(
            count = predicted.len(),
            step, last_port, "generated predicted port candidates"
        );
    }
    predicted
}

/// Compute connection-check priority for a candidate pair.
fn candidate_pair_priority(local_type: &str, remote_type: &str) -> u32 {
    match (local_type, remote_type) {
        ("host", "host") => 100,
        ("upnp", _) | (_, "upnp") => 90,
        ("srflx", "srflx") => 80,
        ("srflx", "host") | ("host", "srflx") => 75,
        ("predicted", "predicted") => 60,
        ("predicted", "srflx") | ("srflx", "predicted") => 50,
        ("predicted", _) | (_, "predicted") => 40,
        _ => 10,
    }
}

/// Build per-player CONNECTIVITY_CHECK_GO messages with prioritized candidate pairs.
fn build_connectivity_check_messages(lobby: &Lobby) -> Vec<(Uuid, ServerMessage)> {
    let mut result = Vec::new();
    for player in &lobby.players {
        let mut pairs: Vec<CandidatePair> = Vec::new();
        for peer in &lobby.players {
            if peer.player_id == player.player_id {
                continue;
            }
            for local_cand in &player.candidates {
                for remote_cand in &peer.candidates {
                    pairs.push(CandidatePair {
                        peer_id: peer.player_id,
                        local_type: local_cand.candidate_type.clone(),
                        remote_type: remote_cand.candidate_type.clone(),
                        remote_addr: remote_cand.addr.clone(),
                        priority: candidate_pair_priority(
                            &local_cand.candidate_type,
                            &remote_cand.candidate_type,
                        ),
                    });
                }
            }
        }
        pairs.sort_by_key(|p| std::cmp::Reverse(p.priority));
        result.push((
            player.player_id,
            ServerMessage::ConnectivityCheckGo { peer_addrs: pairs },
        ));
    }
    result
}

struct RelayPair {
    slot_a: u8,
    slot_b: u8,
    pid_a: Uuid,
    pid_b: Uuid,
    addr_a: Option<String>,
    addr_b: Option<String>,
}

/// Allocate a relay session for a player pair and send RELAY_ASSIGNED to both.
/// Returns None if relay is not configured or the server-wide relay limit is reached.
fn allocate_relay_session(state: &ServerState, relay_addr: &str, pair: &RelayPair) -> Option<u32> {
    if relay_addr.is_empty() {
        return None;
    }
    let max = state.config.max_relay_sessions;
    if max > 0 && state.relay_sessions.len() >= max {
        warn!(
            max_relay_sessions = max,
            current = state.relay_sessions.len(),
            "relay session limit reached, rejecting allocation"
        );
        return None;
    }
    let token = {
        let mut t = Uuid::new_v4().as_u128() as u32;
        // D11: retry on collision (birthday problem with 32-bit truncation)
        let mut attempts = 0;
        while state.relay_sessions.contains_key(&t) && attempts < 10 {
            t = Uuid::new_v4().as_u128() as u32;
            attempts += 1;
        }
        if state.relay_sessions.contains_key(&t) {
            warn!("relay token collision after 10 retries");
            return None;
        }
        t
    };
    let player_addrs = dashmap::DashMap::new();
    // D12: pre-register allowed IPs from known peer addresses
    let allowed_ips = dashmap::DashSet::new();
    if let Some(ref a) = pair.addr_a {
        if let Ok(sa) = a.parse::<SocketAddr>() {
            player_addrs.insert(pair.slot_a, sa);
            allowed_ips.insert(sa.ip());
        }
    }
    if let Some(ref b) = pair.addr_b {
        if let Ok(sa) = b.parse::<SocketAddr>() {
            player_addrs.insert(pair.slot_b, sa);
            allowed_ips.insert(sa.ip());
        }
    }
    state.relay_sessions.insert(
        token,
        crate::relay::RelaySession {
            session_token: token,
            player_addrs,
            expected_players: 2,
            allowed_ips,
            created_at: std::time::Instant::now(),
        },
    );
    info!(token, pid_a = %pair.pid_a, pid_b = %pair.pid_b, "relay session allocated");
    Some(token)
}

/// Register session and send welcome bundle (AUTH_OK, MOTD, status, lobby list, friends).
/// Shared by both GPGS and keypair auth paths.
#[allow(clippy::too_many_arguments)]
fn complete_auth(
    state: &Arc<ServerState>,
    tx: &BoundedSender,
    pid: Uuid,
    callsign: &str,
    platform: &str,
    gpgs_verified: bool,
    auth_method: &str,
    session_id: Uuid,
    addr: SocketAddr,
) {
    state.sessions.insert(
        pid,
        PlayerSession {
            player_id: pid,
            callsign: callsign.to_string(),
            presence: Presence::Online,
            tx: tx.clone(),
            lobby_id: None,
            ping_ms: None,
            gpgs_verified,
            auth_method: auth_method.to_string(),
        },
    );
    state.stats.player_connected();

    // Add client IP to STUN allowlist (ref-counted for shared NAT IPs, D17 fix)
    state
        .stun_allowlist
        .entry(addr.ip())
        .and_modify(|c| *c += 1)
        .or_insert(1);

    info!(%pid, %callsign, %platform, %auth_method, "player authenticated");

    let stun_addrs: Vec<String> = if state.config.stun_public_addrs.is_empty() {
        vec![]
    } else {
        state
            .config
            .stun_public_addrs
            .split(',')
            .map(|s| s.trim().to_string())
            .filter(|s| !s.is_empty())
            .collect()
    };

    let _ = tx.send(ServerMessage::AuthOk {
        player_id: pid,
        session_token: session_id.to_string(),
        stun_addrs,
    });

    if !state.config.motd.is_empty() {
        let _ = tx.send(ServerMessage::Motd {
            message: state.config.motd.clone(),
            url: None,
            severity: "info".into(),
        });
    }

    let snap = state.stats.snapshot();
    let total_played = state.db.total_games_played().unwrap_or(0);
    let active_game_list = build_active_game_list(state);
    let _ = tx.send(ServerMessage::ServerStatus {
        online_players: snap.online,
        active_games_count: snap.in_game,
        active_game_list,
        total_games_played: total_played,
    });

    let lobbies = build_lobby_list(state);
    let _ = tx.send(ServerMessage::LobbyList { lobbies });

    let friend_list = friends::build_friend_list(state, &pid);
    let _ = tx.send(friend_list);

    let _ = state.db.log_connection_event(
        Some(&pid),
        "connect",
        Some(&format!("platform={platform},auth={auth_method}")),
    );
}

async fn handle_authenticated_message(
    state: &Arc<ServerState>,
    player_id: Uuid,
    msg: ClientMessage,
    tx: &BoundedSender,
) {
    match msg {
        ClientMessage::CreateLobby {
            game,
            mission,
            mode,
            max_players,
            lobby_code,
            verified_only,
        } => {
            // Validate lobby parameters
            if !(2..=8).contains(&max_players) {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "max_players must be 2-8".into(),
                });
                return;
            }
            if game.len() > 32 || mission.len() > 128 || mode.len() > 32 {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "game/mission/mode string too long".into(),
                });
                return;
            }
            if !state.rate_limiter.check_lobby_create(player_id) {
                warn!(%player_id, "lobby create rate-limited");
                let _ = tx.send(ServerMessage::RateLimited {
                    retry_after_ms: 10_000,
                });
                return;
            }

            let callsign = state
                .sessions
                .get(&player_id)
                .map(|s| s.callsign.clone())
                .unwrap_or_default();

            let lobby = Lobby::new(
                player_id,
                callsign,
                game,
                mission,
                mode,
                max_players,
                lobby_code,
                verified_only,
            );
            let lobby_id = lobby.id;
            state.lobbies.insert(lobby_id, lobby);
            state.stats.lobby_created();

            // Update session
            if let Some(mut session) = state.sessions.get_mut(&player_id) {
                session.lobby_id = Some(lobby_id);
                session.presence = Presence::InLobby { lobby_id };
            }

            info!(%player_id, %lobby_id, "lobby created");
            if let Some(lobby) = state.lobbies.get(&lobby_id) {
                broadcast_lobby_update(&lobby, state);
            }
        }

        ClientMessage::ListLobbies {} => {
            if !state.rate_limiter.check_lobby_list(player_id) {
                let _ = tx.send(ServerMessage::RateLimited {
                    retry_after_ms: 10_000,
                });
            } else {
                let lobbies = build_lobby_list(state);
                let _ = tx.send(ServerMessage::LobbyList { lobbies });
            }
        }

        ClientMessage::JoinLobby {
            lobby_id,
            lobby_code,
        } => {
            if !state.rate_limiter.check_lobby_join(player_id) {
                warn!(%player_id, "lobby join rate-limited");
                let _ = tx.send(ServerMessage::RateLimited {
                    retry_after_ms: 60_000,
                });
                return;
            }

            // D13: implicitly leave old lobby before joining a new one
            let old_lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(old_lid) = old_lobby_id {
                if old_lid != lobby_id {
                    if let Some(mut old_lobby) = state.lobbies.get_mut(&old_lid) {
                        old_lobby.remove_player(&player_id);
                        if old_lobby.players.is_empty() {
                            drop(old_lobby);
                            state.lobbies.remove(&old_lid);
                            state.stats.lobby_closed();
                        } else {
                            broadcast_lobby_update(&old_lobby, state);
                        }
                    }
                    if let Some(mut session) = state.sessions.get_mut(&player_id) {
                        session.lobby_id = None;
                        session.presence = Presence::Online;
                    }
                    info!(%player_id, lobby_id = %old_lid, "implicitly left old lobby");
                }
            }

            // Check kicked-player and lobby-code restrictions before joining
            if let Some(lobby) = state.lobbies.get(&lobby_id) {
                if lobby.kicked_players.contains(&player_id) {
                    let _ = tx.send(ServerMessage::Error {
                        code: "KICKED_FROM_LOBBY".into(),
                        message: "You were kicked from this lobby".into(),
                    });
                    return;
                }
                if let Some(ref code) = lobby.code {
                    if lobby_code.as_deref() != Some(code.as_str()) {
                        let _ = tx.send(ServerMessage::Error {
                            code: "LOBBY_CODE_REQUIRED".into(),
                            message: "Incorrect or missing lobby code".into(),
                        });
                        return;
                    }
                }
                if lobby.verified_only {
                    let is_verified = state
                        .sessions
                        .get(&player_id)
                        .map(|s| s.gpgs_verified)
                        .unwrap_or(false);
                    if !is_verified {
                        let _ = tx.send(ServerMessage::Error {
                            code: "VERIFIED_ONLY".into(),
                            message: "This lobby requires a verified Google Play Games account"
                                .into(),
                        });
                        return;
                    }
                }
            }

            let callsign = state
                .sessions
                .get(&player_id)
                .map(|s| s.callsign.clone())
                .unwrap_or_default();

            let result = if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                lobby.add_player(player_id, callsign)
            } else {
                false
            };

            if result {
                if let Some(mut session) = state.sessions.get_mut(&player_id) {
                    session.lobby_id = Some(lobby_id);
                    session.presence = Presence::InLobby { lobby_id };
                }
                info!(%player_id, %lobby_id, "joined lobby");
                if let Some(lobby) = state.lobbies.get(&lobby_id) {
                    broadcast_lobby_update(&lobby, state);
                }
            } else {
                let _ = tx.send(ServerMessage::Error {
                    code: "JOIN_FAILED".into(),
                    message: "Could not join lobby (full or not available)".into(),
                });
            }
        }

        ClientMessage::LeaveLobby {} => {
            let lobby_id = if let Some(mut session) = state.sessions.get_mut(&player_id) {
                if let Some(lid) = session.lobby_id.take() {
                    session.presence = Presence::Online;
                    Some(lid)
                } else {
                    None
                }
            } else {
                None
            };
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    lobby.remove_player(&player_id);
                    if lobby.players.is_empty() {
                        drop(lobby);
                        state.lobbies.remove(&lobby_id);
                        state.stats.lobby_closed();
                    } else {
                        broadcast_lobby_update(&lobby, state);
                    }
                }
                info!(%player_id, %lobby_id, "left lobby");
            }
        }

        ClientMessage::Ready { ready } => {
            if let Some(session) = state.sessions.get(&player_id) {
                if let Some(lobby_id) = session.lobby_id {
                    if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                        if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id)
                        {
                            p.ready = ready;
                        }
                        broadcast_lobby_update(&lobby, state);
                    }
                }
            }
        }

        ClientMessage::StartGame {} => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    if lobby.host_player_id != player_id {
                        warn!(%player_id, %lobby_id, "non-host tried to start game");
                        let _ = tx.send(ServerMessage::Error {
                            code: "NOT_HOST".into(),
                            message: "Only the host can start the game".into(),
                        });
                        return;
                    }
                    lobby.state = LobbyState::Starting;
                    state.stats.game_started();
                    info!(%lobby_id, "game starting");

                    // Send connection type info to all players
                    send_connection_info(&lobby, state);

                    // Determine host address from candidates (prefer srflx, then host)
                    let host_addr = lobby
                        .players
                        .iter()
                        .find(|p| p.player_id == player_id)
                        .and_then(|p| {
                            p.candidates
                                .iter()
                                .find(|c| c.candidate_type == "srflx")
                                .or_else(|| {
                                    p.candidates.iter().find(|c| c.candidate_type == "host")
                                })
                        })
                        .map(|c| c.addr.clone())
                        .unwrap_or_default();

                    let game = lobby.game.clone();
                    let mission = lobby.mission.clone();
                    let mode = lobby.mode.clone();
                    let max_players = lobby.max_players;

                    // Collect relay-needed pairs and allocate sessions BEFORE
                    // building GAME_STARTING so relay info is available.
                    // relay_tokens maps (slot_a, slot_b) -> token
                    let relay_addr = state.config.relay_public_addr.clone();
                    let mut relay_tokens: std::collections::HashMap<(u8, u8), u32> =
                        std::collections::HashMap::new();
                    let mut relay_limit_hit = false;
                    for i in 0..lobby.players.len() {
                        for j in (i + 1)..lobby.players.len() {
                            let (conn_type, _) =
                                determine_connection_type(&lobby.players[i], &lobby.players[j]);
                            if conn_type == crate::lobby::ConnectionType::Relay {
                                let addr_a = lobby.players[i]
                                    .candidates
                                    .iter()
                                    .find(|c| c.candidate_type == "srflx")
                                    .map(|c| c.addr.clone());
                                let addr_b = lobby.players[j]
                                    .candidates
                                    .iter()
                                    .find(|c| c.candidate_type == "srflx")
                                    .map(|c| c.addr.clone());
                                let pair = RelayPair {
                                    slot_a: i as u8,
                                    slot_b: j as u8,
                                    pid_a: lobby.players[i].player_id,
                                    pid_b: lobby.players[j].player_id,
                                    addr_a,
                                    addr_b,
                                };
                                match allocate_relay_session(state, &relay_addr, &pair) {
                                    Some(token) => {
                                        relay_tokens.insert((i as u8, j as u8), token);
                                    }
                                    None if !relay_addr.is_empty() => {
                                        relay_limit_hit = true;
                                    }
                                    None => {}
                                }
                            }
                        }
                    }

                    // If relay limit was hit, abort game start and notify players
                    if relay_limit_hit {
                        lobby.state = LobbyState::Waiting;
                        state.stats.game_ended(); // undo the game_started() above
                        warn!(%lobby_id, "game start aborted: relay session limit reached");
                        // Clean up any relay sessions we did allocate for this game
                        for token in relay_tokens.values() {
                            state.relay_sessions.remove(token);
                        }
                        let err_msg = ServerMessage::Error {
                            code: "RELAY_LIMIT_REACHED".into(),
                            message: "Server relay capacity reached. Try again later or use direct connections.".into(),
                        };
                        for p in lobby.players.iter() {
                            if let Some(sess) = state.sessions.get(&p.player_id) {
                                let _ = sess.tx.send(err_msg.clone());
                            }
                        }
                        drop(lobby);
                        return;
                    }

                    // Build per-player GAME_STARTING with peer assignments
                    let player_count = lobby.players.len();
                    let mut per_player_msgs: Vec<(Uuid, ServerMessage)> =
                        Vec::with_capacity(player_count);

                    for my_slot in 0..player_count {
                        let my_pid = lobby.players[my_slot].player_id;
                        let mut peers = Vec::with_capacity(player_count - 1);

                        for other_slot in 0..player_count {
                            if other_slot == my_slot {
                                continue;
                            }
                            let other = &lobby.players[other_slot];
                            let (conn_type, _) = determine_connection_type(
                                &lobby.players[my_slot],
                                &lobby.players[other_slot],
                            );
                            let is_relay = conn_type == crate::lobby::ConnectionType::Relay;

                            // For direct: use winning candidate addr; for relay: use relay server
                            let addr = if is_relay {
                                relay_addr.clone()
                            } else {
                                best_candidate_addr(&other.candidates)
                            };

                            // Look up relay token for this pair
                            let (lo, hi) = if my_slot < other_slot {
                                (my_slot as u8, other_slot as u8)
                            } else {
                                (other_slot as u8, my_slot as u8)
                            };
                            let relay_token = if is_relay {
                                relay_tokens.get(&(lo, hi)).copied()
                            } else {
                                None
                            };
                            let relay_dest_slot = if is_relay {
                                Some(other_slot as u8)
                            } else {
                                None
                            };

                            peers.push(crate::protocol::PeerAssignment {
                                slot: other_slot as u8,
                                addr,
                                is_relay,
                                relay_token,
                                relay_dest_slot,
                            });
                        }

                        per_player_msgs.push((
                            my_pid,
                            ServerMessage::GameStarting {
                                host_addr: host_addr.clone(),
                                game: game.clone(),
                                mission: mission.clone(),
                                mode: mode.clone(),
                                your_slot: my_slot as u8,
                                max_players,
                                difficulty: 1, // default; lobby doesn't track this yet
                                level_num: 1,
                                peers,
                            },
                        ));
                    }

                    // Update presence for all players
                    let player_ids: Vec<Uuid> = lobby.players.iter().map(|p| p.player_id).collect();
                    let lobby_mission = lobby.mission.clone();
                    let pcount = lobby.player_count();
                    drop(lobby);

                    for pid in &player_ids {
                        if let Some(mut session) = state.sessions.get_mut(pid) {
                            session.presence = Presence::InGame {
                                lobby_id,
                                mission: lobby_mission.clone(),
                                player_count: pcount,
                            };
                        }
                    }

                    // Send per-player GAME_STARTING messages
                    for (pid, msg) in per_player_msgs {
                        if let Some(sess) = state.sessions.get(&pid) {
                            let _ = sess.tx.send(msg);
                        }
                    }
                }
            }
        }

        ClientMessage::StunResult {
            candidates,
            nat_type,
        } => {
            info!(%player_id, %nat_type, candidate_count = candidates.len(), "STUN result received");
            if let Some(session) = state.sessions.get(&player_id) {
                if let Some(lobby_id) = session.lobby_id {
                    if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                        // Generate predicted port candidates for symmetric NATs
                        let predicted = generate_predicted_candidates(&candidates, &nat_type);
                        let mut all_candidates = candidates.clone();
                        all_candidates.extend(predicted);

                        if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id)
                        {
                            p.candidates = all_candidates.clone();
                            p.nat_type = Some(nat_type);
                        }
                        // Distribute sender's candidates to all other players in the lobby
                        let sender_nat = lobby
                            .players
                            .iter()
                            .find(|p| p.player_id == player_id)
                            .and_then(|p| p.nat_type.clone())
                            .unwrap_or_default();
                        let peer_msgs: Vec<(Uuid, ServerMessage)> = lobby
                            .players
                            .iter()
                            .filter(|p| p.player_id != player_id)
                            .map(|p| {
                                (
                                    p.player_id,
                                    ServerMessage::PeerCandidates {
                                        peer_id: player_id,
                                        candidates: all_candidates.clone(),
                                        nat_type: sender_nat.clone(),
                                    },
                                )
                            })
                            .collect();

                        // Check if ALL players have submitted STUN results
                        let all_have_candidates = lobby.players.len() > 1
                            && lobby.players.iter().all(|p| !p.candidates.is_empty())
                            && lobby.state == LobbyState::Waiting;
                        let check_msgs = if all_have_candidates {
                            lobby.state = LobbyState::Holepunching;
                            lobby.holepunch_started_at = Some(std::time::Instant::now());
                            Some(build_connectivity_check_messages(&lobby))
                        } else {
                            None
                        };

                        drop(lobby);
                        for (pid, msg) in peer_msgs {
                            if let Some(sess) = state.sessions.get(&pid) {
                                let _ = sess.tx.send(msg);
                            }
                        }
                        // Send CONNECTIVITY_CHECK_GO to all players simultaneously
                        if let Some(msgs) = check_msgs {
                            for (pid, msg) in msgs {
                                if let Some(sess) = state.sessions.get(&pid) {
                                    let _ = sess.tx.send(msg);
                                }
                            }
                            info!(%lobby_id, "connectivity checks started (all STUN results received)");
                        }
                    }
                }
            }
        }

        ClientMessage::ConnectivityOk {
            peer_id: peer,
            winning_candidate_type,
            rtt_ms,
        } => {
            // Record connectivity result and update connection_type for the pair
            if let Some(session) = state.sessions.get(&player_id) {
                if let Some(lobby_id) = session.lobby_id {
                    if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                        let conn_type = match winning_candidate_type.as_str() {
                            "host" => crate::lobby::ConnectionType::DirectLan,
                            "upnp" => crate::lobby::ConnectionType::DirectUpnp,
                            "srflx" => crate::lobby::ConnectionType::DirectHolepunch,
                            "predicted" => crate::lobby::ConnectionType::PredictedHolepunch,
                            "relay" => crate::lobby::ConnectionType::Relay,
                            _ => crate::lobby::ConnectionType::Unknown,
                        };
                        if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id)
                        {
                            p.connection_type = conn_type;
                            p.ping_ms = Some(rtt_ms);
                        }
                        info!(%player_id, %peer, %winning_candidate_type, rtt_ms, "connectivity ok");
                    }
                }
            }
        }

        ClientMessage::ConnectivityUpdate {
            peer_id: peer,
            new_method,
            detail,
        } => {
            // Update connection_type for an existing peer pair mid-session
            if let Some(session) = state.sessions.get(&player_id) {
                if let Some(lobby_id) = session.lobby_id {
                    if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                        let conn_type = match new_method.as_str() {
                            "direct_lan" => crate::lobby::ConnectionType::DirectLan,
                            "direct_upnp" => crate::lobby::ConnectionType::DirectUpnp,
                            "direct_holepunch" => crate::lobby::ConnectionType::DirectHolepunch,
                            "predicted_holepunch" => {
                                crate::lobby::ConnectionType::PredictedHolepunch
                            }
                            "relay" => crate::lobby::ConnectionType::Relay,
                            _ => crate::lobby::ConnectionType::Unknown,
                        };
                        if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id)
                        {
                            p.connection_type = conn_type;
                        }
                        info!(%player_id, %peer, %new_method, ?detail, "connectivity update");
                        // Refresh CONNECTION_INFO for all players
                        send_connection_info(&lobby, state);
                    }
                }
            }
        }

        ClientMessage::FileHashes { hashes } => {
            // Compare against host's hashes and report mismatches
            if let Some(session) = state.sessions.get(&player_id) {
                if let Some(lobby_id) = session.lobby_id {
                    // Store this player's hashes, then compare with host's
                    if let Some(lobby) = state.lobbies.get(&lobby_id) {
                        let host_id = lobby.host_player_id;
                        if player_id == host_id {
                            // Host sent hashes -- nothing to compare against self
                            return;
                        }
                    }
                    // For now, forward hashes to host for comparison
                    if let Some(lobby) = state.lobbies.get(&lobby_id) {
                        let host_id = lobby.host_player_id;
                        drop(lobby);
                        if let Some(host_session) = state.sessions.get(&host_id) {
                            let callsign = state
                                .sessions
                                .get(&player_id)
                                .map(|s| s.callsign.clone())
                                .unwrap_or_default();
                            // Send as a lobby update with file info
                            // The host client handles the actual hash comparison
                            let _ = host_session.tx.send(ServerMessage::Error {
                                code: "FILE_HASHES_RECEIVED".into(),
                                message: serde_json::json!({
                                    "player_id": player_id,
                                    "callsign": callsign,
                                    "hashes": hashes
                                })
                                .to_string(),
                            });
                        }
                    }
                }
            }
        }

        ClientMessage::KickPlayer { player_id: target } => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                let is_host = state
                    .lobbies
                    .get(&lobby_id)
                    .map(|l| l.host_player_id == player_id)
                    .unwrap_or(false);
                if is_host {
                    if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                        // Cap kicked_players to prevent unbounded growth (D16 fix)
                        if lobby.kicked_players.len() < 100 {
                            lobby.kicked_players.insert(target);
                        }
                        lobby.remove_player(&target);
                        info!(%player_id, kicked = %target, %lobby_id, "player kicked");
                        broadcast_lobby_update(&lobby, state);
                    }
                    // Notify kicked player and update their session
                    if let Some(kicked_session) = state.sessions.get(&target) {
                        let _ = kicked_session.tx.send(ServerMessage::Error {
                            code: "KICKED".into(),
                            message: "You have been kicked from the lobby".into(),
                        });
                    }
                    if let Some(mut kicked_session) = state.sessions.get_mut(&target) {
                        kicked_session.lobby_id = None;
                        kicked_session.presence = Presence::Online;
                    }
                }
            }
        }

        ClientMessage::MatchResult {
            lobby_id,
            duration_secs,
            result,
            players,
        } => {
            // Validate that this player was the host
            let is_host = state
                .lobbies
                .get(&lobby_id)
                .map(|l| l.host_player_id == player_id)
                .unwrap_or(false);

            if !is_host {
                let _ = tx.send(ServerMessage::Error {
                    code: "NOT_HOST".into(),
                    message: "Only the host can report match results".into(),
                });
                return;
            }

            let match_id = Uuid::new_v4();
            let mode = state
                .lobbies
                .get(&lobby_id)
                .map(|l| l.mode.clone())
                .unwrap_or_default();
            let mission = state
                .lobbies
                .get(&lobby_id)
                .map(|l| l.mission.clone())
                .unwrap_or_default();

            let _ = state.db.insert_match_result(&MatchResultData {
                match_id,
                lobby_id,
                duration_secs,
                game_mode: mode.to_string(),
                mission,
                result,
                player_count: players.len() as u32,
            });

            for p in &players {
                let callsign = state
                    .sessions
                    .get(&p.player_id)
                    .map(|s| s.callsign.clone())
                    .unwrap_or_default();
                let _ = state.db.insert_match_player(&MatchPlayerData {
                    match_id,
                    player_id: p.player_id,
                    callsign,
                    score: p.score,
                    kills: p.kills,
                    deaths: p.deaths,
                    result: p.result.clone(),
                });
            }

            // Clean up the lobby
            state.lobbies.remove(&lobby_id);
            state.stats.lobby_closed();
            state.stats.game_ended();

            info!(%match_id, %lobby_id, "match result recorded");
            let _ = tx.send(ServerMessage::MatchResultAck { match_id });
        }

        // -- Friends --
        ClientMessage::FriendRequest { target_callsign } => {
            let callsign = state
                .sessions
                .get(&player_id)
                .map(|s| s.callsign.clone())
                .unwrap_or_default();
            let response =
                friends::handle_friend_request(state, player_id, &callsign, &target_callsign);

            // If it was successful (a FriendRequestReceived), that's actually the
            // notification for the TARGET. Send success to the requester.
            match &response {
                ServerMessage::FriendRequestReceived { .. } => {
                    // Notify target if online
                    let target_id = state
                        .db
                        .find_player_by_callsign(&target_callsign)
                        .ok()
                        .flatten();
                    if let Some(tid) = target_id {
                        if let Some(target_session) = state.sessions.get(&tid) {
                            let _ = target_session.tx.send(response);
                        }
                    }
                    // Ack to requester (empty error = success, but we should define a proper ack)
                    // For now, no explicit ack -- the request succeeded silently
                }
                _ => {
                    let _ = tx.send(response);
                }
            }
        }

        ClientMessage::FriendAccept {
            player_id: friend_id,
        } => {
            let _ = state.db.accept_friend(&player_id, &friend_id);
            info!(%player_id, %friend_id, "friend request accepted");
            // Notify the friend if online
            if let Some(friend_session) = state.sessions.get(&friend_id) {
                let _ = friend_session
                    .tx
                    .send(ServerMessage::FriendAccepted { player_id });
            }
        }

        ClientMessage::FriendRemove {
            player_id: friend_id,
        } => {
            let _ = state.db.remove_friend(&player_id, &friend_id);
            info!(%player_id, %friend_id, "friend removed");
            if let Some(friend_session) = state.sessions.get(&friend_id) {
                let _ = friend_session
                    .tx
                    .send(ServerMessage::FriendRemoved { player_id });
            }
        }

        ClientMessage::FriendBlock {
            player_id: blocked_id,
        } => {
            let _ = state.db.block_player(&player_id, &blocked_id);
            info!(%player_id, %blocked_id, "player blocked");
        }

        ClientMessage::FriendList {} => {
            let response = friends::build_friend_list(state, &player_id);
            let _ = tx.send(response);
        }

        ClientMessage::JoinFriendGame { friend_player_id } => {
            // Look up the friend's current game
            let game_info =
                state
                    .sessions
                    .get(&friend_player_id)
                    .and_then(|session| match &session.presence {
                        Presence::InGame { lobby_id, .. } | Presence::InLobby { lobby_id } => {
                            Some(*lobby_id)
                        }
                        _ => None,
                    });

            match game_info {
                Some(lobby_id) => {
                    // Check if player was kicked from this lobby
                    let was_kicked = state
                        .lobbies
                        .get(&lobby_id)
                        .map(|l| l.kicked_players.contains(&player_id))
                        .unwrap_or(false);
                    if was_kicked {
                        let _ = tx.send(ServerMessage::JoinFriendGameResp {
                            success: false,
                            reason: Some("You were kicked from this lobby".into()),
                            lobby_id: None,
                        });
                        return;
                    }

                    // Check verified_only restriction (applies to friend-join too)
                    let is_verified_only = state
                        .lobbies
                        .get(&lobby_id)
                        .map(|l| l.verified_only)
                        .unwrap_or(false);
                    if is_verified_only {
                        let is_verified = state
                            .sessions
                            .get(&player_id)
                            .map(|s| s.gpgs_verified)
                            .unwrap_or(false);
                        if !is_verified {
                            let _ = tx.send(ServerMessage::JoinFriendGameResp {
                                success: false,
                                reason: Some(
                                    "This lobby requires a verified Google Play Games account"
                                        .into(),
                                ),
                                lobby_id: None,
                            });
                            return;
                        }
                    }

                    // Friends bypass lobby code requirement
                    let joinable = state
                        .lobbies
                        .get(&lobby_id)
                        .map(|l| l.is_joinable())
                        .unwrap_or(false);
                    if joinable {
                        // Perform the join (reuse join logic)
                        let callsign = state
                            .sessions
                            .get(&player_id)
                            .map(|s| s.callsign.clone())
                            .unwrap_or_default();
                        let joined = state
                            .lobbies
                            .get_mut(&lobby_id)
                            .map(|mut l| l.add_player(player_id, callsign))
                            .unwrap_or(false);
                        if joined {
                            if let Some(mut session) = state.sessions.get_mut(&player_id) {
                                session.lobby_id = Some(lobby_id);
                                session.presence = Presence::InLobby { lobby_id };
                            }
                            let _ = tx.send(ServerMessage::JoinFriendGameResp {
                                success: true,
                                reason: None,
                                lobby_id: Some(lobby_id),
                            });
                            if let Some(lobby) = state.lobbies.get(&lobby_id) {
                                broadcast_lobby_update(&lobby, state);
                            }
                        } else {
                            let _ = tx.send(ServerMessage::JoinFriendGameResp {
                                success: false,
                                reason: Some("Could not join game (full or locked)".into()),
                                lobby_id: None,
                            });
                        }
                    } else {
                        let _ = tx.send(ServerMessage::JoinFriendGameResp {
                            success: false,
                            reason: Some("Game is not joinable".into()),
                            lobby_id: None,
                        });
                    }
                }
                None => {
                    let _ = tx.send(ServerMessage::JoinFriendGameResp {
                        success: false,
                        reason: Some("Friend is not in a game".into()),
                        lobby_id: None,
                    });
                }
            }
        }

        ClientMessage::SendMessage {
            target_player_id,
            text,
        } => {
            // Rate limit
            if !state.rate_limiter.check_player_message(player_id) {
                warn!(%player_id, "player message rate-limited");
                let _ = tx.send(ServerMessage::RateLimited {
                    retry_after_ms: 60_000,
                });
                return;
            }

            // Validate text: max 200 chars, printable ASCII only
            if text.len() > 200 || text.chars().any(|c| !c.is_ascii_graphic() && c != ' ') {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_MESSAGE".into(),
                    message: "Message must be <= 200 printable ASCII characters".into(),
                });
                return;
            }

            // Check if the target has blocked the sender (silently drop)
            let blocked = state
                .db
                .is_blocked(&target_player_id, &player_id)
                .unwrap_or(false);
            if blocked {
                // Silently ack to sender so they don't know they're blocked
                let _ = tx.send(ServerMessage::MessageSent { target_player_id });
                return;
            }

            // Deliver to target if online
            let callsign = state
                .sessions
                .get(&player_id)
                .map(|s| s.callsign.clone())
                .unwrap_or_default();
            if let Some(target_session) = state.sessions.get(&target_player_id) {
                let _ = target_session.tx.send(ServerMessage::MessageReceived {
                    from_player_id: player_id,
                    from_callsign: callsign,
                    text,
                });
            }
            let _ = tx.send(ServerMessage::MessageSent { target_player_id });
        }

        // Already handled before authentication
        ClientMessage::Authenticate { .. } | ClientMessage::PowSolution { .. } => {}
    }
}
