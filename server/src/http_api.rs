use std::net::SocketAddr;
use std::sync::Arc;

use axum::{
    extract::{Json, Query, State},
    http::{HeaderMap, StatusCode},
    response::IntoResponse,
    routing::get,
    Router,
};
use serde::{Deserialize, Serialize};
use tracing::{info, warn};

use crate::protocol::{LobbyInfo, CURRENT_PROTOCOL};
use crate::ServerState;

/// Build the public HTTP API router (no admin endpoints).
pub fn public_router(state: Arc<ServerState>) -> Router {
    Router::new()
        .route("/api/v1/status", get(status))
        .route("/api/v1/status/simple", get(status_simple))
        .route("/api/v1/health", get(health))
        .route("/api/v1/ready", get(ready))
        .route("/api/v1/leaderboard", get(leaderboard_stub))
        .route_layer(axum::middleware::from_fn_with_state(
            Arc::clone(&state),
            public_budget,
        ))
        .with_state(state)
}

/// Build the combined public + admin router (used when no separate admin port).
pub fn router(state: Arc<ServerState>) -> Router {
    Router::new()
        .route("/api/v1/status", get(status))
        .route("/api/v1/status/simple", get(status_simple))
        .route("/api/v1/health", get(health))
        .route("/api/v1/ready", get(ready))
        .route("/api/v1/leaderboard", get(leaderboard_stub))
        .route_layer(axum::middleware::from_fn_with_state(
            Arc::clone(&state),
            public_budget,
        ))
        .route("/api/v1/admin/ban", axum::routing::post(admin_ban))
        .route("/api/v1/admin/unban", axum::routing::post(admin_unban))
        .route("/api/v1/admin/motd", axum::routing::post(admin_motd_stub))
        .with_state(state)
}

/// Build admin-only router.
pub fn admin_router(state: Arc<ServerState>) -> Router {
    Router::new()
        .route("/api/v1/admin/ban", axum::routing::post(admin_ban))
        .route("/api/v1/admin/unban", axum::routing::post(admin_unban))
        .route("/api/v1/admin/motd", axum::routing::post(admin_motd_stub))
        .route("/api/v1/health", get(health))
        .route("/api/v1/ready", get(ready))
        .with_state(state)
}

/// Run the public HTTP API server.
pub async fn run(
    addr: SocketAddr,
    state: Arc<ServerState>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let use_public_only = state.config.admin_http_listen_addr.is_some();
    let app = if use_public_only {
        public_router(Arc::clone(&state))
    } else {
        router(Arc::clone(&state))
    };

    let listener = tokio::net::TcpListener::bind(addr).await?;
    let _ready = crate::ListenerReady::new(Arc::clone(&state), crate::ListenerReady::HTTP);
    tracing::info!(%addr, public_only = use_public_only, "HTTP API listening");
    crate::transport::serve(listener, app, state, None).await?;
    Ok(())
}

/// Run the admin-only HTTP API server on a separate port.
pub async fn run_admin(
    addr: SocketAddr,
    state: Arc<ServerState>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let app = admin_router(Arc::clone(&state));
    let listener = tokio::net::TcpListener::bind(addr).await?;
    let _ready = crate::ListenerReady::new(Arc::clone(&state), crate::ListenerReady::ADMIN);
    tracing::info!(%addr, "Admin HTTP API listening");
    crate::transport::serve(listener, app, state, None).await?;
    Ok(())
}

// -- Public endpoints --

#[derive(Serialize)]
struct StatusResponse {
    server_version: &'static str,
    protocol_version: u32,
    uptime_seconds: u64,
    players: PlayerCounts,
    lobbies: LobbySection,
    stats: AggregateStats,
}

#[derive(Serialize)]
struct PlayerCounts {
    online: u32,
    in_game: u32,
    in_lobbies: u32,
    peak_today: u32,
    peak_alltime: u32,
}

#[derive(Serialize)]
struct LobbySection {
    count: u32,
    games: Vec<LobbyInfo>,
    next_cursor: Option<uuid::Uuid>,
}

#[derive(Serialize)]
struct AggregateStats {
    total_unique_players: u64,
    total_games_played: u64,
}

async fn public_budget(
    State(state): State<Arc<ServerState>>,
    request: axum::extract::Request,
    next: axum::middleware::Next,
) -> axum::response::Response {
    if !state
        .public_work
        .lock()
        .unwrap()
        .accept(std::time::Instant::now())
    {
        return StatusCode::TOO_MANY_REQUESTS.into_response();
    }
    let Ok(_slot) = state.public_slots.try_acquire() else {
        return StatusCode::TOO_MANY_REQUESTS.into_response();
    };
    let response = next.run(request).await;
    let (parts, body) = response.into_parts();
    let Ok(bytes) = axum::body::to_bytes(body, 128 * 1024).await else {
        return StatusCode::SERVICE_UNAVAILABLE.into_response();
    };
    state.public_egress.wait(bytes.len()).await;
    axum::response::Response::from_parts(parts, axum::body::Body::from(bytes))
}

#[derive(Deserialize)]
struct LobbyQuery {
    after: Option<uuid::Uuid>,
}
async fn status(
    State(state): State<Arc<ServerState>>,
    Query(query): Query<LobbyQuery>,
) -> Json<StatusResponse> {
    let snap = state.stats.snapshot();

    let (games, next_cursor) = crate::ws_handler::lobby_page(&state, query.after);

    let total_unique = state.db.total_unique_players().unwrap_or(0);
    let total_games = state.db.total_games_played().unwrap_or(0);

    Json(StatusResponse {
        server_version: env!("CARGO_PKG_VERSION"),
        protocol_version: CURRENT_PROTOCOL,
        uptime_seconds: state.started_at.elapsed().as_secs(),
        players: PlayerCounts {
            online: snap.online,
            in_game: snap.in_game,
            in_lobbies: snap.lobbies,
            peak_today: snap.peak_today,
            peak_alltime: snap.peak_alltime,
        },
        lobbies: LobbySection {
            count: snap.lobbies,
            games,
            next_cursor,
        },
        stats: AggregateStats {
            total_unique_players: total_unique,
            total_games_played: total_games,
        },
    })
}

