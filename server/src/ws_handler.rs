use std::net::SocketAddr;
use std::sync::Arc;
use std::time::{Duration, Instant};

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
use crate::lobby::{game_info_str, Lobby, LobbyState, Presence};
use crate::pow;
use crate::protocol::*;
use crate::ServerState;

/// Wrapper around bounded mpsc::Sender that provides fire-and-forget send.
/// Retires a slow connection rather than silently losing state transitions.
#[derive(Debug)]
struct QueuedControl {
    text: String,
    _global: Option<tokio::sync::OwnedSemaphorePermit>,
    _connection: tokio::sync::OwnedSemaphorePermit,
}
#[derive(Clone)]
pub struct BoundedSender {
    tx: mpsc::Sender<QueuedControl>,
    retired: Arc<tokio::sync::Notify>,
    global_bytes: Arc<tokio::sync::Semaphore>,
    connection_bytes: Arc<tokio::sync::Semaphore>,
    critical_bytes: Arc<tokio::sync::Semaphore>,
}
impl std::fmt::Debug for BoundedSender {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("BoundedSender").finish_non_exhaustive()
    }
}
impl BoundedSender {
    #[allow(clippy::result_large_err)]
    pub fn send(&self, msg: ServerMessage) -> Result<(), mpsc::error::TrySendError<ServerMessage>> {
        let bulk = matches!(
            &msg,
            ServerMessage::LobbyList { .. }
                | ServerMessage::ServerStatus { .. }
                | ServerMessage::FriendListResp { .. }
        );
        let text = serde_json::to_string(&msg).unwrap_or_default();
        if text.len() > 1024 * 1024 {
            return self.send(ServerMessage::Error {
                code: "QUERY_TOO_LARGE".into(),
                message: "Response exceeds the supported query size".into(),
            });
        }
        let queued = (|| {
            let global = if bulk {
                Some(
                    Arc::clone(&self.global_bytes)
                        .try_acquire_many_owned(text.len() as u32)
                        .ok()?,
                )
            } else {
                None
            };
            let pool = if bulk {
                &self.connection_bytes
            } else {
                &self.critical_bytes
            };
            let connection = Arc::clone(pool)
                .try_acquire_many_owned(text.len() as u32)
                .ok()?;
            self.tx
                .try_send(QueuedControl {
                    text,
                    _global: global,
                    _connection: connection,
                })
                .ok()
        })();
        if queued.is_none() {
            if bulk {
                let _ = self.send(ServerMessage::RateLimited {
                    retry_after_ms: 1000,
                });
            } else {
                self.retired.notify_one();
            }
            return Err(mpsc::error::TrySendError::Full(msg));
        }
        Ok(())
    }
}

/// Build a LOBBY_UPDATE message from the current lobby state.
fn build_lobby_update(lobby: &Lobby) -> ServerMessage {
    let players = lobby
        .players
        .iter()
        .map(|p| LobbyPlayerInfo {
            slot: p.slot,
            player_id: p.player_id,
            callsign: p.callsign.clone(),
            ready: p.ready,
            ping_ms: p.ping_ms,
            connection_type: p.connection_type.as_str().to_string(),
            mission_status: p.mission_status.clone(),
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

fn mission_requirement(game_info: &serde_json::Value) -> Option<(&str, u64)> {
    let requirement = game_info.get("mission_requirement")?.as_object()?;
    let revision = requirement.get("revision")?.as_str()?;
    if revision.is_empty() || revision.len() > 160 {
        return None;
    }
    let size = requirement
        .get("size_bytes")
        .and_then(serde_json::Value::as_u64)
        .unwrap_or(0);
    Some((revision, size))
}

fn validate_mission_requirement(game_info: &serde_json::Value) -> Result<(), &'static str> {
    let Some(value) = game_info.get("mission_requirement") else {
        return Ok(());
    };
    let requirement = value
        .as_object()
        .ok_or("mission_requirement must be an object")?;
    if requirement
        .get("schema")
        .and_then(serde_json::Value::as_u64)
        != Some(1)
    {
        return Err("unsupported mission requirement schema");
    }
    let bounded_string =
        |name: &str, maximum: usize, allow_empty: bool| -> Result<&str, &'static str> {
            let value = requirement
                .get(name)
                .and_then(serde_json::Value::as_str)
                .ok_or("mission requirement has a missing string")?;
            if value.len() > maximum || (!allow_empty && value.is_empty()) {
                return Err("mission requirement string is out of bounds");
            }
            Ok(value)
        };
    bounded_string("revision", 160, false)?;
    let game = bounded_string("game", 2, false)?;
    if !matches!(game, "d1" | "d2") {
        return Err("invalid mission game");
    }
    if game_info.get("game").and_then(serde_json::Value::as_str) != Some(game) {
        return Err("mission requirement game does not match game_info");
    }
    let mission_key = bounded_string("mission_key", 8, true)?;
    if mission_key
        .chars()
        .any(|value| value.is_control() || matches!(value, '/' | '\\'))
    {
        return Err("invalid mission key");
    }
    if game_info.get("mission").and_then(serde_json::Value::as_str) != Some(mission_key) {
        return Err("mission requirement key does not match game_info");
    }
    if bounded_string("display_name", 128, true)?
        .chars()
        .any(char::is_control)
    {
        return Err("invalid mission display name");
    }
    if let Some(path) = requirement.get("descriptor_path") {
        let path = path.as_str().ok_or("invalid descriptor path")?;
        if path.len() > 512 || path.contains('\0') {
            return Err("invalid descriptor path");
        }
    }
    let kind = bounded_string("kind", 16, false)?;
    let offer = requirement
        .get("offer_available")
        .and_then(serde_json::Value::as_bool)
        .unwrap_or(false);
    match kind {
        "builtin" | "wrapper" => {
            let size = requirement
                .get("size_bytes")
                .and_then(serde_json::Value::as_u64)
                .filter(|size| *size > 0)
                .ok_or("invalid mission content size")?;
            let hash = bounded_string("sha256", 64, false)?;
            if hash.len() != 64
                || !hash
                    .as_bytes()
                    .iter()
                    .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(byte))
            {
                return Err("invalid mission content hash");
            }
            if kind == "builtin" {
                if offer
                    || requirement
                        .get("wrapper_filename")
                        .is_some_and(|value| !value.is_null())
                {
                    return Err("built-in mission cannot be offered as a wrapper");
                }
            } else {
                let filename = bounded_string("wrapper_filename", 255, false)?;
                if matches!(filename, "." | "..")
                    || filename
                        .chars()
                        .any(|value| value.is_control() || matches!(value, '/' | '\\'))
                {
                    return Err("invalid mission wrapper filename");
                }
                if offer && size > 256 * 1024 * 1024 {
                    return Err("offered mission wrapper exceeds transfer limit");
                }
            }
        }
        "loose" => {
            if offer
                || requirement
                    .get("size_bytes")
                    .is_some_and(|value| !value.is_null())
                || requirement
                    .get("sha256")
                    .is_some_and(|value| !value.is_null())
            {
                return Err("loose mission cannot be offered or hashed");
            }
        }
        _ => return Err("invalid mission source kind"),
    }
    Ok(())
}

fn valid_mission_status(status: &str) -> bool {
    matches!(
        status,
        "checking"
            | "match"
            | "installed_disabled"
            | "missing"
            | "size_mismatch"
            | "hash_mismatch"
            | "unsupported_source"
            | "error"
            | "queued"
            | "downloading"
            | "paused"
            | "retrying"
            | "failed_resumable"
            | "verifying"
    )
}

/// Build the list of waiting lobbies for LOBBY_LIST responses.
pub fn lobby_page(state: &ServerState, after: Option<Uuid>) -> (Vec<LobbyInfo>, Option<Uuid>) {
    let mut ids: Vec<_> = state
        .lobbies
        .iter()
        .filter(|entry| {
            matches!(entry.state, LobbyState::Waiting | LobbyState::InGame)
                && after.is_none_or(|id| *entry.key() > id)
        })
        .map(|entry| *entry.key())
        .collect();
    ids.sort_unstable();
    let more = ids.len() > 16;
    ids.truncate(16);
    let next = if more { ids.last().copied() } else { None };
    let rows = ids
        .into_iter()
        .filter_map(|id| {
            let l = state.lobbies.get(&id)?;
            let playing = l.state == LobbyState::InGame;
            Some(LobbyInfo {
                lobby_id: l.id,
                host_callsign: l.host_callsign.clone(),
                game: l.game.clone(),
                player_count: if playing {
                    l.runtime_player_count.unwrap_or(l.player_count())
                } else {
                    l.player_count()
                },
                max_players: l.max_players,
                joinable: l.is_joinable(),
                host_ping_ms: state
                    .sessions
                    .get(&l.host_player_id)
                    .and_then(|s| s.ping_ms),
                has_code: l.code.is_some(),
                verified_only: l.verified_only,
                game_info: l.game_info.clone(),
                lobby_state: if playing {
                    "in_progress".into()
                } else {
                    "waiting".into()
                },
                current_level: l.runtime_level,
            })
        })
        .collect();
    (rows, next)
}

fn send_lobby_page(
    state: &ServerState,
    player: Uuid,
    cursor: Option<Uuid>,
    listing: Option<Uuid>,
    tx: &BoundedSender,
) {
    let id = if cursor.is_none() {
        Uuid::new_v4()
    } else {
        let valid = state.sessions.get(&player).is_some_and(|s| {
            s.listing_id == listing
                && s.listing_cursor == cursor
                && s.listing_deadline > Instant::now()
        });
        if !valid {
            let _ = tx.send(ServerMessage::Error {
                code: "QUERY_EXPIRED".into(),
                message: "Refresh the lobby list".into(),
            });
            return;
        }
        listing.unwrap()
    };
    let (lobbies, next_cursor) = lobby_page(state, cursor);
    if tx
        .send(ServerMessage::LobbyList {
            lobbies,
            listing_id: id,
            reset: cursor.is_none(),
            next_cursor,
        })
        .is_ok()
    {
        if let Some(mut session) = state.sessions.get_mut(&player) {
            if cursor.is_none() {
                session.listing_deadline = Instant::now() + Duration::from_secs(30);
            }
            session.listing_id = Some(id);
            session.listing_cursor = next_cursor;
        }
    }
}

/// Build a list of active (in-progress) games, capped at 20.
fn build_active_game_list(state: &ServerState) -> Vec<ActiveGameInfo> {
    let now = chrono::Utc::now();
    let stale_cutoff = std::time::Duration::from_secs(5 * 60);
    state
        .lobbies
        .iter()
        .filter(|entry| {
            let l = entry.value();
            if !matches!(l.state, LobbyState::Starting | LobbyState::InGame) {
                return false;
            }
            // Exclude stale games: use last_state_update if available, else created_at
            let ref_time = l.last_state_update.unwrap_or(l.created_at_instant);
            ref_time.elapsed() < stale_cutoff
        })
        .take(20)
        .map(|entry| {
            let l = entry.value();
            let elapsed = (now - l.created_at).num_seconds().max(0) as u64;
            ActiveGameInfo {
                lobby_id: *entry.key(),
                host_callsign: l.host_callsign.clone(),
                mission: game_info_str(&l.game_info, "mission"),
                mode: game_info_str(&l.game_info, "mode"),
                player_count: l.player_count(),
                duration_secs: elapsed,
            }
        })
        .collect()
}

