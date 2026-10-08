use std::net::SocketAddr;

use serde::Deserialize;

/// Server configuration, loaded from a JSONC file plus environment variable overrides.
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
    /// Public address of the UDP relay, sent to clients in RELAY_ASSIGNED.
    /// Can be a domain name or IP with port (e.g. "match.example.com:9001").
    pub relay_public_addr: String,
    /// Address for STUN listener 1 (e.g. "0.0.0.0:3478")
    pub stun_listen_addr: SocketAddr,
    /// Address for STUN listener 2, different port for NAT detection (e.g. "0.0.0.0:3479")
    pub stun_listen_addr_alt: SocketAddr,
    /// Public STUN addresses sent to clients in AUTH_OK.
    /// Can use domain names or IPs (e.g. "match.example.com:3478,match.example.com:3479").
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
    /// Maximum concurrent WebSocket connections. 0 = unlimited.
    pub max_connections: usize,
    /// Optional separate listen address for admin-only HTTP endpoints.
    /// When set, admin routes are served on this port and the main HTTP port
    /// serves only public routes. When None, all routes are on the main port.
    pub admin_http_listen_addr: Option<SocketAddr>,
    /// Force all game connections through the relay, ignoring connectivity
    /// check results. Useful for testing where peers share a NAT gateway
    /// (e.g. two Android emulators on the same host).
    pub force_relay: bool,
}

/// JSONC config file schema. All fields optional; env vars override file values.
#[derive(Deserialize, Default)]
#[cfg_attr(test, derive(serde::Serialize))]
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
    max_connections: Option<usize>,
    admin_http_listen_addr: Option<String>,
    force_relay: Option<bool>,
}

