pub mod candidate_auth;
pub mod config;
pub mod db;
pub mod friends;
pub mod google_http;
pub mod google_identity;
pub mod google_login;
pub mod http_api;
pub mod lobby;
pub mod nat_sim;
pub mod pow;
pub mod protocol;
pub mod rate_limit;
pub mod relay;
pub mod relay_economy;
pub mod relay_lifecycle;
pub mod resource_budget;
pub mod stats;
pub mod stun;
pub mod tls;
pub mod transport;
pub mod ws_handler;

use std::sync::Arc;

/// Shared server state accessible from all tasks.
pub struct ServerState {
    pub transport_slots: Arc<tokio::sync::Semaphore>,
    pub transport_work: std::sync::Mutex<resource_budget::WorkBudget>,
    pub control_queue_bytes: Arc<tokio::sync::Semaphore>,
    pub control_egress: Arc<resource_budget::FairEgress>,
    pub relay_budget: std::sync::Mutex<resource_budget::RelayBudget>,
    pub relay_economy: std::sync::Mutex<relay_economy::RelayEconomy>,
    pub relay_renewals: std::sync::Mutex<std::collections::BTreeMap<u32, relay_lifecycle::Renewal>>,
    pub auth_work: std::sync::Mutex<resource_budget::WorkBudget>,
    pub auth_slots: tokio::sync::Semaphore,
    pub google_login: Option<google_login::GoogleLogin>,
    pub public_slots: tokio::sync::Semaphore,
    pub public_egress: resource_budget::FairEgress,
    pub public_work: std::sync::Mutex<resource_budget::WorkBudget>,
    pub allocation_work: std::sync::Mutex<resource_budget::AllocationBudget>,
    pub ready_listeners: std::sync::atomic::AtomicU8,
    pub connected_sockets: std::sync::atomic::AtomicUsize,
    /// Serializes short control-plane transitions; never held across network awaits
    pub control: std::sync::Mutex<()>,
    pub config: config::ServerConfig,
    pub lobbies: dashmap::DashMap<uuid::Uuid, lobby::Lobby>,
    pub sessions: dashmap::DashMap<uuid::Uuid, ws_handler::PlayerSession>,
    pub relay_sessions: dashmap::DashMap<u32, relay::RelaySession>,
    pub stats: stats::ServerStats,
    pub db: db::Database,
    pub rate_limiter: rate_limit::RateLimiter,
    pub candidate_attempts: candidate_auth::CandidateAttempts,
    pub started_at: std::time::Instant,
}

/// Build a `ServerState` from a config. Useful for tests.
pub fn build_state(
    config: config::ServerConfig,
) -> Result<Arc<ServerState>, Box<dyn std::error::Error + Send + Sync>> {
    if !config.skip_gpgs_verify
        && (config.max_connections == 0
            || config.max_connections > 500
            || config.max_relay_sessions == 0
            || config.max_relay_sessions > 100)
    {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "Production service capacities must be finite and within supported bounds",
        )
        .into());
    }
    if !config.skip_gpgs_verify
        && (config.db_path.trim().is_empty()
            || config.db_path.eq_ignore_ascii_case(":memory:")
            || config.db_path.to_ascii_lowercase().starts_with("file:"))
    {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "Production requires a persistent SQLite filename, not a temporary database or URI",
        )
        .into());
    }
    if !config.relay_policy.validate()
        || (!config.skip_gpgs_verify && config.relay_policy.allocation_lifetime_seconds < 3600)
    {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "Invalid relay economic limits",
        )
        .into());
    }
    let google_login = if config.google_auth_origin.is_empty() {
        None
    } else {
        Some(
            google_login::GoogleLogin::new(&config.google_auth_origin, &config.google_client_id)
                .map_err(|error| std::io::Error::new(std::io::ErrorKind::InvalidInput, error))?,
        )
    };
    let db = db::Database::open(&config.db_path)?;
    let now = std::time::Instant::now();
    let month = relay_economy::Month::current(now);
    let mut economy = relay_economy::RelayEconomy::new(config.relay_policy.clone(), month, now);
    if let Some((period, limit, wanted)) = economy.request_credit(month) {
        economy.finish_credit(db.reserve_relay_egress(period, limit, wanted));
    }
    Ok(Arc::new(ServerState {
        transport_slots: Arc::new(tokio::sync::Semaphore::new(64)),
        transport_work: std::sync::Mutex::new(resource_budget::WorkBudget::new(128, 1)),
        control_queue_bytes: Arc::new(tokio::sync::Semaphore::new(32 * 1024 * 1024)),
        control_egress: Arc::new(resource_budget::FairEgress::new(
            8 * 1024 * 1024,
            std::time::Duration::from_secs(1),
        )),
        relay_budget: std::sync::Mutex::new(resource_budget::RelayBudget::new()),
        relay_economy: std::sync::Mutex::new(economy),
        relay_renewals: Default::default(),
        auth_work: std::sync::Mutex::new(resource_budget::WorkBudget::new(64, 1)),
        auth_slots: tokio::sync::Semaphore::new(8),
        google_login,
        public_slots: tokio::sync::Semaphore::new(32),
        public_egress: resource_budget::FairEgress::new(
            8 * 1024 * 1024,
            std::time::Duration::from_secs(1),
        ),
        public_work: std::sync::Mutex::new(resource_budget::WorkBudget::new(120, 1)),
        allocation_work: std::sync::Mutex::new(resource_budget::AllocationBudget::new()),
        ready_listeners: std::sync::atomic::AtomicU8::new(0),
        connected_sockets: std::sync::atomic::AtomicUsize::new(0),
        control: std::sync::Mutex::new(()),
        config,
        lobbies: dashmap::DashMap::new(),
        sessions: dashmap::DashMap::new(),
        relay_sessions: dashmap::DashMap::new(),
        stats: stats::ServerStats::new(),
        db,
        rate_limiter: rate_limit::RateLimiter::new(),
        candidate_attempts: candidate_auth::CandidateAttempts::default(),
        started_at: std::time::Instant::now(),
    }))
}

/// Listener lifetime marker, cleared on failure, cancellation or normal exit
pub struct ListenerReady {
    state: Arc<ServerState>,
    bit: u8,
}
impl ListenerReady {
    pub const HTTP: u8 = 1;
    pub const WS: u8 = 2;
    pub const RELAY: u8 = 4;
    pub const STUN1: u8 = 8;
    pub const STUN2: u8 = 16;
    pub const ADMIN: u8 = 32;
    pub fn new(state: Arc<ServerState>, bit: u8) -> Self {
        state
            .ready_listeners
            .fetch_or(bit, std::sync::atomic::Ordering::SeqCst);
        Self { state, bit }
    }
    pub fn required(state: &ServerState) -> u8 {
        Self::HTTP
            | Self::WS
            | Self::RELAY
            | if state.config.stun_public_addrs.is_empty() {
                0
            } else {
                Self::STUN1 | Self::STUN2
            }
            | if state.config.admin_http_listen_addr.is_some() {
                Self::ADMIN
            } else {
                0
            }
    }
}
impl Drop for ListenerReady {
    fn drop(&mut self) {
        self.state
            .ready_listeners
            .fetch_and(!self.bit, std::sync::atomic::Ordering::SeqCst);
    }
}
