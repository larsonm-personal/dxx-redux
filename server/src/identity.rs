use serde::Deserialize;
use tracing::{debug, info, warn};

/// Result of GPGS token verification.
pub enum VerifyResult {
    /// Token verified successfully; contains the stable GPGS player ID.
    Ok { gpgs_player_id: String },
    /// Verification failed.
    Failed { reason: String },
}

#[derive(Deserialize)]
struct TokenResponse {
    access_token: String,
    #[allow(dead_code)]
    id_token: Option<String>,
}

#[derive(Deserialize)]
struct PlayersResponse {
    #[serde(rename = "playerId")]
    player_id: String,
}

/// Exchange a GPGS server auth code for a stable GPGS player ID.
///
/// 1. POST to Google's token endpoint with the authorization_code grant
/// 2. GET /games/v1/players/me with the access_token to retrieve the
///    stable GPGS player ID (distinct from the Google account sub claim)
pub async fn verify_gpgs_token(
    deadline: tokio::time::Instant,
    client_id: &str,
    client_secret: &str,
    auth_code: &str,
) -> VerifyResult {
    verify_gpgs_at(
        deadline,
        client_id,
        client_secret,
        auth_code,
        "https://oauth2.googleapis.com/token",
        "https://www.googleapis.com/games/v1/players/me",
    )
    .await
}

async fn bounded_json<T: serde::de::DeserializeOwned>(
    mut response: reqwest::Response,
) -> Result<T, &'static str> {
    let mut bytes = Vec::new();
    while let Some(chunk) = response.chunk().await.map_err(|_| "response read failed")? {
        if bytes.len() + chunk.len() > 16 * 1024 {
            return Err("response too large");
        }
        bytes.extend_from_slice(&chunk);
    }
    serde_json::from_slice(&bytes).map_err(|_| "invalid response")
}

async fn verify_gpgs_at(
    deadline: tokio::time::Instant,
    client_id: &str,
    client_secret: &str,
    auth_code: &str,
    token_url: &str,
    player_url: &str,
) -> VerifyResult {
    match tokio::time::timeout_at(
        deadline,
        verify_gpgs_exchange(client_id, client_secret, auth_code, token_url, player_url),
    )
    .await
    {
        Ok(result) => result,
        Err(_) => VerifyResult::Failed {
            reason: "Authentication deadline exceeded".into(),
        },
    }
}

