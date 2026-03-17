use std::net::SocketAddr;

use serde::Deserialize;

/// Server configuration, loaded from JSON5 config file + environment variable overrides.
#[derive(Clone)]
pub struct ServerConfig {
    /// Address for the WebSocket listener (e.g. "0.0.0.0:9000")
    pub ws_listen_addr: SocketAddr,
    /// Address for the HTTP API listener (e.g. "0.0.0.0:8080")
    pub http_listen_addr: SocketAddr,
    /// Address for the UDP relay listener (e.g. "0.0.0.0:9001")
    pub relay_listen_addr: SocketAddr,
    /// Path to the SQLite database file
    pub db_path: String,
    /// Google OAuth2 client ID for GPGS token verification
    pub google_client_id: String,
    /// Google OAuth2 client secret
    pub google_client_secret: String,
    /// Admin bearer token for /api/v1/admin/* endpoints
    pub admin_token: String,
    /// MOTD displayed to clients on connect (empty = no MOTD)
    pub motd: String,
    /// Public URL shown when rejecting old clients
    pub update_url: String,
    /// Path to TLS certificate PEM file (empty = no TLS)
    pub tls_cert_path: String,
    /// Path to TLS private key PEM file (empty = no TLS)
    pub tls_key_path: String,
    /// Public address of the UDP relay, sent to clients in RELAY_ASSIGNED
    pub relay_public_addr: String,
    /// Address for STUN listener 1 (e.g. "0.0.0.0:3478")
    pub stun_listen_addr: SocketAddr,
    /// Address for STUN listener 2, different port for NAT detection (e.g. "0.0.0.0:3479")
    pub stun_listen_addr_alt: SocketAddr,
    /// Public STUN addresses sent to clients in AUTH_OK (e.g. "203.0.113.5:3478,203.0.113.5:3479")
    /// If empty, STUN listeners are not started.
    pub stun_public_addrs: String,
    /// Directory for log files (empty = no file logging, stdout only)
    pub log_dir: String,
    /// Skip GPGS token verification (dev/test mode: use token as identity key directly)
    pub skip_gpgs_verify: bool,
    /// Proof-of-work difficulty (leading zero bits) for keypair registration.
    /// Default 20 (~200ms on phone, <1us to verify).
    pub pow_difficulty: u8,
    /// Maximum number of concurrent relay sessions. 0 = unlimited.
    pub max_relay_sessions: usize,
}

/// JSON5 config file schema. All fields optional; env vars override file values.
#[derive(Deserialize, Default)]
#[serde(default)]
struct ConfigFile {
    ws_listen_addr: Option<String>,
    http_listen_addr: Option<String>,
    relay_listen_addr: Option<String>,
    db_path: Option<String>,
    google_client_id: Option<String>,
    google_client_secret: Option<String>,
    admin_token: Option<String>,
    motd: Option<String>,
    update_url: Option<String>,
    tls_cert_path: Option<String>,
    tls_key_path: Option<String>,
    relay_public_addr: Option<String>,
    stun_listen_addr: Option<String>,
    stun_listen_addr_alt: Option<String>,
    stun_public_addrs: Option<String>,
    log_dir: Option<String>,
    skip_gpgs_verify: Option<bool>,
    pow_difficulty: Option<u8>,
    max_relay_sessions: Option<usize>,
}

