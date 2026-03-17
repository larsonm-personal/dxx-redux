use dxx_matchmaking::{config, http_api, relay, stats, stun, tls, ws_handler};
use std::sync::Arc;
use tracing::{error, info};
use tracing_subscriber::layer::SubscriberExt;
use tracing_subscriber::util::SubscriberInitExt;

#[tokio::main]
async fn main() {
    let config = config::ServerConfig::load();

    // Initialize structured logging: console (human-readable) + optional file (JSON)
    let env_filter = tracing_subscriber::EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info"));

    let console_layer = tracing_subscriber::fmt::layer()
        .with_target(true)
        .with_thread_ids(false);

    // Guard must live for the entire program so the file writer stays open
    let _file_guard;

    if config.log_dir.is_empty() {
        _file_guard = None;
        tracing_subscriber::registry()
            .with(env_filter)
            .with(console_layer)
            .init();
    } else {
        let file_appender =
            tracing_appender::rolling::daily(&config.log_dir, "dxx-matchmaking.log");
        let (non_blocking, guard) = tracing_appender::non_blocking(file_appender);
        _file_guard = Some(guard);

        let file_layer = tracing_subscriber::fmt::layer()
            .json()
            .with_writer(non_blocking);

        tracing_subscriber::registry()
            .with(env_filter)
            .with(console_layer)
            .with(file_layer)
            .init();

        info!(log_dir = %config.log_dir, "file logging enabled (daily rotation)");
    }

    info!(
        listen_ws = %config.ws_listen_addr,
        listen_http = %config.http_listen_addr,
        listen_relay = %config.relay_listen_addr,
        listen_stun1 = %config.stun_listen_addr,
        listen_stun2 = %config.stun_listen_addr_alt,
        db = %config.db_path,
        tls = !config.tls_cert_path.is_empty(),
        gpgs = !config.google_client_id.is_empty(),
        relay_public = %config.relay_public_addr,
        stun_public = %config.stun_public_addrs,
        "starting dxx-matchmaking server"
    );

    let state = match dxx_matchmaking::build_state(config) {
        Ok(s) => s,
        Err(e) => {
            error!(%e, "failed to open database");
            std::process::exit(1);
        }
    };

    // Load TLS certificate if configured
    match tls::load_rustls_config(&state.config.tls_cert_path, &state.config.tls_key_path) {
        Ok(Some(_tls_config)) => {
            info!("TLS certificate loaded (reverse proxy recommended for production)");
        }
        Ok(None) => {
            info!("TLS not configured, serving plain WebSocket");
        }
        Err(e) => {
            error!(%e, "failed to load TLS certificate");
            std::process::exit(1);
        }
    }

    // Spawn the HTTP API server (public status + admin endpoints)
    let http_state = Arc::clone(&state);
    let http_addr = state.config.http_listen_addr;
    let http_handle = tokio::spawn(async move {
        if let Err(e) = http_api::run(http_addr, http_state).await {
            error!(%e, "HTTP API server failed");
        }
    });

    // Spawn the UDP relay listener
    let relay_state = Arc::clone(&state);
    let relay_addr = state.config.relay_listen_addr;
    let relay_handle = tokio::spawn(async move {
        if let Err(e) = relay::run(relay_addr, relay_state).await {
            error!(%e, "UDP relay failed");
        }
    });

    // Spawn STUN listeners (only if public addresses are configured)
    let stun_handles = if !state.config.stun_public_addrs.is_empty() {
        let addr1 = state.config.stun_listen_addr;
        let addr2 = state.config.stun_listen_addr_alt;
        let al1 = Arc::clone(&state.stun_allowlist);
        let al2 = Arc::clone(&state.stun_allowlist);
        let h1 = tokio::spawn(async move {
            if let Err(e) = stun::run(addr1, al1).await {
                error!(%e, "STUN listener 1 failed");
            }
        });
        let h2 = tokio::spawn(async move {
            if let Err(e) = stun::run(addr2, al2).await {
                error!(%e, "STUN listener 2 failed");
            }
        });
        info!(
            listen1 = %addr1, listen2 = %addr2,
            public = %state.config.stun_public_addrs,
            "STUN listeners started"
        );
        vec![h1, h2]
    } else {
        info!("STUN listeners disabled (STUN_PUBLIC_ADDRS not set)");
        vec![]
    };

    // Spawn periodic tasks (stats snapshots, cleanup)
    let periodic_state = Arc::clone(&state);
    let periodic_handle = tokio::spawn(async move {
        stats::periodic_tasks(periodic_state).await;
    });

    // Run the WebSocket server (blocks on the main task)
    let ws_state = Arc::clone(&state);
    let ws_addr = state.config.ws_listen_addr;
    if let Err(e) = ws_handler::run(ws_addr, ws_state).await {
        error!(%e, "WebSocket server failed");
    }

    // If the WS server exits, shut down everything
    http_handle.abort();
    relay_handle.abort();
    periodic_handle.abort();
    for h in stun_handles {
        h.abort();
    }
}
