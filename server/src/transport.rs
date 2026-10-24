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

fn retryable_accept_error(error: &std::io::Error) -> bool {
    use std::io::ErrorKind;
    if matches!(
        error.kind(),
        ErrorKind::Interrupted
            | ErrorKind::WouldBlock
            | ErrorKind::ConnectionAborted
            | ErrorKind::ConnectionReset
            | ErrorKind::TimedOut
            | ErrorKind::BrokenPipe
            | ErrorKind::NetworkDown
            | ErrorKind::NetworkUnreachable
            | ErrorKind::HostUnreachable
            | ErrorKind::OutOfMemory
    ) {
        return true;
    }
    // Resource pressure: EMFILE/ENFILE on Unix, ENOBUFS on Linux/BSD,
    // and WSAEMFILE/WSAENOBUFS on Windows. No socket or limit is changed
    match error.raw_os_error() {
        #[cfg(unix)]
        Some(23 | 24) => true,
        // Linux may report pending connection/network errors from accept
        #[cfg(any(target_os = "linux", target_os = "android"))]
        Some(64 | 71 | 92 | 95 | 100 | 101 | 105 | 112 | 113) => true,
        #[cfg(any(
            target_os = "macos",
            target_os = "freebsd",
            target_os = "openbsd",
            target_os = "netbsd",
            target_os = "dragonfly"
        ))]
        Some(55) => true,
        #[cfg(windows)]
        Some(10024 | 10055) => true,
        _ => false,
    }
}

async fn accept_with_retry<T, F, Fut>(mut accept: F) -> std::io::Result<T>
where
    F: FnMut() -> Fut,
    Fut: std::future::Future<Output = std::io::Result<T>>,
{
    let mut delay = Duration::from_millis(10);
    loop {
        match accept().await {
            Ok(connection) => return Ok(connection),
            Err(error) if retryable_accept_error(&error) => {
                tokio::time::sleep(delay).await;
                delay = (delay * 2).min(Duration::from_millis(250));
            }
            Err(error) => return Err(error),
        }
    }
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
        let (stream, peer) = accept_with_retry(|| listener.accept()).await?;
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::{Error, ErrorKind};

    #[tokio::test]
    async fn recoverable_accept_errors_back_off_then_deliver_the_connection() {
        let mut failures = std::collections::VecDeque::from([
            Err(Error::from(ErrorKind::ConnectionAborted)),
            Err(Error::from(ErrorKind::Interrupted)),
            Err(Error::from(ErrorKind::OutOfMemory)),
            Ok(42),
        ]);
        let started = Instant::now();
        let value = accept_with_retry(|| std::future::ready(failures.pop_front().unwrap()))
            .await
            .unwrap();
        assert_eq!(value, 42);
        assert!(started.elapsed() >= Duration::from_millis(70));
        assert!(failures.is_empty());
        for kind in [
            ErrorKind::InvalidInput,
            ErrorKind::PermissionDenied,
            ErrorKind::NotConnected,
        ] {
            let mut calls = 0;
            let error = accept_with_retry(|| {
                calls += 1;
                std::future::ready(Err::<(), _>(Error::from(kind)))
            })
            .await
            .unwrap_err();
            assert_eq!(error.kind(), kind);
            assert_eq!(calls, 1);
        }
    }

    #[test]
    fn descriptor_pressure_is_retryable_but_invalid_listener_is_fatal() {
        #[cfg(unix)]
        {
            assert!(retryable_accept_error(&Error::from_raw_os_error(24)));
            assert!(retryable_accept_error(&Error::from_raw_os_error(23)));
            assert!(!retryable_accept_error(&Error::from_raw_os_error(9)));
        }
        #[cfg(windows)]
        {
            assert!(retryable_accept_error(&Error::from_raw_os_error(10024)));
            assert!(retryable_accept_error(&Error::from_raw_os_error(10055)));
            assert!(!retryable_accept_error(&Error::from_raw_os_error(10038)));
        }
    }
}