impl ServerConfig {
    /// Load config: read JSON5 file (if present), then overlay environment variables.
    /// File path comes from `CONFIG_FILE` env var (default: `server_config.json5`).
    pub fn load() -> Self {
        let config_path =
            std::env::var("CONFIG_FILE").unwrap_or_else(|_| "server_config.json5".into());
        let file_cfg = match std::fs::read_to_string(&config_path) {
            Ok(contents) => match json5::from_str::<ConfigFile>(&contents) {
                Ok(cfg) => {
                    eprintln!("loaded config from {config_path}");
                    cfg
                }
                Err(e) => {
                    eprintln!("warning: failed to parse {config_path}: {e}");
                    ConfigFile::default()
                }
            },
            Err(_) => ConfigFile::default(),
        };

        // Helper: env var > file value > default
        fn resolve(env_key: &str, file_val: &Option<String>, default: &str) -> String {
            std::env::var(env_key).unwrap_or_else(|_| {
                file_val
                    .as_deref()
                    .unwrap_or(default)
                    .to_string()
            })
        }
        fn resolve_addr(env_key: &str, file_val: &Option<String>, default: &str) -> SocketAddr {
            resolve(env_key, file_val, default)
                .parse()
                .unwrap_or_else(|e| {
                    panic!("{env_key} is not a valid socket address: {e}");
                })
        }

        let skip_env = std::env::var("SKIP_GPGS_VERIFY").ok();
        let skip = match &skip_env {
            Some(v) => v == "true",
            None => file_cfg.skip_gpgs_verify.unwrap_or(false),
        };

        let pow_env = std::env::var("POW_DIFFICULTY").ok();
        let pow = match &pow_env {
            Some(v) => v.parse().unwrap_or(20),
            None => file_cfg.pow_difficulty.unwrap_or(20),
        };

        let max_relay_env = std::env::var("MAX_RELAY_SESSIONS").ok();
        let max_relay = match &max_relay_env {
            Some(v) => v.parse().unwrap_or(100),
            None => file_cfg.max_relay_sessions.unwrap_or(100),
        };

        Self {
            ws_listen_addr: resolve_addr(
                "WS_LISTEN_ADDR",
                &file_cfg.ws_listen_addr,
                "0.0.0.0:9000",
            ),
            http_listen_addr: resolve_addr(
                "HTTP_LISTEN_ADDR",
                &file_cfg.http_listen_addr,
                "0.0.0.0:8080",
            ),
            relay_listen_addr: resolve_addr(
                "RELAY_LISTEN_ADDR",
                &file_cfg.relay_listen_addr,
                "0.0.0.0:9001",
            ),
            db_path: resolve("DB_PATH", &file_cfg.db_path, "dxx_matchmaking.db"),
            google_client_id: resolve("GOOGLE_CLIENT_ID", &file_cfg.google_client_id, ""),
            google_client_secret: resolve(
                "GOOGLE_CLIENT_SECRET",
                &file_cfg.google_client_secret,
                "",
            ),
            admin_token: resolve("ADMIN_TOKEN", &file_cfg.admin_token, ""),
            motd: resolve("MOTD", &file_cfg.motd, ""),
            update_url: resolve("UPDATE_URL", &file_cfg.update_url, ""),
            tls_cert_path: resolve("TLS_CERT_PATH", &file_cfg.tls_cert_path, ""),
            tls_key_path: resolve("TLS_KEY_PATH", &file_cfg.tls_key_path, ""),
            relay_public_addr: resolve("RELAY_PUBLIC_ADDR", &file_cfg.relay_public_addr, ""),
            stun_listen_addr: resolve_addr(
                "STUN_LISTEN_ADDR",
                &file_cfg.stun_listen_addr,
                "0.0.0.0:3478",
            ),
            stun_listen_addr_alt: resolve_addr(
                "STUN_LISTEN_ADDR_ALT",
                &file_cfg.stun_listen_addr_alt,
                "0.0.0.0:3479",
            ),
            stun_public_addrs: resolve("STUN_PUBLIC_ADDRS", &file_cfg.stun_public_addrs, ""),
            log_dir: resolve("LOG_DIR", &file_cfg.log_dir, ""),
            skip_gpgs_verify: skip,
            pow_difficulty: pow,
            max_relay_sessions: max_relay,
        }
    }

    /// Load config from environment variables only (backwards-compatible alias).
    pub fn from_env() -> Self {
        Self::load()
    }
}
