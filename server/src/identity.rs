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
    client_id: &str,
    client_secret: &str,
    auth_code: &str,
) -> VerifyResult {
    // Step 1: Exchange authorization code for tokens
    let http = reqwest::Client::new();
    let token_resp = http
        .post("https://oauth2.googleapis.com/token")
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
        let body = token_resp.text().await.unwrap_or_default();
        warn!(%status, body_prefix = &body[..body.len().min(200)], "GPGS token exchange failed");
        return VerifyResult::Failed {
            reason: format!("token exchange returned {status}"),
        };
    }

    let tokens: TokenResponse = match token_resp.json().await {
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
        .get("https://www.googleapis.com/games/v1/players/me")
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
        let body = player_resp.text().await.unwrap_or_default();
        warn!(%status, body_prefix = &body[..body.len().min(200)], "GPGS players/me failed");
        return VerifyResult::Failed {
            reason: format!("players/me returned {status}"),
        };
    }

    let player: PlayersResponse = match player_resp.json().await {
        Ok(p) => p,
        Err(e) => {
            warn!(%e, "failed to parse GPGS players/me response");
            return VerifyResult::Failed {
                reason: format!("failed to parse players/me response: {e}"),
            };
        }
    };

    info!(gpgs_player_id = %player.player_id, "GPGS identity verified");

    VerifyResult::Ok {
        gpgs_player_id: player.player_id,
    }
}
