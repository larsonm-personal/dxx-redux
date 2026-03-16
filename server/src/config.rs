use std::net::SocketAddr;

/// Server configuration, loaded from environment variables.
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
    /// Directory for log files (empty = no file logging, stdout only)
    pub log_dir: String,
    /// Skip GPGS token verification (dev/test mode: use token as identity key directly)
    pub skip_gpgs_verify: bool,
    /// Proof-of-work difficulty (leading zero bits) for keypair registration.
    /// Default 20 (~200ms on phone, <1us to verify).
    pub pow_difficulty: u8,
}

impl ServerConfig {
    pub fn from_env() -> Self {
        fn env_or(key: &str, default: &str) -> String {
            std::env::var(key).unwrap_or_else(|_| default.to_string())
        }
        fn env_addr(key: &str, default: &str) -> SocketAddr {
            env_or(key, default).parse().unwrap_or_else(|e| {
                panic!("{key} is not a valid socket address: {e}");
            })
        }

        Self {
            ws_listen_addr: env_addr("WS_LISTEN_ADDR", "0.0.0.0:9000"),
            http_listen_addr: env_addr("HTTP_LISTEN_ADDR", "0.0.0.0:8080"),
            relay_listen_addr: env_addr("RELAY_LISTEN_ADDR", "0.0.0.0:9001"),
            db_path: env_or("DB_PATH", "dxx_matchmaking.db"),
            google_client_id: env_or("GOOGLE_CLIENT_ID", ""),
            google_client_secret: env_or("GOOGLE_CLIENT_SECRET", ""),
            admin_token: env_or("ADMIN_TOKEN", ""),
            motd: env_or("MOTD", ""),
            update_url: env_or("UPDATE_URL", ""),
            tls_cert_path: env_or("TLS_CERT_PATH", ""),
            tls_key_path: env_or("TLS_KEY_PATH", ""),
            relay_public_addr: env_or("RELAY_PUBLIC_ADDR", ""),
            log_dir: env_or("LOG_DIR", ""),
            skip_gpgs_verify: env_or("SKIP_GPGS_VERIFY", "false") == "true",
            pow_difficulty: env_or("POW_DIFFICULTY", "20").parse().unwrap_or(20),
        }
    }
}
