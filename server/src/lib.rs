pub mod config;
pub mod db;
pub mod friends;
pub mod http_api;
pub mod identity;
pub mod lobby;
pub mod nat_sim;
pub mod pow;
pub mod protocol;
pub mod rate_limit;
pub mod relay;
pub mod stats;
pub mod stun;
pub mod tls;
pub mod ws_handler;

use std::net::IpAddr;
use std::sync::Arc;

/// Shared server state accessible from all tasks.
pub struct ServerState {
    pub config: config::ServerConfig,
    pub lobbies: dashmap::DashMap<uuid::Uuid, lobby::Lobby>,
    pub sessions: dashmap::DashMap<uuid::Uuid, ws_handler::PlayerSession>,
    pub relay_sessions: dashmap::DashMap<u32, relay::RelaySession>,
    pub stats: stats::ServerStats,
    pub db: db::Database,
    pub rate_limiter: rate_limit::RateLimiter,
    pub stun_allowlist: Arc<dashmap::DashSet<IpAddr>>,
    pub started_at: std::time::Instant,
}

/// Build a `ServerState` from a config. Useful for tests.
pub fn build_state(config: config::ServerConfig) -> Result<Arc<ServerState>, rusqlite::Error> {
    let db = db::Database::open(&config.db_path)?;
    Ok(Arc::new(ServerState {
        config,
        lobbies: dashmap::DashMap::new(),
        sessions: dashmap::DashMap::new(),
        relay_sessions: dashmap::DashMap::new(),
        stats: stats::ServerStats::new(),
        db,
        rate_limiter: rate_limit::RateLimiter::new(),
        stun_allowlist: Arc::new(dashmap::DashSet::new()),
        started_at: std::time::Instant::now(),
    }))
}
