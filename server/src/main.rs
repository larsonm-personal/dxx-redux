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
        listen_admin = config.admin_http_listen_addr.map(|a| a.to_string()).as_deref().unwrap_or("(same as http)"),
        listen_relay = %config.relay_listen_addr,
        listen_stun1 = %config.stun_listen_addr,
        listen_stun2 = %config.stun_listen_addr_alt,
        db = %config.db_path,
        tls = !config.tls_cert_path.is_empty(),
        google_login = !config.google_client_id.is_empty() && !config.google_auth_origin.is_empty(),
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
    let tls_config =
        match tls::load_rustls_config(&state.config.tls_cert_path, &state.config.tls_key_path) {
            Ok(Some(tls_cfg)) => {
                info!("TLS certificate loaded -- native wss:// enabled");
                Some(tls_cfg)
            }
            Ok(None) => {
                info!("TLS not configured, serving plain WebSocket");
                None
            }
            Err(e) => {
                error!(%e, "failed to load TLS certificate");
                std::process::exit(1);
            }
        };

    // Own every listener; any unexpected exit terminates the whole service
    let mut tasks = tokio::task::JoinSet::new();
    let http = Arc::clone(&state);
    tasks.spawn(async move {
        (
            "http",
            http_api::run(http.config.http_listen_addr, http).await,
        )
    });
    if let Some(addr) = state.config.admin_http_listen_addr {
        let admin = Arc::clone(&state);
        tasks.spawn(async move { ("admin", http_api::run_admin(addr, admin).await) });
    }
    let udp = Arc::clone(&state);
    tasks.spawn(async move { ("relay", relay::run(udp.config.relay_listen_addr, udp).await) });
    if !state.config.stun_public_addrs.is_empty() {
        for (listener, (addr, bit)) in [
            (
                state.config.stun_listen_addr,
                dxx_matchmaking::ListenerReady::STUN1,
            ),
            (
                state.config.stun_listen_addr_alt,
                dxx_matchmaking::ListenerReady::STUN2,
            ),
        ]
        .into_iter()
        .enumerate()
        {
            let stun_state = Arc::clone(&state);
            tasks.spawn(async move {
                let result: Result<(), Box<dyn std::error::Error + Send + Sync>> = async {
                    let socket = tokio::net::UdpSocket::bind(addr).await?;
                    let _ready = dxx_matchmaking::ListenerReady::new(Arc::clone(&stun_state), bit);
                    stun::authenticated_stun_loop(
                        &socket,
                        &stun_state.candidate_attempts,
                        listener,
                    )
                    .await;
                    Ok(())
                }
                .await;
                ("stun", result)
            });
        }
    }
    let periodic = Arc::clone(&state);
    tasks.spawn(async move {
        stats::periodic_tasks(periodic).await;
        ("periodic", Ok(()))
    });
    let ws = Arc::clone(&state);
    tasks.spawn(async move {
        (
            "websocket",
            ws_handler::run(ws.config.ws_listen_addr, ws, tls_config).await,
        )
    });
    let failed = tokio::select! {
        result = tasks.join_next() => {
            match result {
                Some(Ok((service, result))) => error!(service, error = ?result.err(), "required service stopped"),
                other => error!(?other, "required service task failed"),
            }
            true
        }
        result = tokio::signal::ctrl_c() => {
            if let Err(e) = result { error!(%e, "shutdown signal listener failed"); }
            false
        }
    };
    tasks.abort_all();
    while tasks.join_next().await.is_some() {}
    if failed {
        std::process::exit(1);
    }
}
