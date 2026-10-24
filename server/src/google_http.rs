//! Login endpoints share the selected WebSocket origin, not the public status listener

use std::{net::SocketAddr, sync::Arc, time::Duration};

use axum::{
    body::to_bytes,
    extract::{ConnectInfo, Request, State},
    http::{header, HeaderValue, StatusCode},
    response::{IntoResponse, Response},
    routing::post,
    Json, Router,
};
use serde::Deserialize;

use crate::{google_login::LoginError, ServerState};

const MAX_LOGIN_BODY: usize = 12 * 1024;
const REQUEST_LIFETIME: Duration = Duration::from_secs(5);

pub fn routes() -> Router<Arc<ServerState>> {
    Router::new()
        .route("/api/v1/auth/challenge", post(login))
        .route("/api/v1/auth/google", post(login))
        .route("/api/v1/auth/logout", post(login))
        .route("/api/v1/auth/cancel", post(login))
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Exchange {
    challenge: String,
    google_id_token: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Logout {
    session_token: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ChallengeRequest {}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Cancel {
    challenge: String,
}

async fn login(
    ConnectInfo(peer): ConnectInfo<SocketAddr>,
    State(state): State<Arc<ServerState>>,
    request: Request,
) -> Response {
    // All responses, including admission failures, must prohibit credential caching
    let mut response = admitted_login(peer, &state, request).await;
    response
        .headers_mut()
        .insert(header::CACHE_CONTROL, HeaderValue::from_static("no-store"));
    response
}

async fn admitted_login(peer: SocketAddr, state: &ServerState, request: Request) -> Response {
    let Some(login) = &state.google_login else {
        return StatusCode::SERVICE_UNAVAILABLE.into_response();
    };
    let Some(source) =
        crate::ws_handler::client_address(peer, request.headers(), &state.config.trusted_proxy_ips)
    else {
        return StatusCode::BAD_REQUEST.into_response();
    };
    // Admission precedes body reads and JSON/crypto work; do not queue waiting for a slot
    // Rejected sources must not spend the shared capacity needed by other users
    if !state.rate_limiter.check_ip_login(source.ip())
        || !state
            .auth_work
            .lock()
            .unwrap()
            .accept(std::time::Instant::now())
    {
        return StatusCode::TOO_MANY_REQUESTS.into_response();
    }
    let Ok(_permit) = state.auth_slots.try_acquire() else {
        return StatusCode::SERVICE_UNAVAILABLE.into_response();
    };
    let deadline = tokio::time::Instant::now() + REQUEST_LIFETIME;
    let result = tokio::time::timeout_at(deadline, async {
        let (parts, body) = request.into_parts();
        if parts.uri.query().is_some() {
            return StatusCode::BAD_REQUEST.into_response();
        }
        // Require JSON even for challenge allocation so browser form posts cannot mint sessions
        if parts
            .headers
            .get(header::CONTENT_TYPE)
            .and_then(|value| value.to_str().ok())
            .and_then(|value| value.split(';').next())
            != Some("application/json")
        {
            return StatusCode::UNSUPPORTED_MEDIA_TYPE.into_response();
        }
        let Ok(body) = to_bytes(body, MAX_LOGIN_BODY).await else {
            return StatusCode::PAYLOAD_TOO_LARGE.into_response();
        };
        match parts.uri.path() {
            "/api/v1/auth/challenge" => {
                if serde_json::from_slice::<ChallengeRequest>(&body).is_err() {
                    return StatusCode::BAD_REQUEST.into_response();
                }
                match login.challenge(source.ip(), std::time::Instant::now()) {
                    Ok(challenge) => Json(challenge).into_response(),
                    Err(error) => login_error(error),
                }
            }
            "/api/v1/auth/google" => {
                let Ok(exchange) = serde_json::from_slice::<Exchange>(&body) else {
                    return StatusCode::BAD_REQUEST.into_response();
                };
                match login
                    .exchange(&exchange.challenge, &exchange.google_id_token, deadline)
                    .await
                {
                    Ok(credential) => Json(credential).into_response(),
                    Err(error) => login_error(error),
                }
            }
            "/api/v1/auth/cancel" => {
                let Ok(cancel) = serde_json::from_slice::<Cancel>(&body) else {
                    return StatusCode::BAD_REQUEST.into_response();
                };
                login.cancel(&cancel.challenge);
                StatusCode::NO_CONTENT.into_response()
            }
            "/api/v1/auth/logout" => {
                let Ok(logout) = serde_json::from_slice::<Logout>(&body) else {
                    return StatusCode::BAD_REQUEST.into_response();
                };
                // Idempotent, possession-only revocation; no account existence oracle
                login.revoke(&logout.session_token);
                StatusCode::NO_CONTENT.into_response()
            }
            _ => StatusCode::NOT_FOUND.into_response(),
        }
    })
    .await;
    result.unwrap_or_else(|_| StatusCode::REQUEST_TIMEOUT.into_response())
}

fn login_error(error: LoginError) -> Response {
    match error {
        LoginError::InvalidRequest => StatusCode::BAD_REQUEST,
        LoginError::InvalidCredential => StatusCode::UNAUTHORIZED,
        LoginError::Capacity => StatusCode::TOO_MANY_REQUESTS,
        LoginError::Unavailable => StatusCode::SERVICE_UNAVAILABLE,
    }
    .into_response()
}