/// Per-player session state, stored in the shared sessions DashMap.
pub struct PlayerSession {
    pub listing_id: Option<Uuid>,
    pub listing_cursor: Option<Uuid>,
    pub listing_deadline: Instant,
    pub connection_id: Uuid,
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
    /// Client's public IP as observed from the WebSocket TCP connection.
    /// Used to generate "observed" candidates for port-forwarding detection.
    pub public_ip: std::net::IpAddr,
}

/// Build the WebSocket router (separated for testability).
pub fn ws_router(state: Arc<ServerState>) -> Router {
    Router::new()
        .route("/ws", get(ws_upgrade))
        .with_state(state)
}

/// Run the WebSocket server, optionally with TLS.
pub async fn run(
    addr: SocketAddr,
    state: Arc<ServerState>,
    tls_config: Option<Arc<rustls::ServerConfig>>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let app = ws_router(Arc::clone(&state));

    let listener = tokio::net::TcpListener::bind(addr).await?;
    let _ready = crate::ListenerReady::new(Arc::clone(&state), crate::ListenerReady::WS);
    info!(%addr, tls = tls_config.is_some(), "WebSocket server listening");
    crate::transport::serve(listener, app, state, tls_config).await?;
    Ok(())
}

/// Counts every upgraded socket, including unauthenticated connections
struct ConnectionPermit(Arc<ServerState>);
impl Drop for ConnectionPermit {
    fn drop(&mut self) {
        self.0
            .connected_sockets
            .fetch_sub(1, std::sync::atomic::Ordering::Relaxed);
    }
}

fn client_address(
    peer: SocketAddr,
    headers: &axum::http::HeaderMap,
    trusted: &[std::net::IpAddr],
) -> Option<SocketAddr> {
    if !trusted.contains(&peer.ip()) {
        return Some(peer);
    }
    let mut values = headers.get_all("x-real-ip").iter();
    let ip = values
        .next()?
        .to_str()
        .ok()?
        .parse::<std::net::IpAddr>()
        .ok()?;
    if values.next().is_some() || ip.is_unspecified() || ip.is_multicast() {
        return None;
    }
    Some(SocketAddr::new(ip, peer.port()))
}

async fn ws_upgrade(
    ws: WebSocketUpgrade,
    ConnectInfo(addr): ConnectInfo<SocketAddr>,
    State(state): State<Arc<ServerState>>,
    headers: axum::http::HeaderMap,
) -> impl IntoResponse {
    let Some(addr) = client_address(addr, &headers, &state.config.trusted_proxy_ips) else {
        return (
            axum::http::StatusCode::BAD_REQUEST,
            "Invalid proxy identity",
        )
            .into_response();
    };
    // Count pending and authenticated sockets atomically
    let max_conn = state.config.max_connections;
    if state
        .connected_sockets
        .fetch_update(
            std::sync::atomic::Ordering::Relaxed,
            std::sync::atomic::Ordering::Relaxed,
            |count| {
                if max_conn == 0 || count < max_conn {
                    Some(count + 1)
                } else {
                    None
                }
            },
        )
        .is_err()
    {
        warn!(%addr, max_connections = max_conn, "connection cap reached");
        return (axum::http::StatusCode::SERVICE_UNAVAILABLE, "server full").into_response();
    }
    let permit = ConnectionPermit(Arc::clone(&state));
    if !state.rate_limiter.check_ip_connection(addr.ip()) {
        warn!(%addr, "WebSocket upgrade rate-limited");
        return (axum::http::StatusCode::TOO_MANY_REQUESTS, "rate limited").into_response();
    }
    debug!(%addr, "WebSocket upgrade accepted");
    ws.max_message_size(16 * 1024)
        .max_frame_size(16 * 1024)
        .on_upgrade(move |socket| async move {
            let _permit = permit;
            handle_connection(socket, addr, state).await;
        })
        .into_response()
}

async fn handle_connection(socket: WebSocket, addr: SocketAddr, state: Arc<ServerState>) {
    let (mut ws_tx, mut ws_rx) = socket.split();

    // Bounded channel for sending messages back to this client.
    // Backpressure: if the client falls 256 messages behind, we drop it.
    let (raw_tx, mut rx) = mpsc::channel::<QueuedControl>(256);
    let retired = Arc::new(tokio::sync::Notify::new());
    let tx = BoundedSender {
        tx: raw_tx,
        retired: Arc::clone(&retired),
        global_bytes: Arc::clone(&state.control_queue_bytes),
        connection_bytes: Arc::new(tokio::sync::Semaphore::new(2 * 1024 * 1024)),
        critical_bytes: Arc::new(tokio::sync::Semaphore::new(64 * 1024)),
    };

    // Task: drain the outbound channel and send to the WebSocket
    let egress = Arc::clone(&state.control_egress);
    let mut send_task = tokio::spawn(async move {
        while let Some(msg) = rx.recv().await {
            egress.wait(msg.text.len()).await;
            // Timeout on send: if the client's TCP window is full for 10s, disconnect
            match tokio::time::timeout(
                Duration::from_secs(10),
                ws_tx.send(Message::Text(msg.text.into())),
            )
            .await
            {
                Ok(Ok(())) => {}
                Ok(Err(_)) => break, // send error
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
    struct PendingInstallation {
        challenge: String,
        public_key: String,
        callsign: String,
        platform: String,
        audience: String,
        difficulty: u8,
    }
    let mut pending_installation: Option<PendingInstallation> = None;

    let auth_deadline = tokio::time::Instant::now() + Duration::from_secs(20);
    // Authentication has an absolute deadline, including peers sending pings
    while let Some(msg_result) = match tokio::select! {
        biased;
        _ = retired.notified() => Ok(None),
        result = tokio::time::timeout_at(if player_id.is_none() { auth_deadline } else {
                tokio::time::Instant::now() + Duration::from_secs(120)
            }, ws_rx.next()) => result,
    } {
        Ok(Some(r)) => Some(r),
        Ok(None) => None, // stream ended
        Err(_) => {
            debug!(%addr, "WebSocket read timed out (120s idle)");
            None // treat as disconnect
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

        let auth_message = matches!(
            &client_msg,
            ClientMessage::Authenticate { .. }
                | ClientMessage::InstallationProof { .. }
                | ClientMessage::PowSolution { .. }
        );
        let _auth_slot = if auth_message {
            if !state.auth_work.lock().unwrap().accept(Instant::now()) {
                let _ = tx.send(ServerMessage::AuthFail {
                    reason: "Authentication busy; retry later".into(),
                });
                break;
            }
            match state.auth_slots.try_acquire() {
                Ok(permit) => Some(permit),
                Err(_) => {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Authentication busy; retry later".into(),
                    });
                    break;
                }
            }
        } else {
            None
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
                auth_audience,
            } => {
                if player_id.is_some() {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Already authenticated".into(),
                    });
                    break;
                }
                // A new attempt retires any challenge from an earlier identity
                pending_pow = None;
                pending_installation = None;
                // Version check first
                if !(MIN_CLIENT_PROTOCOL..=CURRENT_PROTOCOL).contains(&protocol_version) {
                    warn!(%addr, %client_version, %protocol_version, required = MIN_CLIENT_PROTOCOL, "client version rejected");
                    let _ = tx.send(ServerMessage::VersionRejected {
                        reason: format!(
                            "Client protocol {protocol_version} is unsupported; this server requires protocol {MIN_CLIENT_PROTOCOL}..={CURRENT_PROTOCOL}. Please update to a compatible client."
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
                if callsign.is_empty()
                    || callsign.len() > 20
                    || !callsign.bytes().all(|b| (0x20..=0x7E).contains(&b))
                {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Invalid callsign (1-20 printable ASCII characters)".into(),
                    });
                    break;
                }

                if !matches!(
                    auth_method.as_str(),
                    "keypair" | "gpgs" | "dev" | "p256_challenge"
                ) {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Unsupported authentication method".into(),
                    });
                    break;
                }
                if auth_method == "p256_challenge" {
                    let audience = auth_audience.unwrap_or_default();
                    if audience.is_empty() || audience != state.config.installation_auth_audience {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Installation authentication audience unavailable".into(),
                        });
                        break;
                    }
                    let Some(pk) = public_key else {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Installation public key required".into(),
                        });
                        break;
                    };
                    if pk.len() != 130
                        || !pk
                            .bytes()
                            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
                        || !pk.starts_with("04")
                        || platform.is_empty()
                        || platform.len() > 32
                        || !platform.bytes().all(|b| (0x20..=0x7e).contains(&b))
                    {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Invalid installation identity".into(),
                        });
                        break;
                    }
                    let hash = format!("p256:{}", pow::hash_pubkey(&pk));
                    let difficulty = match state.db.find_player_by_keypair(&hash) {
                        Ok(Some(_)) => 0,
                        Ok(None) => state.config.pow_difficulty,
                        Err(_) => {
                            let _ = tx.send(ServerMessage::AuthFail {
                                reason: "Identity storage unavailable".into(),
                            });
                            break;
                        }
                    };
                    let challenge = pow::generate_challenge();
                    let _ = tx.send(ServerMessage::InstallationChallenge {
                        challenge: challenge.clone(),
                        difficulty,
                    });
                    pending_installation = Some(PendingInstallation {
                        challenge,
                        public_key: pk,
                        callsign,
                        platform,
                        audience,
                        difficulty,
                    });
                    continue;
                }
                // The current keypair producer is local-test-only until challenge-bound
                // authentication and the matching release client are qualified
                if auth_method == "keypair"
                    && (!state.config.skip_gpgs_verify || !addr.ip().is_loopback())
                {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Keypair authentication is unavailable for this preview".into(),
                    });
                    break;
                }
                if auth_method != "keypair"
                    && ((state.config.skip_gpgs_verify && !addr.ip().is_loopback())
                        || (!state.config.skip_gpgs_verify
                            && (auth_method == "dev"
                                || state.config.google_client_id.is_empty()
                                || state.config.google_client_secret.is_empty())))
                {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Authentication method unavailable on this server".into(),
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
                    if token.is_empty() || token.len() > 4096 {
                        let _ = tx.send(ServerMessage::AuthFail {
                            reason: "Invalid authentication token".into(),
                        });
                        break;
                    }
                    let gpgs_id = if state.config.skip_gpgs_verify {
                        debug!(%addr, "GPGS verification skipped (dev mode)");
                        token.clone()
                    } else {
                        match tokio::time::timeout_at(
                            auth_deadline,
                            identity::verify_gpgs_token(
                                auth_deadline,
                                &state.config.google_client_id,
                                &state.config.google_client_secret,
                                &token,
                            ),
                        )
                        .await
                        {
                            Ok(identity::VerifyResult::Ok { gpgs_player_id }) => gpgs_player_id,
                            Ok(identity::VerifyResult::Failed { reason }) => {
                                warn!(%addr, %reason, "GPGS authentication failed");
                                let _ = tx.send(ServerMessage::AuthFail {
                                    reason: "Google Play Games verification failed".into(),
                                });
                                break;
                            }
                            Err(_) => {
                                let _ = tx.send(ServerMessage::AuthFail {
                                    reason: "Authentication deadline exceeded".into(),
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

            ClientMessage::InstallationProof {
                challenge,
                solution,
                signature,
            } => {
                if player_id.is_some() {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Already authenticated".into(),
                    });
                    break;
                }
                // Consume before verification: every connection challenge permits one proof
                let Some(pending) = pending_installation.take() else {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "No installation challenge pending".into(),
                    });
                    break;
                };
                let transcript = pow::installation_transcript(
                    &pending.challenge,
                    &pending.public_key,
                    &pending.callsign,
                    &pending.platform,
                    &pending.audience,
                    &solution,
                );
                if tokio::time::Instant::now() >= auth_deadline
                    || challenge != pending.challenge
                    || solution.is_empty()
                    || solution.len() > 20
                    || !solution.bytes().all(|b| b.is_ascii_digit())
                    || signature.len() > 144
                    || !pow::verify_pow(&challenge, &solution, pending.difficulty)
                    || !pow::verify_installation_signature(
                        &pending.public_key,
                        transcript.as_bytes(),
                        &signature,
                    )
                {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Invalid installation proof".into(),
                    });
                    break;
                }
                let hash = format!("p256:{}", pow::hash_pubkey(&pending.public_key));
                let result = {
                    // Serialize first registration, then release before complete_auth takes ownership
                    let _control = state.control.lock().unwrap();
                    state
                        .db
                        .find_player_by_keypair(&hash)
                        .and_then(|existing| match existing {
                            Some(id) => Ok(id),
                            None => state.db.register_keypair_player(&hash, &pending.callsign),
                        })
                };
                let Ok(pid) = result else {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Identity storage unavailable".into(),
                    });
                    break;
                };
                if state.db.is_banned(&pid).unwrap_or(true) {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Identity unavailable".into(),
                    });
                    break;
                }
                complete_auth(
                    &state,
                    &tx,
                    pid,
                    &pending.callsign,
                    &pending.platform,
                    false,
                    "p256_challenge",
                    session_id,
                    addr,
                );
                pending_pow = None;
                player_id = Some(pid);
            }

            ClientMessage::PowSolution {
                challenge,
                solution,
            } => {
                if player_id.is_some() {
                    let _ = tx.send(ServerMessage::AuthFail {
                        reason: "Already authenticated".into(),
                    });
                    break;
                }
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
                let _transition = state.control.lock().unwrap();
                if state
                    .sessions
                    .get(&pid)
                    .is_none_or(|s| s.connection_id != session_id)
                {
                    break;
                }
                handle_authenticated_message(&state, pid, client_msg, &tx);
            }
        }
    }

    // Ownership check and cleanup share the same transaction as replacement and commands
    if let Some(pid) = player_id {
        let _transition = state.control.lock().unwrap();
        if state
            .sessions
            .get(&pid)
            .is_some_and(|s| s.connection_id == session_id)
        {
            remove_session(&state, pid);
        }
    }

    state.rate_limiter.release_connection(session_id);

    // Drop the sender so the send_task can drain remaining messages
    // and exit cleanly instead of being aborted mid-flight.
    drop(tx);
    if tokio::time::timeout(Duration::from_secs(2), &mut send_task)
        .await
        .is_err()
    {
        send_task.abort();
        let _ = send_task.await;
    }
}