impl ServerConfig {
    /// Load config: read a JSONC file (if present), then overlay environment variables.
    /// File path comes from `CONFIG_FILE` env var (default: `server_config.jsonc`).
    pub fn load() -> Self {
        let config_path =
            std::env::var("CONFIG_FILE").unwrap_or_else(|_| "server_config.jsonc".into());
        let file_cfg = match std::fs::read_to_string(&config_path) {
            Ok(contents) => match strip_jsonc_comments(&contents).and_then(|json| {
                serde_json::from_str::<ConfigFile>(&json).map_err(|e| e.to_string())
            }) {
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
            std::env::var(env_key)
                .unwrap_or_else(|_| file_val.as_deref().unwrap_or(default).to_string())
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

        let max_conn_env = std::env::var("MAX_CONNECTIONS").ok();
        let max_conn = match &max_conn_env {
            Some(v) => v.parse().unwrap_or(500),
            None => file_cfg.max_connections.unwrap_or(500),
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
            max_connections: max_conn,
            admin_http_listen_addr: {
                let raw = resolve(
                    "ADMIN_HTTP_LISTEN_ADDR",
                    &file_cfg.admin_http_listen_addr,
                    "",
                );
                if raw.is_empty() {
                    None
                } else {
                    Some(raw.parse().unwrap_or_else(|e| {
                        panic!("ADMIN_HTTP_LISTEN_ADDR is not a valid socket address: {e}");
                    }))
                }
            },
            force_relay: {
                let env_val = std::env::var("FORCE_RELAY").ok();
                match &env_val {
                    Some(v) => v == "true",
                    None => file_cfg.force_relay.unwrap_or(false),
                }
            },
        }
    }

    /// Load config from environment variables only (backwards-compatible alias).
    pub fn from_env() -> Self {
        Self::load()
    }
}

fn strip_jsonc_comments(input: &str) -> Result<String, String> {
    let bytes = input.as_bytes();
    let mut output = Vec::with_capacity(input.len());
    let mut index = 0;
    let mut in_string = false;
    let mut escaped = false;

    while index < bytes.len() {
        let current = bytes[index];
        if in_string {
            output.push(current);
            if escaped {
                escaped = false;
            } else if current == b'\\' {
                escaped = true;
            } else if current == b'"' {
                in_string = false;
            }
            index += 1;
            continue;
        }

        if current == b'"' {
            in_string = true;
            output.push(b'"');
            index += 1;
        } else if current == b'/' && bytes.get(index + 1) == Some(&b'/') {
            index += 2;
            while index < bytes.len() && bytes[index] != b'\n' && bytes[index] != b'\r' {
                index += 1;
            }
        } else if current == b'/' && bytes.get(index + 1) == Some(&b'*') {
            index += 2;
            let mut closed = false;
            while index < bytes.len() {
                if bytes[index] == b'*' && bytes.get(index + 1) == Some(&b'/') {
                    index += 2;
                    closed = true;
                    break;
                }
                if bytes[index] == b'\n' || bytes[index] == b'\r' {
                    output.push(bytes[index]);
                }
                index += 1;
            }
            if !closed {
                return Err("unterminated JSONC block comment".into());
            }
        } else {
            output.push(current);
            index += 1;
        }
    }

    String::from_utf8(output).map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::{strip_jsonc_comments, ConfigFile};

    #[test]
    fn configuration_template_matches_schema() {
        let mut entries = serde_json::Map::new();
        for line in include_str!("../server_config.template.jsonc").lines() {
            let Some((key, value)) = line
                .trim()
                .strip_prefix("// ")
                .and_then(|s| s.split_once(':'))
            else {
                continue;
            };
            if !key.bytes().all(|b| b.is_ascii_lowercase() || b == b'_') {
                continue;
            }
            let clean = strip_jsonc_comments(value).expect("valid template entry");
            let value = serde_json::from_str(clean.trim().trim_end_matches(','))
                .expect("template defaults must be valid JSON");
            assert!(
                entries.insert(key.to_owned(), value).is_none(),
                "duplicate template key: {key}"
            );
        }
        let schema = serde_json::to_value(ConfigFile::default()).unwrap();
        assert_eq!(
            entries.keys().collect::<Vec<_>>(),
            schema.as_object().unwrap().keys().collect::<Vec<_>>()
        );
        let config: ConfigFile =
            serde_json::from_value(entries.into()).expect("template field types");
        assert_eq!(config.max_connections, Some(500));
        assert_eq!(config.force_relay, Some(false));
        assert_eq!(config.admin_http_listen_addr.as_deref(), Some(""));
    }

    #[test]
    fn configuration_file_precedence() {
        // Re-enter this test in a child so environment overrides cannot race other tests.
        const EXPECTED: &str = "DXX_TEST_CONFIG_EXPECTED";
        if let Ok(expected) = std::env::var(EXPECTED) {
            let config = super::ServerConfig::load();
            assert_eq!(
                format!("{}:{}", config.max_connections, config.force_relay),
                expected
            );
            return;
        }
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("server_config.jsonc");
        for (contents, overrides, expected) in [
            ("{}", None, "500:false"),
            (
                r#"{"max_connections":42,"force_relay":true}"#,
                None,
                "42:true",
            ),
            (
                r#"{"max_connections":42,"force_relay":true}"#,
                Some(("73", "false")),
                "73:false",
            ),
        ] {
            std::fs::write(&path, contents).unwrap();
            let mut command = std::process::Command::new(std::env::current_exe().unwrap());
            command
                .args([
                    "--exact",
                    "config::tests::configuration_file_precedence",
                    "--nocapture",
                ])
                .env_clear()
                .env("CONFIG_FILE", &path)
                .env(EXPECTED, expected);
            if let Some((max, relay)) = overrides {
                command
                    .env("MAX_CONNECTIONS", max)
                    .env("FORCE_RELAY", relay);
            }
            let output = command.output().expect("run isolated config loader");
            assert!(
                output.status.success(),
                "{}\n{}",
                String::from_utf8_lossy(&output.stdout),
                String::from_utf8_lossy(&output.stderr)
            );
        }
    }

    #[test]
    fn jsonc_comments_preserve_string_contents() {
        let input = r#"{
            // comment
            "update_url": "https://example.invalid/a//b",
            "motd": "/* literal */"
        }"#;
        let clean = strip_jsonc_comments(input).expect("valid JSONC");
        let config: ConfigFile = serde_json::from_str(&clean).expect("valid JSON");
        assert_eq!(
            config.update_url.as_deref(),
            Some("https://example.invalid/a//b")
        );
        assert_eq!(config.motd.as_deref(), Some("/* literal */"));
    }

    #[test]
    fn jsonc_block_comments_preserve_line_numbers_and_require_termination() {
        let clean = strip_jsonc_comments("{\n/* one\ntwo */\n\"motd\": \"ok\"\n}")
            .expect("valid JSONC block comment");
        assert_eq!(clean.lines().count(), 5);
        assert!(strip_jsonc_comments("{ /* unterminated").is_err());
    }
}