async fn verify_gpgs_exchange(
    client_id: &str,
    client_secret: &str,
    auth_code: &str,
    token_url: &str,
    player_url: &str,
) -> VerifyResult {
    // Step 1: Exchange authorization code for tokens
    let http = match reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(8))
        .build()
    {
        Ok(client) => client,
        Err(_) => {
            return VerifyResult::Failed {
                reason: "HTTP client unavailable".into(),
            }
        }
    };
    let token_resp = http
        .post(token_url)
        .form(&[
            ("grant_type", "authorization_code"),
            ("code", auth_code),
            ("client_id", client_id),
            ("client_secret", client_secret),
            ("redirect_uri", ""),
        ])
        .send()
        .await;

    let token_resp = match token_resp {
        Ok(r) => r,
        Err(e) => {
            warn!(%e, "GPGS token exchange HTTP request failed");
            return VerifyResult::Failed {
                reason: format!("token exchange request failed: {e}"),
            };
        }
    };

    if !token_resp.status().is_success() {
        let status = token_resp.status();
        warn!(%status, "GPGS token exchange failed");
        return VerifyResult::Failed {
            reason: format!("token exchange returned {status}"),
        };
    }

    let tokens: TokenResponse = match bounded_json(token_resp).await {
        Ok(t) => t,
        Err(e) => {
            warn!(%e, "failed to parse GPGS token response");
            return VerifyResult::Failed {
                reason: format!("failed to parse token response: {e}"),
            };
        }
    };

    debug!("GPGS token exchange succeeded, fetching player ID");

    // Step 2: Use access_token to get the stable GPGS player ID
    let player_resp = http
        .get(player_url)
        .bearer_auth(&tokens.access_token)
        .send()
        .await;

    let player_resp = match player_resp {
        Ok(r) => r,
        Err(e) => {
            warn!(%e, "GPGS players/me request failed");
            return VerifyResult::Failed {
                reason: format!("players/me request failed: {e}"),
            };
        }
    };

    if !player_resp.status().is_success() {
        let status = player_resp.status();
        warn!(%status, "GPGS players/me failed");
        return VerifyResult::Failed {
            reason: format!("players/me returned {status}"),
        };
    }

    let player: PlayersResponse = match bounded_json(player_resp).await {
        Ok(p) => p,
        Err(e) => {
            warn!(%e, "failed to parse GPGS players/me response");
            return VerifyResult::Failed {
                reason: format!("failed to parse players/me response: {e}"),
            };
        }
    };

    if player.player_id.is_empty() || player.player_id.len() > 256 {
        return VerifyResult::Failed {
            reason: "Invalid player identity".into(),
        };
    }
    info!("GPGS identity verified");

    VerifyResult::Ok {
        gpgs_player_id: player.player_id,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use axum::{
        routing::{get, post},
        Form, Json, Router,
    };
    use std::collections::HashMap;

    #[tokio::test]
    async fn gpgs_exchange_uses_verified_player_response() {
        let app = Router::new()
            .route(
                "/token",
                post(|Form(form): Form<HashMap<String, String>>| async move {
                    assert_eq!(form["code"], "fixture-code");
                    assert_eq!(form["client_id"], "fixture-client");
                    assert_eq!(form["client_secret"], "fixture-secret");
                    assert_eq!(form["grant_type"], "authorization_code");
                    Json(serde_json::json!({"access_token":"fixture-access"}))
                }),
            )
            .route(
                "/player",
                get(|headers: axum::http::HeaderMap| async move {
                    assert_eq!(headers["authorization"], "Bearer fixture-access");
                    Json(serde_json::json!({"playerId":"verified-fixture-player"}))
                }),
            )
            .route(
                "/denied",
                get(|| async { axum::http::StatusCode::UNAUTHORIZED }),
            )
            .route(
                "/delayed",
                get(|| async {
                    tokio::time::sleep(std::time::Duration::from_secs(1)).await;
                    axum::Json(serde_json::json!({"playerId":"too-late"}))
                }),
            )
            .route("/oversize", get(|| async { "x".repeat(20 * 1024) }));
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let base = format!("http://{}", listener.local_addr().unwrap());
        let task = tokio::spawn(async { axum::serve(listener, app).await.unwrap() });
        let verify = |path: &str| (format!("{base}/token"), format!("{base}/{path}"));
        let (token, player) = verify("player");
        assert!(
            matches!(verify_gpgs_at(tokio::time::Instant::now() + std::time::Duration::from_secs(2), "fixture-client", "fixture-secret", "fixture-code", &token, &player).await,
            VerifyResult::Ok { gpgs_player_id } if gpgs_player_id == "verified-fixture-player")
        );
        for path in ["denied", "oversize"] {
            let (token, player) = verify(path);
            assert!(matches!(
                verify_gpgs_at(
                    tokio::time::Instant::now() + std::time::Duration::from_secs(2),
                    "fixture-client",
                    "fixture-secret",
                    "fixture-code",
                    &token,
                    &player
                )
                .await,
                VerifyResult::Failed { .. }
            ));
        }
        // A valid exchange started late must not extend the connection deadline
        let (token, player) = verify("delayed");
        let start = tokio::time::Instant::now();
        let result = verify_gpgs_at(
            start + std::time::Duration::from_millis(100),
            "fixture-client",
            "fixture-secret",
            "fixture-code",
            &token,
            &player,
        )
        .await;
        assert!(
            matches!(result, VerifyResult::Failed { reason } if reason == "Authentication deadline exceeded")
        );
        assert!(start.elapsed() < std::time::Duration::from_millis(500));
        task.abort();
        let _ = task.await;
    }
}