const LATE_JOIN_TIMEOUT: Duration = Duration::from_secs(30);
// Startup and native consent/synchronization each have a 60-second allowance
const NATIVE_ADMISSION_TIMEOUT: Duration = Duration::from_secs(120);

/// Retire only expired, unlaunched memberships; established peer routes stay owned
pub fn expire_pending_late_joins(state: &ServerState, now: Instant) {
    let _transition = state.control.lock().unwrap();
    let expired: Vec<_> = state
        .lobbies
        .iter()
        .flat_map(|room| {
            room.pending_late_joiners
                .iter()
                .filter_map(|(pid, started)| {
                    (now.saturating_duration_since(*started) >= LATE_JOIN_TIMEOUT).then_some(*pid)
                })
                .chain(
                    room.assigned_late_joiners
                        .iter()
                        .filter_map(|(pid, lease)| {
                            (now.saturating_duration_since(lease.assigned)
                                >= NATIVE_ADMISSION_TIMEOUT)
                                .then_some(*pid)
                        }),
                )
                .collect::<Vec<_>>()
        })
        .collect();
    for pid in expired {
        leave_room(state, pid);
        if let Some(session) = state.sessions.get(&pid) {
            let _ = session.tx.send(ServerMessage::Error {
                code: "LOBBY_JOIN_TIMEOUT".into(),
                message: "Joining the running game timed out".into(),
            });
        }
    }
}

/// Leave a room under the control transition lock; host departure closes it
fn leave_room(state: &ServerState, pid: Uuid) {
    crate::relay::revoke_player(state, pid);
    let room_id = state.sessions.get(&pid).and_then(|s| s.lobby_id);
    let Some(room_id) = room_id else {
        return;
    };
    if let Some(mut session) = state.sessions.get_mut(&pid) {
        session.lobby_id = None;
        session.presence = Presence::Online;
    }
    let close = if let Some(mut room) = state.lobbies.get_mut(&room_id) {
        room.remove_player(&pid);
        if room.host_player_id == pid || room.players.is_empty() {
            true
        } else {
            broadcast_lobby_update(&room, state);
            false
        }
    } else {
        false
    };
    if close {
        close_room(
            state,
            room_id,
            pid,
            "HOST_DISCONNECTED",
            "The host has left the room",
        );
    }
}

/// Close a room once, fencing all memberships and allocations under control lock
fn close_room(state: &ServerState, room_id: Uuid, requester: Uuid, code: &str, message: &str) {
    crate::relay::revoke_room(state, room_id);
    if let Some((_, room)) = state.lobbies.remove(&room_id) {
        for peer in &room.players {
            if let Some(mut session) = state.sessions.get_mut(&peer.player_id) {
                if session.lobby_id == Some(room_id) {
                    session.lobby_id = None;
                    session.presence = Presence::Online;
                    if peer.player_id != requester {
                        let _ = session.tx.send(ServerMessage::Error {
                            code: code.into(),
                            message: message.into(),
                        });
                    }
                }
            }
        }
        state.stats.lobby_closed();
        if matches!(room.state, LobbyState::Starting | LobbyState::InGame) {
            state.stats.game_ended();
        }
    }
}

/// Retire exactly the current session under the control transition lock
fn remove_session(state: &ServerState, pid: Uuid) {
    leave_room(state, pid);
    if let Some((_, old)) = state.sessions.remove(&pid) {
        old.tx.retired.notify_one();
        state.stats.player_disconnected();
        // Atomic ref-count transition for multiple clients behind the same NAT
        if let dashmap::mapref::entry::Entry::Occupied(mut entry) =
            state.stun_allowlist.entry(old.public_ip)
        {
            if *entry.get() <= 1 {
                entry.remove();
            } else {
                *entry.get_mut() -= 1;
            }
        }
        let _ = state
            .db
            .log_connection_event(Some(&pid), "disconnect", None);
    }
}

/// Pick the best candidate address for direct connection: prefer srflx, then
/// observed (port-forwarded), then host.
fn best_candidate_addr(candidates: &[crate::protocol::ConnectionCandidate]) -> String {
    best_candidate_addr_for_type(candidates, &crate::lobby::ConnectionType::Unknown)
}

/// Pick the best candidate address considering the determined connection type.
/// - DirectLan: prefer host (LAN address) so peers talk directly on LAN, not
///   via the public NAT address (which fails with hairpin NAT).
/// - DirectUpnp: prefer upnp candidate (explicitly port-forwarded).
/// - Everything else: srflx > observed > host.
fn best_candidate_addr_for_type(
    candidates: &[crate::protocol::ConnectionCandidate],
    conn_type: &crate::lobby::ConnectionType,
) -> String {
    use crate::lobby::ConnectionType;
    match conn_type {
        ConnectionType::DirectLan => candidates
            .iter()
            .find(|c| c.candidate_type == "host")
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "observed"))
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "srflx"))
            .map(|c| c.addr.clone())
            .unwrap_or_default(),
        ConnectionType::DirectUpnp => candidates
            .iter()
            .find(|c| c.candidate_type == "upnp")
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "srflx"))
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "observed"))
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "host"))
            .map(|c| c.addr.clone())
            .unwrap_or_default(),
        _ => candidates
            .iter()
            .find(|c| c.candidate_type == "srflx")
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "observed"))
            .or_else(|| candidates.iter().find(|c| c.candidate_type == "host"))
            .map(|c| c.addr.clone())
            .unwrap_or_default(),
    }
}

