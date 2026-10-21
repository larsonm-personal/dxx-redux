//! Bound work before HTTP parsing, WebSocket upgrade and authentication
use std::net::SocketAddr;
use std::sync::Arc;
use std::time::{Duration, Instant};

use axum::extract::ConnectInfo;
use axum::Router;
use tokio::io::{AsyncRead, AsyncWrite};
use tokio::net::TcpListener;
use tower::ServiceExt;

use crate::ServerState;

async fn serve_http<T>(stream: T, peer: SocketAddr, app: Router)
where
    T: AsyncRead + AsyncWrite + Unpin + Send + 'static,
{
    let service = app.into_service().map_request(
        move |mut request: hyper::Request<hyper::body::Incoming>| {
            request.extensions_mut().insert(ConnectInfo(peer));
            request
        },
    );
    let service = hyper_util::service::TowerToHyperService::new(service);
    // This deadline ends at upgrade; active WebSockets have their own ownership
    let _ = tokio::time::timeout(Duration::from_secs(20), async {
        hyper::server::conn::http1::Builder::new()
            .max_buf_size(32 * 1024)
            .max_headers(64)
            .serve_connection(hyper_util::rt::TokioIo::new(stream), service)
            .with_upgrades()
            .await
    })
    .await;
}

pub async fn serve(
    listener: TcpListener,
    app: Router,
    state: Arc<ServerState>,
    tls: Option<Arc<rustls::ServerConfig>>,
) -> std::io::Result<()> {
    let acceptor = tls.map(tokio_rustls::TlsAcceptor::from);
    let mut tasks = tokio::task::JoinSet::new();
    loop {
        while tasks.try_join_next().is_some() {}
        let (stream, peer) = listener.accept().await?;
        if !state.transport_work.lock().unwrap().accept(Instant::now()) {
            continue;
        }
        let Ok(permit) = Arc::clone(&state.transport_slots).try_acquire_owned() else {
            continue;
        };
        let app = app.clone();
        let acceptor = acceptor.clone();
        tasks.spawn(async move {
            let _permit = permit;
            if let Some(acceptor) = acceptor {
                if let Ok(Ok(stream)) =
                    tokio::time::timeout(Duration::from_secs(5), acceptor.accept(stream)).await
                {
                    serve_http(stream, peer, app).await;
                }
            } else {
                serve_http(stream, peer, app).await;
            }
        });
    }
}