async fn status_simple(State(state): State<Arc<ServerState>>) -> String {
    let online = state.stats.snapshot().online;
    online.to_string()
}

#[derive(Serialize)]
struct HealthResponse {
    status: &'static str,
    websocket_connections: u32,
    relay_sessions: usize,
    lobbies: u32,
}

async fn health(State(state): State<Arc<ServerState>>) -> Json<HealthResponse> {
    let snap = state.stats.snapshot();
    Json(HealthResponse {
        status: "ok",
        websocket_connections: snap.online,
        relay_sessions: state.relay_sessions.len(),
        lobbies: snap.lobbies,
    })
}

async fn leaderboard_stub() -> (StatusCode, &'static str) {
    (
        StatusCode::NOT_IMPLEMENTED,
        "leaderboard not yet implemented",
    )
}

// -- Admin endpoints --

fn check_admin_token(
    headers: &HeaderMap,
    expected: &str,
) -> Result<(), (StatusCode, &'static str)> {
    if expected.is_empty() {
        warn!("admin API called but admin token not configured");
        return Err((StatusCode::SERVICE_UNAVAILABLE, "admin API not configured"));
    }
    let provided = headers
        .get("authorization")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.strip_prefix("Bearer "));
    match provided {
        Some(token) if constant_time_eq(token.as_bytes(), expected.as_bytes()) => Ok(()),
        _ => {
            warn!("admin API authentication failed");
            Err((StatusCode::UNAUTHORIZED, "invalid admin token"))
        }
    }
}

/// Constant-time byte comparison to prevent timing side-channels on token checks.
fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    let mut diff: u8 = 0;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

#[derive(Deserialize)]
struct BanRequest {
    player_id: String,
    reason: String,
    duration_hours: Option<u64>,
}

async fn admin_ban(
    State(state): State<Arc<ServerState>>,
    headers: HeaderMap,
    Json(req): Json<BanRequest>,
) -> impl IntoResponse {
    if let Err(e) = check_admin_token(&headers, &state.config.admin_token) {
        return e.into_response();
    }

    let player_id = match uuid::Uuid::parse_str(&req.player_id) {
        Ok(id) => id,
        Err(_) => return (StatusCode::BAD_REQUEST, "invalid player_id").into_response(),
    };

    let expires = req.duration_hours.map(|h| {
        (chrono::Utc::now() + chrono::Duration::hours(h as i64))
            .format("%Y-%m-%dT%H:%M:%SZ")
            .to_string()
    });

    match state
        .db
        .ban_player(&player_id, &req.reason, expires.as_deref())
    {
        Ok(()) => {
            info!(%player_id, reason = %req.reason, duration_hours = ?req.duration_hours, "player banned");
            (StatusCode::OK, "banned").into_response()
        }
        Err(e) => {
            warn!(%player_id, %e, "ban_player DB error");
            (StatusCode::INTERNAL_SERVER_ERROR, e.to_string()).into_response()
        }
    }
}

#[derive(Deserialize)]
struct UnbanRequest {
    player_id: String,
}

async fn admin_unban(
    State(state): State<Arc<ServerState>>,
    headers: HeaderMap,
    Json(req): Json<UnbanRequest>,
) -> impl IntoResponse {
    if let Err(e) = check_admin_token(&headers, &state.config.admin_token) {
        return e.into_response();
    }

    let player_id = match uuid::Uuid::parse_str(&req.player_id) {
        Ok(id) => id,
        Err(_) => return (StatusCode::BAD_REQUEST, "invalid player_id").into_response(),
    };

    match state.db.unban_player(&player_id) {
        Ok(()) => {
            info!(%player_id, "player unbanned");
            (StatusCode::OK, "unbanned").into_response()
        }
        Err(e) => {
            warn!(%player_id, %e, "unban_player DB error");
            (StatusCode::INTERNAL_SERVER_ERROR, e.to_string()).into_response()
        }
    }
}

async fn admin_motd_stub() -> (StatusCode, &'static str) {
    (
        StatusCode::NOT_IMPLEMENTED,
        "MOTD update not yet implemented",
    )
}

/// Readiness is separate from process liveness; it never exposes addresses or keys
async fn ready(State(state): State<Arc<ServerState>>) -> (StatusCode, Json<serde_json::Value>) {
    let active = state
        .ready_listeners
        .load(std::sync::atomic::Ordering::SeqCst);
    let required = crate::ListenerReady::required(&state);
    let ready = active & required == required;
    (
        if ready {
            StatusCode::OK
        } else {
            StatusCode::SERVICE_UNAVAILABLE
        },
        Json(
            serde_json::json!({ "ready": ready, "protocol_version": CURRENT_PROTOCOL,
            "pending_transports": 64 - state.transport_slots.available_permits(),
            "pending_and_authenticated_sockets": state.connected_sockets.load(std::sync::atomic::Ordering::SeqCst),
            "relay_sessions": state.relay_sessions.len(), "rooms": state.lobbies.len(),
            "pending_native_admissions": state.lobbies.iter().map(|r| r.assigned_late_joiners.len()).sum::<usize>() }),
        ),
    )
}