/// Check if two IPv4 addresses are both in the same private subnet (/16 for
/// 192.168.x.x and 172.16-31.x.x, /8 for 10.x.x.x).
fn ips_share_private_subnet(a: &str, b: &str) -> bool {
    let parse = |s: &str| -> Option<[u8; 4]> {
        let parts: Vec<&str> = s.split('.').collect();
        if parts.len() != 4 {
            return None;
        }
        Some([
            parts[0].parse().ok()?,
            parts[1].parse().ok()?,
            parts[2].parse().ok()?,
            parts[3].parse().ok()?,
        ])
    };
    let (a, b) = match (parse(a), parse(b)) {
        (Some(a), Some(b)) => (a, b),
        _ => return false,
    };
    // 10.0.0.0/8
    if a[0] == 10 && b[0] == 10 {
        return true;
    }
    // 172.16.0.0/12 -- compare first two octets
    if a[0] == 172 && b[0] == 172 && (16..=31).contains(&a[1]) && (16..=31).contains(&b[1]) {
        return a[1] == b[1];
    }
    // 192.168.0.0/16 -- compare first two octets
    if a[0] == 192 && a[1] == 168 && b[0] == 192 && b[1] == 168 {
        return true;
    }
    false
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

    // Extract public-facing addrs: srflx (from STUN) or observed (server-injected)
    let a_public = a
        .candidates
        .iter()
        .find(|c| c.candidate_type == "srflx" || c.candidate_type == "observed");
    let b_public = b
        .candidates
        .iter()
        .find(|c| c.candidate_type == "srflx" || c.candidate_type == "observed");

    // Check for same public IP (likely LAN)
    if let (Some(ac), Some(bc)) = (a_public, b_public) {
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

    // Both cone NATs or both have public-facing candidates (srflx or observed)
    if a_public.is_some() && b_public.is_some() {
        return (
            ConnectionType::DirectHolepunch,
            Some("both peers have public-facing candidates".into()),
        );
    }

    // One side has a public-facing candidate (port-forwarded or STUN) and the other
    // has only host candidates -- the public side is reachable, connectivity check
    // will confirm.
    if a_public.is_some() || b_public.is_some() {
        return (
            ConnectionType::DirectHolepunch,
            Some("one peer has public-facing candidate".into()),
        );
    }

    // Both peers have only host candidates (no srflx) -- check for LAN via
    // matching private IP prefix. This enables LAN play without STUN.
    let a_hosts: Vec<&str> = a
        .candidates
        .iter()
        .filter(|c| c.candidate_type == "host")
        .filter_map(|c| c.addr.split(':').next())
        .collect();
    let b_hosts: Vec<&str> = b
        .candidates
        .iter()
        .filter(|c| c.candidate_type == "host")
        .filter_map(|c| c.addr.split(':').next())
        .collect();

    if !a_hosts.is_empty() && !b_hosts.is_empty() {
        // Check if any host IP pair shares a /16 private subnet
        for a_ip in &a_hosts {
            for b_ip in &b_hosts {
                if ips_share_private_subnet(a_ip, b_ip) {
                    return (
                        ConnectionType::DirectLan,
                        Some("host candidates on same private subnet".into()),
                    );
                }
            }
        }
        // Host candidates exist but not on same subnet -- try holepunch
        return (
            ConnectionType::DirectHolepunch,
            Some("host candidates only, different subnets".into()),
        );
    }

    // No candidates at all from either peer -- fall back to relay
    if a.candidates.is_empty() && b.candidates.is_empty() {
        return (
            ConnectionType::Relay,
            Some("no candidates from either peer".into()),
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

/// Generate "observed" candidates using the server-known public IP (from the
/// WebSocket TCP connection) combined with each host candidate's port.
///
/// If a player has manually forwarded their game port at their router, the
/// address public_ip:game_port is directly reachable. The connectivity checker
/// will verify reachability. Skips if matching srflx candidates already exist.
fn generate_observed_candidates(
    candidates: &[ConnectionCandidate],
    ws_ip: std::net::IpAddr,
) -> Vec<ConnectionCandidate> {
    let ws_ip_str = ws_ip.to_string();
    // Don't inject for loopback (local testing) or link-local addresses
    if ws_ip.is_loopback() {
        return Vec::new();
    }

    // Collect existing srflx addrs to avoid duplicates
    let srflx_addrs: std::collections::HashSet<String> = candidates
        .iter()
        .filter(|c| c.candidate_type == "srflx")
        .map(|c| c.addr.clone())
        .collect();

    // Extract ports from host candidates
    let host_ports: Vec<u16> = candidates
        .iter()
        .filter(|c| c.candidate_type == "host")
        .filter_map(|c| c.addr.rsplit_once(':'))
        .filter_map(|(_, port_str)| port_str.parse::<u16>().ok())
        .collect();

    let mut observed = Vec::new();
    for port in host_ports {
        let addr = format!("{ws_ip_str}:{port}");
        if !srflx_addrs.contains(&addr) {
            observed.push(ConnectionCandidate {
                candidate_type: "observed".into(),
                addr,
            });
        }
    }
    observed
}

/// Compute connection-check priority for a candidate pair.
fn candidate_pair_priority(local_type: &str, remote_type: &str) -> u32 {
    match (local_type, remote_type) {
        ("host", "host") => 100,
        ("upnp", _) | (_, "upnp") => 90,
        ("srflx", "srflx") => 80,
        ("srflx", "host") | ("host", "srflx") => 75,
        // "observed" = server-injected public_ip:game_port for port-forward detection
        ("observed", _) | (_, "observed") => 70,
        ("predicted", "predicted") => 60,
        ("predicted", "srflx") | ("srflx", "predicted") => 50,
        ("predicted", _) | (_, "predicted") => 40,
        _ => 10,
    }
}

/// Build per-player CONNECTIVITY_CHECK_GO messages with prioritized candidate pairs.
fn build_connectivity_check_messages(lobby: &mut Lobby) -> Vec<(Uuid, ServerMessage)> {
    let mut result = Vec::new();
    for player in &mut lobby.players {
        player.connectivity_checks.clear();
        player.validated_routes.clear();
    }
    for player in &lobby.players {
        let mut pairs: Vec<CandidatePair> = Vec::new();
        for peer in &lobby.players {
            if peer.player_id == player.player_id {
                continue;
            }
            for local_cand in &player.candidates {
                for remote_cand in &peer.candidates {
                    pairs.push(CandidatePair {
                        check_id: Uuid::new_v4(),
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
    for (player_id, message) in &result {
        if let ServerMessage::ConnectivityCheckGo { peer_addrs } = message {
            let player = lobby
                .players
                .iter_mut()
                .find(|p| p.player_id == *player_id)
                .unwrap();
            player
                .connectivity_checks
                .extend(peer_addrs.iter().map(|pair| (pair.check_id, pair.clone())));
        }
    }
    result
}

/// Resolve only the endpoint this reporter validated for this peer's current candidates
fn validated_remote_addr(
    reporter: &crate::lobby::LobbyPlayer,
    peer: &crate::lobby::LobbyPlayer,
) -> String {
    reporter
        .validated_routes
        .get(&peer.player_id)
        .filter(|pair| {
            peer.candidates.iter().any(|candidate| {
                candidate.candidate_type == pair.remote_type && candidate.addr == pair.remote_addr
            })
        })
        .map(|pair| pair.remote_addr.clone())
        .unwrap_or_default()
}

/// An authenticated player can only accept an issued offer for the current room/peer/attempt
fn accept_connectivity_report(
    lobby: &mut Lobby,
    reporter: Uuid,
    peer: Uuid,
    check_id: Uuid,
    kind: &str,
    rtt: u32,
) -> bool {
    lobby.expire_connectivity_attempt(Instant::now());
    let current_attempt = match lobby.state {
        LobbyState::Holepunching => lobby
            .holepunch_started_at
            .is_some_and(|started| started.elapsed() < crate::lobby::HOLEPUNCH_TIMEOUT),
        LobbyState::InGame => lobby
            .pending_late_joiners
            .get(&reporter)
            .is_some_and(|started| started.elapsed() < LATE_JOIN_TIMEOUT),
        _ => false,
    };
    if !current_attempt {
        return false;
    }
    let Some(player) = lobby.players.iter().find(|p| p.player_id == reporter) else {
        return false;
    };
    let Some(pair) = player.connectivity_checks.get(&check_id).cloned() else {
        return false;
    };
    if reporter == peer || pair.peer_id != peer || (kind != "relay" && pair.remote_type != kind) {
        return false;
    }
    let Some(remote) = lobby.players.iter().find(|p| p.player_id == peer) else {
        return false;
    };
    if !remote.candidates.iter().any(|candidate| {
        candidate.candidate_type == pair.remote_type && candidate.addr == pair.remote_addr
    }) {
        return false;
    }
    let conn_type = match kind {
        "host" => crate::lobby::ConnectionType::DirectLan,
        "upnp" => crate::lobby::ConnectionType::DirectUpnp,
        "srflx" | "observed" => crate::lobby::ConnectionType::DirectHolepunch,
        "predicted" => crate::lobby::ConnectionType::PredictedHolepunch,
        "relay" => crate::lobby::ConnectionType::Relay,
        _ => return false,
    };
    let player = lobby
        .players
        .iter_mut()
        .find(|p| p.player_id == reporter)
        .unwrap();
    player
        .connectivity_checks
        .retain(|_, offer| offer.peer_id != peer);
    player.validated_routes.remove(&peer);
    if kind != "relay" {
        player.validated_routes.insert(peer, pair);
    }
    player.connection_type = conn_type;
    player.ping_ms = Some(rtt);
    true
}

struct RelayPair {
    slot_a: u8,
    slot_b: u8,
    pid_a: Uuid,
    pid_b: Uuid,
}

/// Allocate a relay session for a player pair and send RELAY_ASSIGNED to both.
/// Returns None if relay is not configured or the server-wide relay limit is reached.
fn allocate_relay_session(
    state: &ServerState,
    relay_addr: &str,
    room_id: Uuid,
    pair: &RelayPair,
) -> Option<u32> {
    if relay_addr.is_empty() {
        return None;
    }
    let max = state.config.max_relay_sessions;
    if max > 0 && state.relay_sessions.len() >= max {
        return None;
    }
    if !state.allocation_work.lock().unwrap().accept(
        pair.pid_a,
        pair.pid_b,
        room_id,
        Instant::now(),
    ) {
        return None;
    }
    let token = (0..16)
        .map(|_| Uuid::new_v4().as_u128() as u32)
        .find(|token| !state.relay_sessions.contains_key(token))?;
    state.relay_sessions.insert(
        token,
        crate::relay::RelaySession::new(
            room_id,
            [(pair.slot_a, pair.pid_a), (pair.slot_b, pair.pid_b)],
            Instant::now(),
        ),
    );
    Some(token)
}

fn relay_credentials(
    state: &ServerState,
    token: Option<u32>,
    slot: u8,
) -> Option<crate::relay::RelayCredentials> {
    state
        .relay_sessions
        .get(&token?)
        .and_then(|session| session.credentials(slot))
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
    let _transition = state.control.lock().unwrap();
    remove_session(state, pid);
    state.sessions.insert(
        pid,
        PlayerSession {
            listing_id: None,
            listing_cursor: None,
            listing_deadline: Instant::now(),
            connection_id: session_id,
            player_id: pid,
            callsign: callsign.to_string(),
            presence: Presence::Online,
            tx: tx.clone(),
            lobby_id: None,
            ping_ms: None,
            gpgs_verified,
            auth_method: auth_method.to_string(),
            public_ip: addr.ip(),
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

    send_lobby_page(state, pid, None, None, tx);

    let friend_list = friends::build_friend_list(state, &pid);
    let _ = tx.send(friend_list);

    let _ = state.db.log_connection_event(
        Some(&pid),
        "connect",
        Some(&format!("platform={platform},auth={auth_method}")),
    );
}

// Called only while the control mutex is held; no map guard may span this call
fn complete_late_join(
    state: &Arc<ServerState>,
    player_id: Uuid,
    lobby_id: Uuid,
    tx: &BoundedSender,
) {
    let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) else {
        return;
    };
    if lobby.state != LobbyState::InGame {
        return;
    }
    if !lobby.pending_late_joiners.contains_key(&player_id) {
        return;
    }
    // Native host consent owns one admission at a time. Never expose an
    // unaccepted peer to a second joiner; that joiner may retry after acceptance
    if !lobby.assigned_late_joiners.is_empty() {
        drop(lobby);
        leave_room(state, player_id);
        let _ = tx.send(ServerMessage::Error {
            code: "LOBBY_JOIN_BUSY".into(),
            message: "Another player is awaiting host consent; please join again shortly".into(),
        });
        return;
    }
    let Some(started) = lobby.pending_late_joiners.remove(&player_id) else {
        return;
    };
    if started.elapsed() >= LATE_JOIN_TIMEOUT {
        drop(lobby);
        leave_room(state, player_id);
        let _ = tx.send(ServerMessage::Error {
            code: "LOBBY_JOIN_TIMEOUT".into(),
            message: "Joining the running game timed out".into(),
        });
        return;
    }
    let host_id = lobby.host_player_id;
    let Some(host_connection) = state.sessions.get(&host_id).map(|s| s.connection_id) else {
        return;
    };
    let admission_id = Uuid::new_v4();
    let joiner = lobby
        .players
        .iter()
        .find(|p| p.player_id == player_id)
        .cloned();
    let established: Vec<_> = lobby
        .players
        .iter()
        .filter(|p| {
            p.player_id != player_id
                && !lobby.pending_late_joiners.contains_key(&p.player_id)
                && !lobby.assigned_late_joiners.contains_key(&p.player_id)
        })
        .cloned()
        .collect();
    if let Some(joiner) = joiner {
        let relay_addr = state.config.relay_public_addr.clone();
        let mut peers = Vec::new();
        let mut approvals = Vec::new();
        let mut failed = !established.iter().any(|p| p.player_id == host_id);
        let mut host_addr = String::new();
        for existing in &established {
            let (method, _) = determine_connection_type(existing, &joiner);
            let existing_addr = validated_remote_addr(&joiner, existing);
            let joiner_addr = validated_remote_addr(existing, &joiner);
            // Late-join ICE currently lacks reverse proof. Every unconfirmed
            // direction uses authenticated relay until pairwise ICE exists
            let is_relay = state.config.force_relay
                || existing.player_id != host_id
                || method == crate::lobby::ConnectionType::Relay
                || existing_addr.is_empty()
                || joiner_addr.is_empty();
            let token = if is_relay && !relay_addr.is_empty() {
                allocate_relay_session(
                    state,
                    &relay_addr,
                    lobby_id,
                    &RelayPair {
                        slot_a: existing.slot,
                        slot_b: joiner.slot,
                        pid_a: existing.player_id,
                        pid_b: player_id,
                    },
                )
            } else {
                None
            };
            if is_relay && token.is_none() {
                failed = true;
                break;
            }
            let remote = if is_relay {
                relay_addr.clone()
            } else {
                existing_addr
            };
            if existing.player_id == host_id {
                host_addr = remote.clone();
            }
            peers.push(PeerAssignment {
                slot: existing.slot,
                addr: remote,
                is_relay,
                relay_token: token,
                relay_credentials: relay_credentials(state, token, joiner.slot),
                relay_dest_slot: if is_relay { Some(existing.slot) } else { None },
            });
            approvals.push((
                existing.player_id,
                ServerMessage::LateJoinApproved {
                    admission_id: (existing.player_id == host_id).then_some(admission_id),
                    peer: PeerAssignment {
                        slot: joiner.slot,
                        addr: if is_relay {
                            relay_addr.clone()
                        } else {
                            joiner_addr
                        },
                        is_relay,
                        relay_token: token,
                        relay_credentials: relay_credentials(state, token, existing.slot),
                        relay_dest_slot: if is_relay { Some(joiner.slot) } else { None },
                    },
                },
            ));
        }
        if failed {
            drop(lobby);
            // Revokes every partially allocated pair for this joiner
            leave_room(state, player_id);
            let _ = tx.send(ServerMessage::Error {
                code: "LOBBY_JOIN_FAILED".into(),
                message: "Relay capacity is unavailable; please join again later".into(),
            });
            return;
        }
        lobby.assigned_late_joiners.insert(
            player_id,
            crate::lobby::AssignedAdmission {
                id: admission_id,
                host_connection,
                assigned: Instant::now(),
                slot: joiner.slot,
            },
        );
        let message = ServerMessage::GameStarting {
            game_epoch: lobby.game_epoch,
            host_addr,
            game: lobby.game.clone(),
            your_slot: joiner.slot,
            max_players: lobby.max_players,
            current_level: lobby.runtime_level,
            game_info: lobby.game_info.clone(),
            peers,
        };
        let mission = game_info_str(&lobby.game_info, "mission");
        let player_count = lobby.player_count();
        drop(lobby);
        if let Some(mut session) = state.sessions.get_mut(&player_id) {
            session.presence = Presence::InGame {
                lobby_id,
                mission,
                player_count,
            };
        }
        if let Some(session) = state.sessions.get(&player_id) {
            let _ = session.tx.send(message);
        }
        for (id, approval) in approvals {
            if let Some(session) = state.sessions.get(&id) {
                let _ = session.tx.send(approval);
            }
        }
    }
}

fn handle_authenticated_message(
    state: &Arc<ServerState>,
    player_id: Uuid,
    msg: ClientMessage,
    tx: &BoundedSender,
) {
    match msg {
        ClientMessage::CreateLobby {
            game,
            max_players,
            lobby_code,
            verified_only,
            game_info,
        } => {
            // Validate lobby parameters
            if !(2..=8).contains(&max_players) {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "max_players must be 2-8".into(),
                });
                return;
            }
            if game.len() > 32 {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "game string too long".into(),
                });
                return;
            }
            if serde_json::to_string(&game_info).unwrap_or_default().len() > GAME_INFO_MAX_BYTES {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "game_info too large (max 5KB)".into(),
                });
                return;
            }
            if let Err(reason) = validate_mission_requirement(&game_info) {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: reason.into(),
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

            leave_room(state, player_id);
            let lobby = Lobby::new(
                player_id,
                callsign,
                game,
                max_players,
                lobby_code,
                verified_only,
                game_info,
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

        ClientMessage::ListLobbies { cursor, listing_id } => {
            if cursor.is_none() && !state.rate_limiter.check_lobby_list(player_id) {
                let _ = tx.send(ServerMessage::RateLimited {
                    retry_after_ms: 10_000,
                });
            } else {
                send_lobby_page(state, player_id, cursor, listing_id, tx);
            }
        }

        ClientMessage::UpdateGameInfo { game_info } => {
            if serde_json::to_string(&game_info).unwrap_or_default().len() > GAME_INFO_MAX_BYTES {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: "game_info too large (max 5KB)".into(),
                });
                return;
            }
            if let Err(reason) = validate_mission_requirement(&game_info) {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_PARAMS".into(),
                    message: reason.into(),
                });
                return;
            }
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    if lobby.host_player_id != player_id {
                        let _ = tx.send(ServerMessage::Error {
                            code: "NOT_HOST".into(),
                            message: "Only the host can update game info".into(),
                        });
                        return;
                    }
                    if lobby.state != LobbyState::Waiting {
                        let _ = tx.send(ServerMessage::Error {
                            code: "INVALID_STATE".into(),
                            message: "Cannot update game info after game started".into(),
                        });
                        return;
                    }
                    lobby.game_info = game_info;
                    for p in &mut lobby.players {
                        p.ready = false;
                        p.mission_status = None;
                    }
                    broadcast_lobby_update(&lobby, state);
                }
            }
        }

        ClientMessage::UpdateGameState {
            player_count,
            max_players,
            current_level,
            game_status,
        } => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    if lobby.host_player_id != player_id {
                        return; // silently ignore non-host updates
                    }
                    if lobby.state != LobbyState::InGame && lobby.state != LobbyState::Starting {
                        return;
                    }
                    // First update transitions Starting -> InGame
                    if lobby.state == LobbyState::Starting {
                        lobby.state = LobbyState::InGame;
                        info!(%lobby_id, "lobby transitioned Starting -> InGame");
                    }
                    lobby.runtime_player_count = Some(player_count);
                    lobby.runtime_level = Some(current_level);
                    lobby.runtime_game_status = Some(game_status);
                    lobby.last_state_update = Some(Instant::now());
                    // Update max_players in case host changed it mid-game
                    if (2..=8).contains(&max_players) {
                        lobby.max_players = max_players;
                    }
                }
            }
        }

        ClientMessage::HostNativeAdmitted {
            lobby_id,
            game_epoch,
            admission_id,
            slot,
        } => {
            let Some(host_connection) = state
                .sessions
                .get(&player_id)
                .filter(|s| s.lobby_id == Some(lobby_id))
                .map(|s| s.connection_id)
            else {
                return;
            };
            if let Some(mut room) = state.lobbies.get_mut(&lobby_id) {
                if room.host_player_id != player_id
                    || room.state != LobbyState::InGame
                    || room.game_epoch != game_epoch
                {
                    return;
                }
                let accepted = room.assigned_late_joiners.iter().find_map(|(pid, lease)| {
                    (lease.id == admission_id
                        && lease.slot == slot
                        && lease.host_connection == host_connection
                        && lease.assigned.elapsed() < NATIVE_ADMISSION_TIMEOUT)
                        .then_some(*pid)
                });
                if let Some(pid) = accepted {
                    room.assigned_late_joiners.remove(&pid);
                }
            }
        }

        ClientMessage::EndGame {} => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    if lobby.host_player_id != player_id {
                        return; // only host can end the game
                    }
                    if matches!(lobby.state, LobbyState::InGame | LobbyState::Starting) {
                        crate::relay::revoke_room(state, lobby_id);
                        state.stats.game_ended();
                        lobby.state = LobbyState::Waiting;
                        lobby.pending_late_joiners.clear();
                        lobby.assigned_late_joiners.clear();
                        lobby.game_epoch = Uuid::nil();
                        lobby.runtime_player_count = None;
                        lobby.runtime_level = None;
                        lobby.runtime_game_status = None;
                        lobby.last_state_update = None;
                        // Reset all players to not-ready
                        for p in &mut lobby.players {
                            p.ready = false;
                            p.connectivity_checks.clear();
                            p.validated_routes.clear();
                            p.mission_status = None;
                        }
                        info!(%player_id, %lobby_id, "host ended game, lobby reset to Waiting");
                        broadcast_lobby_update(&lobby, state);
                    }
                }
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

            let old_room = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if old_room == Some(lobby_id) {
                if let Some(room) = state.lobbies.get(&lobby_id) {
                    broadcast_lobby_update(&room, state);
                }
                return;
            }
            if !state
                .lobbies
                .get(&lobby_id)
                .is_some_and(|room| room.is_joinable())
            {
                let _ = tx.send(ServerMessage::Error {
                    code: "JOIN_FAILED".into(),
                    message: "Room unavailable".into(),
                });
                return;
            }
            leave_room(state, player_id);
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
            leave_room(state, player_id);
        }

        ClientMessage::Ready { ready } => {
            let session_room = state.sessions.get(&player_id).map(|s| s.lobby_id);
            if let Some(Some(lobby_id)) = session_room {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    let requirement_present = mission_requirement(&lobby.game_info).is_some();
                    if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id) {
                        let mission_matches =
                            p.mission_status.as_ref().map(|s| s.status.as_str()) == Some("match");
                        p.ready = ready && (!requirement_present || mission_matches);
                    }
                    broadcast_lobby_update(&lobby, state);
                }
            }
        }

        ClientMessage::MissionStatus {
            revision,
            status,
            verified_bytes,
            total_bytes,
            transfer_id,
            attempt,
            failure_code,
            game_data_warning,
        } => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    let Some((required_revision, required_size)) =
                        mission_requirement(&lobby.game_info)
                    else {
                        return;
                    };
                    let valid = revision == required_revision
                        && valid_mission_status(&status)
                        && verified_bytes <= total_bytes
                        && total_bytes == required_size
                        && attempt <= 100
                        && transfer_id.as_ref().is_none_or(|v| v.len() <= 64)
                        && failure_code.as_ref().is_none_or(|v| v.len() <= 64);
                    if !valid {
                        let _ = tx.send(ServerMessage::Error {
                            code: "INVALID_MISSION_STATUS".into(),
                            message: "Invalid or stale mission status".into(),
                        });
                        return;
                    }
                    if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id) {
                        let progression_valid = p.mission_status.as_ref().is_none_or(|previous| {
                            attempt >= previous.attempt
                                && (attempt > previous.attempt
                                    || transfer_id != previous.transfer_id
                                    || verified_bytes >= previous.verified_bytes)
                        });
                        if !progression_valid {
                            let _ = tx.send(ServerMessage::Error {
                                code: "INVALID_MISSION_STATUS".into(),
                                message: "Mission progress moved backwards".into(),
                            });
                            return;
                        }
                        p.ready &= status == "match";
                        p.mission_status = Some(MissionStatusReport {
                            revision,
                            status,
                            verified_bytes,
                            total_bytes,
                            transfer_id,
                            attempt,
                            failure_code,
                            game_data_warning,
                        });
                    }
                    broadcast_lobby_update(&lobby, state);
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
                    lobby.expire_connectivity_attempt(Instant::now());
                    if !matches!(lobby.state, LobbyState::Waiting | LobbyState::Holepunching) {
                        let _ = tx.send(ServerMessage::Error {
                            code: "GAME_ALREADY_STARTED".into(),
                            message: "The game has already started".into(),
                        });
                        return;
                    }
                    if lobby.players.len() < 2 {
                        let _ = tx.send(ServerMessage::Error {
                            code: "NOT_ENOUGH_PLAYERS".into(),
                            message: "At least two players are required".into(),
                        });
                        return;
                    }
                    if mission_requirement(&lobby.game_info).is_some()
                        && lobby.players.iter().any(|p| {
                            !p.ready
                                || p.mission_status.as_ref().map(|s| s.status.as_str())
                                    != Some("match")
                        })
                    {
                        let _ = tx.send(ServerMessage::Error {
                            code: "MISSION_NOT_READY".into(),
                            message: "Every player must have the matching mission and be ready"
                                .into(),
                        });
                        return;
                    }
                    lobby.state = LobbyState::Starting;
                    lobby.game_epoch = Uuid::new_v4();
                    state.stats.game_started();
                    info!(%lobby_id, "game starting");

                    // Send connection type info to all players
                    send_connection_info(&lobby, state);

                    // Determine host address from candidates
                    let host_addr = lobby
                        .players
                        .iter()
                        .find(|p| p.player_id == player_id)
                        .map(|p| best_candidate_addr(&p.candidates))
                        .unwrap_or_default();

                    let game = lobby.game.clone();
                    let game_info = lobby.game_info.clone();
                    let max_players = lobby.max_players;

                    // Collect relay-needed pairs and allocate sessions BEFORE
                    // building GAME_STARTING so relay info is available.
                    // relay_tokens maps (slot_a, slot_b) -> token
                    let relay_addr = state.config.relay_public_addr.clone();
                    let mut relay_tokens: std::collections::HashMap<(u8, u8), u32> =
                        std::collections::HashMap::new();
                    let mut direct_addresses = std::collections::HashMap::new();
                    let mut relay_limit_hit = false;
                    for i in 0..lobby.players.len() {
                        for j in (i + 1)..lobby.players.len() {
                            let (conn_type, _) =
                                determine_connection_type(&lobby.players[i], &lobby.players[j]);
                            // Apply same unconfirmed-DirectLan downgrade
                            let confirmed_direct = |ct: &crate::lobby::ConnectionType| {
                                !matches!(
                                    ct,
                                    crate::lobby::ConnectionType::Unknown
                                        | crate::lobby::ConnectionType::Relay
                                )
                            };
                            let force_or_downgrade_to_relay = state.config.force_relay
                                || (conn_type == crate::lobby::ConnectionType::DirectLan
                                    && !confirmed_direct(&lobby.players[i].connection_type)
                                    && !confirmed_direct(&lobby.players[j].connection_type));
                            let addr_i =
                                validated_remote_addr(&lobby.players[j], &lobby.players[i]);
                            let addr_j =
                                validated_remote_addr(&lobby.players[i], &lobby.players[j]);
                            let requires_relay = force_or_downgrade_to_relay
                                || conn_type == crate::lobby::ConnectionType::Relay
                                || addr_i.is_empty()
                                || addr_j.is_empty();
                            direct_addresses.insert((i as u8, j as u8), (addr_i, addr_j));
                            if requires_relay {
                                let pair = RelayPair {
                                    slot_a: lobby.players[i].slot,
                                    slot_b: lobby.players[j].slot,
                                    pid_a: lobby.players[i].player_id,
                                    pid_b: lobby.players[j].player_id,
                                };
                                match allocate_relay_session(state, &relay_addr, lobby_id, &pair) {
                                    Some(token) => {
                                        relay_tokens.insert((i as u8, j as u8), token);
                                    }
                                    None => {
                                        relay_limit_hit = true;
                                    }
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
                            // Each unordered pair was resolved and allocated once above
                            let (lo, hi) = if my_slot < other_slot {
                                (my_slot as u8, other_slot as u8)
                            } else {
                                (other_slot as u8, my_slot as u8)
                            };
                            let relay_token = relay_tokens.get(&(lo, hi)).copied();
                            let is_relay = relay_token.is_some();
                            let addresses = &direct_addresses[&(lo, hi)];
                            let addr = if is_relay {
                                relay_addr.clone()
                            } else if my_slot < other_slot {
                                addresses.1.clone()
                            } else {
                                addresses.0.clone()
                            };
                            let relay_dest_slot = if is_relay { Some(other.slot) } else { None };

                            peers.push(crate::protocol::PeerAssignment {
                                slot: other.slot,
                                addr,
                                is_relay,
                                relay_token,
                                relay_dest_slot,
                                relay_credentials: relay_credentials(
                                    state,
                                    relay_token,
                                    lobby.players[my_slot].slot,
                                ),
                            });
                        }

                        per_player_msgs.push((
                            my_pid,
                            ServerMessage::GameStarting {
                                game_epoch: lobby.game_epoch,
                                host_addr: peers
                                    .iter()
                                    .find(|peer| peer.slot == 0)
                                    .map(|peer| peer.addr.clone())
                                    .unwrap_or_else(|| host_addr.clone()),
                                game: game.clone(),
                                your_slot: lobby.players[my_slot].slot,
                                max_players,
                                current_level: None,
                                game_info: game_info.clone(),
                                peers,
                            },
                        ));
                    }

                    // Update presence for all players
                    let player_ids: Vec<Uuid> = lobby.players.iter().map(|p| p.player_id).collect();
                    let lobby_mission = game_info_str(&lobby.game_info, "mission");
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
            if candidates.len() > 16
                || nat_type.len() > 32
                || candidates.iter().any(|c| {
                    c.addr.len() > 128
                        || !matches!(
                            c.candidate_type.as_str(),
                            "host" | "srflx" | "upnp" | "predicted"
                        )
                        || c.addr.parse::<SocketAddr>().map_or(true, |a| {
                            a.port() == 0 || a.ip().is_unspecified() || a.ip().is_multicast()
                        })
                })
            {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_CANDIDATES".into(),
                    message: "Invalid or excessive candidates".into(),
                });
                return;
            }
            info!(%player_id, %nat_type, candidate_count = candidates.len(), "STUN result received");
            let session_info = state.sessions.get(&player_id).map(|s| {
                (
                    s.public_ip,
                    s.lobby_id,
                    matches!(s.presence, Presence::InLobby { .. }),
                )
            });
            if let Some((ws_ip, Some(lobby_id), awaiting_game)) = session_info {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    // Generate predicted port candidates for symmetric NATs
                    let predicted = generate_predicted_candidates(&candidates, &nat_type);
                    let mut all_candidates = candidates.clone();
                    all_candidates.extend(predicted);

                    // Inject "observed" candidates: server-known public IP + each
                    // host candidate's port. If the player port-forwarded, these
                    // addresses will be directly reachable. The connectivity check
                    // will verify. Skip if we already have a matching srflx.
                    let observed = generate_observed_candidates(&all_candidates, ws_ip);
                    if !observed.is_empty() {
                        info!(
                            %player_id,
                            count = observed.len(),
                            "injected observed candidates from WS IP"
                        );
                        all_candidates.extend(observed);
                    }

                    for player in &mut lobby.players {
                        if player.player_id == player_id {
                            player.connectivity_checks.clear();
                            player.validated_routes.clear();
                        } else {
                            player
                                .connectivity_checks
                                .retain(|_, pair| pair.peer_id != player_id);
                            player.validated_routes.remove(&player_id);
                        }
                    }
                    if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id) {
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
                        Some(build_connectivity_check_messages(&mut lobby))
                    } else {
                        None
                    };

                    // Relay admission must not depend on unavailable direct candidates
                    let relay_late_join = awaiting_game
                        && lobby.state == LobbyState::InGame
                        && player_id != lobby.host_player_id
                        && (state.config.force_relay
                            || lobby
                                .players
                                .iter()
                                .find(|p| p.player_id == lobby.host_player_id)
                                .is_none_or(|p| p.candidates.is_empty())
                            || all_candidates.is_empty());

                    // Late-join ICE: if lobby is InGame and the sender just got
                    // candidates, and the host has candidates, start pairwise ICE
                    // between the joiner and host only.
                    let late_join_msgs = if awaiting_game
                        && !relay_late_join
                        && lobby.state == LobbyState::InGame
                        && player_id != lobby.host_player_id
                    {
                        let host_id = lobby.host_player_id;
                        // Check both have candidates before borrowing players
                        let both_have_candidates = lobby
                            .players
                            .iter()
                            .find(|p| p.player_id == host_id)
                            .map(|h| !h.candidates.is_empty())
                            .unwrap_or(false)
                            && lobby
                                .players
                                .iter()
                                .find(|p| p.player_id == player_id)
                                .map(|j| !j.candidates.is_empty())
                                .unwrap_or(false);
                        if both_have_candidates {
                            // Now safe to borrow players immutably
                            let h = lobby
                                .players
                                .iter()
                                .find(|p| p.player_id == host_id)
                                .unwrap();
                            let j = lobby
                                .players
                                .iter()
                                .find(|p| p.player_id == player_id)
                                .unwrap();
                            let mut pairs: Vec<CandidatePair> = Vec::new();
                            for local_cand in &j.candidates {
                                for remote_cand in &h.candidates {
                                    pairs.push(CandidatePair {
                                        check_id: Uuid::new_v4(),
                                        peer_id: host_id,
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
                            pairs.sort_by_key(|p| std::cmp::Reverse(p.priority));
                            let probe_addrs: Vec<String> =
                                j.candidates.iter().map(|c| c.addr.clone()).collect();
                            let joiner_callsign = j.callsign.clone();
                            let player = lobby
                                .players
                                .iter_mut()
                                .find(|p| p.player_id == player_id)
                                .unwrap();
                            player
                                .connectivity_checks
                                .extend(pairs.iter().map(|pair| (pair.check_id, pair.clone())));
                            info!(
                                %player_id, %host_id, pairs = pairs.len(),
                                "late-join ICE: sending probes"
                            );
                            Some((host_id, pairs, probe_addrs, joiner_callsign))
                        } else {
                            None
                        }
                    } else {
                        None
                    };

                    drop(lobby);
                    for (pid, msg) in peer_msgs {
                        if let Some(sess) = state.sessions.get(&pid) {
                            let _ = sess.tx.send(msg);
                        }
                    }
                    if relay_late_join {
                        complete_late_join(state, player_id, lobby_id, tx);
                        return;
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
                    // Send late-join ICE messages
                    if let Some((host_id, pairs, probe_addrs, joiner_callsign)) = late_join_msgs {
                        // CONNECTIVITY_CHECK_GO to joiner
                        if let Some(sess) = state.sessions.get(&player_id) {
                            let _ = sess
                                .tx
                                .send(ServerMessage::ConnectivityCheckGo { peer_addrs: pairs });
                        }
                        // LATE_JOIN_PROBE to host
                        if let Some(sess) = state.sessions.get(&host_id) {
                            let _ = sess.tx.send(ServerMessage::LateJoinProbe {
                                joiner_id: player_id,
                                joiner_callsign,
                                probe_addrs,
                            });
                        }
                        info!(
                            %lobby_id, joiner = %player_id,
                            "late-join ICE initiated"
                        );
                    }
                }
            }
        }

        ClientMessage::ConnectivityOk {
            peer_id: peer,
            check_id,
            winning_candidate_type,
            rtt_ms,
        } => {
            // Record connectivity result and update connection_type for the pair
            let session_room = state.sessions.get(&player_id).map(|s| s.lobby_id);
            if let Some(Some(lobby_id)) = session_room {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    if !accept_connectivity_report(
                        &mut lobby,
                        player_id,
                        peer,
                        check_id,
                        &winning_candidate_type,
                        rtt_ms,
                    ) {
                        return;
                    }
                    info!(%player_id, %peer, %winning_candidate_type, rtt_ms, "connectivity ok");

                    drop(lobby);
                    complete_late_join(state, player_id, lobby_id, tx);
                }
            }
        }

        ClientMessage::ConnectivityUpdate {
            peer_id: peer,
            new_method,
            detail,
        } => {
            // Update connection_type for an existing peer pair mid-session
            let session_room = state.sessions.get(&player_id).map(|s| s.lobby_id);
            if let Some(Some(lobby_id)) = session_room {
                if let Some(mut lobby) = state.lobbies.get_mut(&lobby_id) {
                    let conn_type = match new_method.as_str() {
                        "direct_lan" => crate::lobby::ConnectionType::DirectLan,
                        "direct_upnp" => crate::lobby::ConnectionType::DirectUpnp,
                        "direct_holepunch" => crate::lobby::ConnectionType::DirectHolepunch,
                        "predicted_holepunch" => crate::lobby::ConnectionType::PredictedHolepunch,
                        "relay" => crate::lobby::ConnectionType::Relay,
                        _ => crate::lobby::ConnectionType::Unknown,
                    };
                    if let Some(p) = lobby.players.iter_mut().find(|p| p.player_id == player_id) {
                        p.connection_type = conn_type;
                    }
                    info!(%player_id, %peer, %new_method, ?detail, "connectivity update");
                    // Refresh CONNECTION_INFO for all players
                    send_connection_info(&lobby, state);
                }
            }
        }

        ClientMessage::FileHashes { hashes } => {
            // Compare against host's hashes and report mismatches
            let session_room = state.sessions.get(&player_id).map(|s| s.lobby_id);
            if let Some(Some(lobby_id)) = session_room {
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

        ClientMessage::KickPlayer { player_id: target } => {
            let lobby_id = state.sessions.get(&player_id).and_then(|s| s.lobby_id);
            if let Some(lobby_id) = lobby_id {
                let is_host = state
                    .lobbies
                    .get(&lobby_id)
                    .map(|l| {
                        l.host_player_id == player_id
                            && target != player_id
                            && l.players.iter().any(|p| p.player_id == target)
                            && state
                                .sessions
                                .get(&target)
                                .is_some_and(|s| s.lobby_id == Some(lobby_id))
                    })
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
                    crate::relay::revoke_player(state, target);
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

            let active = state.lobbies.get(&lobby_id).is_some_and(|room| {
                matches!(room.state, LobbyState::Starting | LobbyState::InGame)
            });
            if !active {
                let _ = tx.send(ServerMessage::Error {
                    code: "GAME_NOT_STARTED".into(),
                    message: "Only an active game can report results".into(),
                });
                return;
            }

            let match_id = Uuid::new_v4();
            let mode = state
                .lobbies
                .get(&lobby_id)
                .map(|l| game_info_str(&l.game_info, "mode"))
                .unwrap_or_default();
            let mission = state
                .lobbies
                .get(&lobby_id)
                .map(|l| game_info_str(&l.game_info, "mission"))
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

            close_room(
                state,
                lobby_id,
                player_id,
                "LOBBY_CLOSED",
                "The game has ended",
            );

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
            if state.db.accept_friend(&player_id, &friend_id).is_err() {
                let _ = tx.send(ServerMessage::Error {
                    code: "INVALID_REQUEST".into(),
                    message: "Pending friend request required".into(),
                });
                return;
            }
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
            let accepted = |a: &Uuid, b: &Uuid| {
                state.db.get_friends(a).is_ok_and(|friends| {
                    friends
                        .iter()
                        .any(|f| f.friend_id == *b && f.status == "accepted")
                })
            };
            if !accepted(&player_id, &friend_player_id) || !accepted(&friend_player_id, &player_id)
            {
                let _ = tx.send(ServerMessage::JoinFriendGameResp {
                    success: false,
                    reason: Some("Accepted friendship required".into()),
                    lobby_id: None,
                });
                return;
            }
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
                        if state.sessions.get(&player_id).and_then(|s| s.lobby_id) != Some(lobby_id)
                        {
                            leave_room(state, player_id);
                        }
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
        ClientMessage::Authenticate { .. }
        | ClientMessage::PowSolution { .. }
        | ClientMessage::InstallationProof { .. } => {}
    }
}

#[cfg(test)]
mod tests {
    #[tokio::test]
    async fn bulk_reservations_release_on_receive_and_closed_queue() {
        let global = Arc::new(tokio::sync::Semaphore::new(2 * 1024 * 1024));
        let connection = Arc::new(tokio::sync::Semaphore::new(2 * 1024 * 1024));
        let (tx, mut rx) = tokio::sync::mpsc::channel(256);
        let sender = BoundedSender {
            tx,
            retired: Arc::new(tokio::sync::Notify::new()),
            global_bytes: global.clone(),
            connection_bytes: connection.clone(),
            critical_bytes: Arc::new(tokio::sync::Semaphore::new(64 * 1024)),
        };
        let message = || ServerMessage::LobbyList {
            lobbies: vec![],
            listing_id: Uuid::new_v4(),
            reset: true,
            next_cursor: None,
        };
        sender.send(message()).unwrap();
        assert!(global.available_permits() < 2 * 1024 * 1024);
        assert_eq!(global.available_permits(), connection.available_permits());
        drop(rx.recv().await.unwrap());
        assert_eq!(global.available_permits(), 2 * 1024 * 1024);
        sender.send(message()).unwrap();
        drop(rx);
        assert!(sender.send(message()).is_err());
        assert_eq!(global.available_permits(), 2 * 1024 * 1024);
        assert_eq!(connection.available_permits(), 2 * 1024 * 1024);
    }

    #[tokio::test]
    async fn shared_bulk_exhaustion_preserves_all_supported_hosts() {
        use futures_util::FutureExt;
        let global = Arc::new(tokio::sync::Semaphore::new(0));
        for _ in 0..500 {
            let (tx, mut rx) = tokio::sync::mpsc::channel(256);
            let retired = Arc::new(tokio::sync::Notify::new());
            let sender = BoundedSender {
                tx,
                retired: retired.clone(),
                global_bytes: global.clone(),
                connection_bytes: Arc::new(tokio::sync::Semaphore::new(2 * 1024 * 1024)),
                critical_bytes: Arc::new(tokio::sync::Semaphore::new(64 * 1024)),
            };
            assert!(sender
                .send(ServerMessage::LobbyList {
                    lobbies: vec![],
                    listing_id: Uuid::new_v4(),
                    reset: true,
                    next_cursor: None
                })
                .is_err());
            assert!(rx.recv().await.unwrap().text.contains("RATE_LIMITED"));
            sender
                .send(ServerMessage::LobbyUpdate {
                    lobby_id: Uuid::new_v4(),
                    players: vec![],
                })
                .unwrap();
            assert!(rx.recv().await.unwrap().text.contains("LOBBY_UPDATE"));
            assert!(retired.notified().now_or_never().is_none());
            assert_eq!(sender.critical_bytes.available_permits(), 64 * 1024);
        }
    }

    use super::*;
    use crate::lobby::{ConnectionType, LobbyPlayer};
    use crate::protocol::ConnectionCandidate;
    use uuid::Uuid;

    #[test]
    fn test_mission_content_identity_for_builtin_and_wrapper() {
        for kind in ["builtin", "wrapper"] {
            let mut info = serde_json::json!({
                "game": "d2", "mission": "custom",
                "mission_requirement": {
                    "schema": 1, "revision": "identity", "game": "d2", "mission_key": "custom",
                    "display_name": "Custom", "kind": kind,
                    "size_bytes": 42, "sha256": "ab".repeat(32), "offer_available": false
                }
            });
            if kind == "wrapper" {
                info["mission_requirement"]["wrapper_filename"] = serde_json::json!("custom.zip");
            }
            assert!(validate_mission_requirement(&info).is_ok());
            for invalid_hash in [serde_json::Value::Null, serde_json::json!("invalid")] {
                let mut changed = info.clone();
                changed["mission_requirement"]["sha256"] = invalid_hash;
                assert!(validate_mission_requirement(&changed).is_err());
            }
            for invalid_size in [0, -1] {
                let mut changed = info.clone();
                changed["mission_requirement"]["size_bytes"] = serde_json::json!(invalid_size);
                assert!(validate_mission_requirement(&changed).is_err());
            }
        }
    }

    #[test]
    fn connectivity_offers_bind_reporter_peer_endpoint_and_attempt() {
        let a = make_player(
            vec![("host", "10.0.2.15:41001"), ("host", "10.0.2.16:41001")],
            None,
        );
        let b = make_player(
            vec![("host", "10.0.2.15:41002"), ("host", "10.0.2.17:41002")],
            None,
        );
        let (aid, bid) = (a.player_id, b.player_id);
        let mut lobby = Lobby::new(
            aid,
            "host".into(),
            "d2".into(),
            4,
            None,
            false,
            serde_json::json!({}),
        );
        lobby.players = vec![a, b];
        lobby.state = LobbyState::Holepunching;
        lobby.holepunch_started_at = Some(Instant::now());
        build_connectivity_check_messages(&mut lobby);
        let offer = lobby.players[0]
            .connectivity_checks
            .values()
            .find(|p| p.remote_addr == "10.0.2.17:41002")
            .unwrap()
            .clone();
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            Uuid::new_v4(),
            "host",
            1
        ));
        assert!(!accept_connectivity_report(
            &mut lobby,
            bid,
            aid,
            offer.check_id,
            "host",
            1
        ));
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            aid,
            offer.check_id,
            "host",
            1
        ));
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            offer.check_id,
            "srflx",
            1
        ));
        assert!(accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            offer.check_id,
            "host",
            1
        ));
        assert_eq!(
            validated_remote_addr(&lobby.players[0], &lobby.players[1]),
            "10.0.2.17:41002"
        );
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            offer.check_id,
            "relay",
            1
        ));
        build_connectivity_check_messages(&mut lobby);
        assert!(validated_remote_addr(&lobby.players[0], &lobby.players[1]).is_empty());
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            offer.check_id,
            "host",
            1
        ));
        let fresh = lobby.players[0]
            .connectivity_checks
            .values()
            .find(|p| p.remote_addr == "10.0.2.17:41002")
            .unwrap()
            .clone();
        lobby.players[1].candidates[1].addr = "10.0.2.18:41002".into();
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            fresh.check_id,
            "host",
            1
        ));
        lobby.remove_player(&bid);
        lobby.state = LobbyState::Waiting;
        assert!(lobby.add_player(bid, "replacement".into()));
        lobby.state = LobbyState::Holepunching;
        lobby.holepunch_started_at = Some(Instant::now());
        lobby.players[1].candidates = vec![ConnectionCandidate {
            candidate_type: "host".into(),
            addr: fresh.remote_addr.clone(),
        }];
        assert!(!accept_connectivity_report(
            &mut lobby,
            aid,
            bid,
            fresh.check_id,
            "host",
            1
        ));
        assert!(validated_remote_addr(&lobby.players[0], &lobby.players[1]).is_empty());
    }

    #[test]
    fn expired_connectivity_attempt_rejects_delayed_report_and_allows_fresh_attempt() {
        let a = make_player(vec![("host", "10.0.2.16:41001")], None);
        let b = make_player(vec![("host", "10.0.2.17:41002")], None);
        let (aid, bid) = (a.player_id, b.player_id);
        let mut lobby = Lobby::new(
            aid,
            "host".into(),
            "d2".into(),
            4,
            None,
            false,
            serde_json::json!({}),
        );
        lobby.players = vec![a, b];
        lobby.state = LobbyState::Holepunching;
        lobby.holepunch_started_at = Some(Instant::now());
        build_connectivity_check_messages(&mut lobby);
        let accepted = *lobby.players[0].connectivity_checks.keys().next().unwrap();
        let delayed = *lobby.players[1].connectivity_checks.keys().next().unwrap();
        assert!(!lobby.expire_connectivity_attempt(Instant::now()));
        assert!(accept_connectivity_report(
            &mut lobby, aid, bid, accepted, "host", 1
        ));
        assert!(!validated_remote_addr(&lobby.players[0], &lobby.players[1]).is_empty());
        lobby.holepunch_started_at = Some(Instant::now() - crate::lobby::HOLEPUNCH_TIMEOUT);
        // The worker has not run: the report path must fence the expired attempt itself
        assert!(!accept_connectivity_report(
            &mut lobby, bid, aid, delayed, "host", 1
        ));
        assert_eq!(lobby.state, LobbyState::Waiting);
        assert!(lobby.holepunch_started_at.is_none());
        for player in &lobby.players {
            assert!(player.connectivity_checks.is_empty());
            assert!(player.validated_routes.is_empty());
            assert_eq!(player.connection_type, ConnectionType::Unknown);
            assert_eq!(player.ping_ms, None);
        }
        // A late report still cannot resurrect the completed timeout transition
        assert!(!accept_connectivity_report(
            &mut lobby, bid, aid, delayed, "host", 1
        ));
        lobby.state = LobbyState::Holepunching;
        lobby.holepunch_started_at = Some(Instant::now());
        build_connectivity_check_messages(&mut lobby);
        let fresh = *lobby.players[1].connectivity_checks.keys().next().unwrap();
        assert!(!accept_connectivity_report(
            &mut lobby, bid, aid, delayed, "host", 1
        ));
        assert!(accept_connectivity_report(
            &mut lobby, bid, aid, fresh, "host", 1
        ));
        assert_eq!(
            validated_remote_addr(&lobby.players[1], &lobby.players[0]),
            "10.0.2.16:41001"
        );
        // The worker uses the same transition to retire both accepted and unused offers
        let expiry = lobby.holepunch_started_at.unwrap() + crate::lobby::HOLEPUNCH_TIMEOUT;
        assert!(lobby.expire_connectivity_attempt(expiry));
        assert!(lobby
            .players
            .iter()
            .all(|p| p.connectivity_checks.is_empty() && p.validated_routes.is_empty()));
    }

    fn make_player(candidates: Vec<(&str, &str)>, nat_type: Option<&str>) -> LobbyPlayer {
        LobbyPlayer {
            slot: 0,
            player_id: Uuid::new_v4(),
            callsign: "test".into(),
            ready: false,
            candidates: candidates
                .into_iter()
                .map(|(t, a)| ConnectionCandidate {
                    candidate_type: t.into(),
                    addr: a.into(),
                })
                .collect(),
            connectivity_checks: std::collections::HashMap::new(),
            validated_routes: std::collections::HashMap::new(),
            nat_type: nat_type.map(|s| s.into()),
            connection_type: ConnectionType::Unknown,
            ping_ms: None,
            mission_status: None,
        }
    }

    // -- ips_share_private_subnet tests --

    #[test]
    fn test_same_192_168_subnet() {
        assert!(ips_share_private_subnet("192.168.1.10", "192.168.1.20"));
        assert!(ips_share_private_subnet("192.168.1.10", "192.168.99.20"));
    }

    #[test]
    fn test_same_10_subnet() {
        assert!(ips_share_private_subnet("10.0.1.5", "10.0.2.99"));
        assert!(ips_share_private_subnet("10.255.0.1", "10.0.0.1"));
    }

    #[test]
    fn test_same_172_16_subnet() {
        assert!(ips_share_private_subnet("172.16.5.1", "172.16.5.2"));
        // Different /16 within 172.16-31 range
        assert!(!ips_share_private_subnet("172.16.5.1", "172.17.5.2"));
    }

    #[test]
    fn test_different_private_ranges() {
        assert!(!ips_share_private_subnet("192.168.1.1", "10.0.0.1"));
        assert!(!ips_share_private_subnet("172.16.0.1", "192.168.0.1"));
    }

    #[test]
    fn test_public_ips_not_shared() {
        assert!(!ips_share_private_subnet("8.8.8.8", "8.8.4.4"));
        assert!(!ips_share_private_subnet("203.0.113.1", "198.51.100.1"));
    }

    #[test]
    fn test_invalid_ips() {
        assert!(!ips_share_private_subnet("not-an-ip", "192.168.1.1"));
        assert!(!ips_share_private_subnet("", ""));
    }

    // -- determine_connection_type tests --

    #[test]
    fn test_both_srflx_same_ip_is_lan() {
        let a = make_player(vec![("srflx", "1.2.3.4:5000")], Some("full_cone"));
        let b = make_player(vec![("srflx", "1.2.3.4:6000")], Some("full_cone"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectLan);
    }

    #[test]
    fn test_both_cone_srflx_different_ip_is_holepunch() {
        let a = make_player(vec![("srflx", "1.2.3.4:5000")], Some("full_cone"));
        let b = make_player(vec![("srflx", "5.6.7.8:6000")], Some("full_cone"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectHolepunch);
    }

    #[test]
    fn test_both_symmetric_no_predicted_is_relay() {
        let a = make_player(vec![("srflx", "1.2.3.4:5000")], Some("symmetric"));
        let b = make_player(vec![("srflx", "5.6.7.8:6000")], Some("symmetric"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::Relay);
    }

    #[test]
    fn test_host_only_same_subnet_is_lan() {
        let a = make_player(vec![("host", "192.168.1.10:5000")], Some("no_stun"));
        let b = make_player(vec![("host", "192.168.1.20:6000")], Some("no_stun"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectLan);
    }

    #[test]
    fn test_host_only_different_subnet_is_holepunch() {
        let a = make_player(vec![("host", "192.168.1.10:5000")], Some("no_stun"));
        let b = make_player(vec![("host", "10.0.0.5:6000")], Some("no_stun"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectHolepunch);
    }

    #[test]
    fn test_no_candidates_is_relay() {
        let a = make_player(vec![], Some("unknown"));
        let b = make_player(vec![], Some("unknown"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::Relay);
    }

    #[test]
    fn test_upnp_candidate_is_direct_upnp() {
        let a = make_player(
            vec![("host", "192.168.1.10:5000"), ("upnp", "1.2.3.4:5000")],
            Some("full_cone"),
        );
        let b = make_player(vec![("srflx", "5.6.7.8:6000")], Some("full_cone"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectUpnp);
    }

    // -- best_candidate_addr tests --

    #[test]
    fn test_best_addr_prefers_srflx() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.1:5000".into(),
            },
            ConnectionCandidate {
                candidate_type: "srflx".into(),
                addr: "1.2.3.4:5000".into(),
            },
        ];
        assert_eq!(best_candidate_addr(&candidates), "1.2.3.4:5000");
    }

    #[test]
    fn test_best_addr_falls_back_to_host() {
        let candidates = vec![ConnectionCandidate {
            candidate_type: "host".into(),
            addr: "192.168.1.1:5000".into(),
        }];
        assert_eq!(best_candidate_addr(&candidates), "192.168.1.1:5000");
    }

    #[test]
    fn test_best_addr_empty_on_no_candidates() {
        let candidates: Vec<ConnectionCandidate> = vec![];
        assert_eq!(best_candidate_addr(&candidates), "");
    }

    // -- best_candidate_addr_for_type tests --

    #[test]
    fn test_best_addr_for_lan_prefers_host() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.10:42424".into(),
            },
            ConnectionCandidate {
                candidate_type: "srflx".into(),
                addr: "203.0.113.5:42424".into(),
            },
        ];
        assert_eq!(
            best_candidate_addr_for_type(&candidates, &ConnectionType::DirectLan),
            "192.168.1.10:42424"
        );
    }

    #[test]
    fn test_best_addr_for_lan_falls_back_to_observed() {
        // No host candidate -- LAN should fall back to observed
        let candidates = vec![ConnectionCandidate {
            candidate_type: "observed".into(),
            addr: "203.0.113.5:42424".into(),
        }];
        assert_eq!(
            best_candidate_addr_for_type(&candidates, &ConnectionType::DirectLan),
            "203.0.113.5:42424"
        );
    }

    #[test]
    fn test_best_addr_for_upnp_prefers_upnp() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.10:42424".into(),
            },
            ConnectionCandidate {
                candidate_type: "srflx".into(),
                addr: "203.0.113.5:42424".into(),
            },
            ConnectionCandidate {
                candidate_type: "upnp".into(),
                addr: "203.0.113.5:5000".into(),
            },
        ];
        assert_eq!(
            best_candidate_addr_for_type(&candidates, &ConnectionType::DirectUpnp),
            "203.0.113.5:5000"
        );
    }

    #[test]
    fn test_best_addr_for_holepunch_prefers_srflx() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.10:42424".into(),
            },
            ConnectionCandidate {
                candidate_type: "srflx".into(),
                addr: "203.0.113.5:42424".into(),
            },
        ];
        assert_eq!(
            best_candidate_addr_for_type(&candidates, &ConnectionType::DirectHolepunch),
            "203.0.113.5:42424"
        );
    }

    // -- observed candidate tests --

    #[test]
    fn test_generate_observed_from_host_candidates() {
        let candidates = vec![ConnectionCandidate {
            candidate_type: "host".into(),
            addr: "192.168.1.10:42424".into(),
        }];
        let ws_ip: std::net::IpAddr = "203.0.113.5".parse().unwrap();
        let observed = generate_observed_candidates(&candidates, ws_ip);
        assert_eq!(observed.len(), 1);
        assert_eq!(observed[0].candidate_type, "observed");
        assert_eq!(observed[0].addr, "203.0.113.5:42424");
    }

    #[test]
    fn test_observed_skips_existing_srflx() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.10:42424".into(),
            },
            ConnectionCandidate {
                candidate_type: "srflx".into(),
                addr: "203.0.113.5:42424".into(),
            },
        ];
        let ws_ip: std::net::IpAddr = "203.0.113.5".parse().unwrap();
        let observed = generate_observed_candidates(&candidates, ws_ip);
        assert!(observed.is_empty(), "should skip when srflx matches");
    }

    #[test]
    fn test_observed_skips_loopback() {
        let candidates = vec![ConnectionCandidate {
            candidate_type: "host".into(),
            addr: "192.168.1.10:42424".into(),
        }];
        let ws_ip: std::net::IpAddr = "127.0.0.1".parse().unwrap();
        let observed = generate_observed_candidates(&candidates, ws_ip);
        assert!(observed.is_empty(), "should skip loopback IP");
    }

    #[test]
    fn test_observed_candidate_enables_holepunch() {
        let a = make_player(
            vec![
                ("host", "192.168.1.10:42424"),
                ("observed", "203.0.113.5:42424"),
            ],
            Some("no_stun"),
        );
        let b = make_player(vec![("host", "10.0.0.5:42424")], Some("no_stun"));
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectHolepunch);
    }

    #[test]
    fn test_both_observed_same_ip_is_lan() {
        let a = make_player(
            vec![
                ("host", "192.168.1.10:42424"),
                ("observed", "203.0.113.5:42424"),
            ],
            Some("no_stun"),
        );
        let b = make_player(
            vec![
                ("host", "192.168.1.20:42424"),
                ("observed", "203.0.113.5:42424"),
            ],
            Some("no_stun"),
        );
        let (ct, _) = determine_connection_type(&a, &b);
        assert_eq!(ct, ConnectionType::DirectLan);
    }

    #[test]
    fn test_best_addr_prefers_observed_over_host() {
        let candidates = vec![
            ConnectionCandidate {
                candidate_type: "host".into(),
                addr: "192.168.1.1:5000".into(),
            },
            ConnectionCandidate {
                candidate_type: "observed".into(),
                addr: "203.0.113.5:5000".into(),
            },
        ];
        assert_eq!(best_candidate_addr(&candidates), "203.0.113.5:5000");
    }

    #[test]
    fn test_candidate_pair_priority_observed() {
        // observed should be prioritized between srflx and predicted
        let obs = candidate_pair_priority("observed", "host");
        let srflx = candidate_pair_priority("srflx", "host");
        let predicted = candidate_pair_priority("predicted", "predicted");
        assert!(obs < srflx, "observed < srflx");
        assert!(obs > predicted, "observed > predicted");
    }
}
