use super::*;
use axum::{extract::State, response::IntoResponse, routing::get, Router};
use jsonwebtoken::{encode, EncodingKey, Header};
use serde_json::{json, Value};
use std::sync::{
    atomic::{AtomicU64, AtomicUsize, Ordering},
    Arc,
};

const AUDIENCE: &str = "matchmaking-fixture.apps.googleusercontent.com";
const NONCE: &str = "fixture-login-nonce-with-at-least-32-bytes";
// Generated solely for this public test fixture, never a deployed credential
const PRIVATE_KEY: &str = "-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDR7PZFIquYyQwN
lzsRzBvXrKMrsH+OQ1EIyaLYE6TWUAbRljm+/y1VCwbD+gc1miJYOKOh9bRs4GpZ
htZAZG8n1XS4exQRPF9wlDykv6eKmTaUN9Tle7uy08J/sSVAuUD3B8xgkIXSsTUA
cNja4Y0Npu5EVlxbgpnvBJeoVtSAJQ6dxmO4dBoBR0aMvJ5U0UxYkp5tzJ91qAIj
6hCvzHFvOKsDiJxboHQXphlHVtEEhcgOd1zGcYIoRgrNwa+mPShhRzmedc6GoJ74
/K4ed04zybrsKaHkQ6h9jYQoiz7perACH/sz+CcTpt6Gqp1koy5MSoQUsJEKNUrJ
Vxohw771AgMBAAECggEBAIMpv6Xqx7SqyXEkcK/qC+tx3nHJ0akdvl/zPGAtxexy
DBCgAGyqG7TrMsr4SdDgAV2F5hen4Ze1kD1n1i5dkmYdJNSvchZi1m8M7+cwKIqc
uFFboRXRfbF6RihqkpAiL8kpgjVDl5nRRwCDgbsg+nTxPCZOn14999gz0l5IAnYd
jzAvYn+4H0JWy8jGpLILsWtLQUbMgfeeZvEeFCrylcuRlrLZZodG739V+UZEahBc
XYrMTT/3uRAxesdZ25fA4aJSONzppyFINL8lB6tN24rDspR1hWHzOGsKuDhzwhSK
7XpfSZezVmsWUujdzzM1a+lMfFIRyetT1ibKrpxkI/ECgYEA2+jK1kGxjJdjNHBV
ivPN+hGswFCLE8N9awosqZMxiBmLt6lBIGHMd5kfOAQTEQfqEh65OKgAY3yx3WQp
XbxUw5JnKe0NFXrnBISQWgE4BOjAAuZumHLbcLHHHXsZbXkJJKjU843j0h/TcRsz
74u0mMUalnaP1wnTW+Dy2zgjNlsCgYEA9GC3hBEWu8MHd8S2NvXHdQsgTmJj4D60
iiZWUIYRndleNp6fpsjBKdfk1CHk8fJQptzAKpgYr/0DBCuSpmDdW1w+GA0FKxxi
UKmHEZ4gHAA+7Q/booqJI0adyzZPUTMk7yt/+HnCuD3K/Mq2RbXkj8jN0Hfaeyau
y+f2FGfsAO8CgYA40nAG6Lad40go9pXD77dbVFHU53roQm0XyWs87eGwjxlA/aTs
XxjcFl6wtKzQi8D5qaKmdxKSole7IvRlWhJ07lFlzfDBih5DVpWODB6TdiiXFz+g
eJ0nA2i1v2cUmF+6bz2HKTSiyMJzyVmkHe8uQk/abQxBvjI03W0jI+BFXQKBgQC9
pUzlbYmR3rEt7fdmQWgvWZoRmmNdw+vBhizXwqNrNyPZCVJmUUcuHMAVK9SLOnwC
ci+jHnOSJ6wukU32VheSbcw2R+xp31YguN77+hV7BWnP+M1+nzn7qTKNYDE/l4wj
G0PtZTiKWFHtybaX9KdSOggWrtxbU9K0pOegQGGy+wKBgQDauwfyxAE6qv+q/Url
V/tVHOCxu3t2ar83IjH/7pbOaMre6ji3Svq9jZif8/YIaihQ6+IdfS65CW758U1Q
tJruw4A7Uv/DNXOQfvxufdu4Thk9L86Je7KmiR0w0IVnUUyrJJYo/hUFg431E0Ez
F5541rLQwzcTT0MMmnCbQwNYlQ==
-----END PRIVATE KEY-----";
const MODULUS: &str = "0ez2RSKrmMkMDZc7Ecwb16yjK7B_jkNRCMmi2BOk1lAG0ZY5vv8tVQsGw_oHNZoiWDijofW0bOBqWYbWQGRvJ9V0uHsUETxfcJQ8pL-nipk2lDfU5Xu7stPCf7ElQLlA9wfMYJCF0rE1AHDY2uGNDabuRFZcW4KZ7wSXqFbUgCUOncZjuHQaAUdGjLyeVNFMWJKebcyfdagCI-oQr8xxbzirA4icW6B0F6YZR1bRBIXIDndcxnGCKEYKzcGvpj0oYUc5nnXOhqCe-PyuHndOM8m67Cmh5EOofY2EKIs-6XqwAh_7M_gnE6behqqdZKMuTEqEFLCRCjVKyVcaIcO-9Q";

fn jwk(kid: &str) -> Value {
    json!({"kid":kid,"kty":"RSA","alg":"RS256","use":"sig","n":MODULUS,"e":"AQAB"})
}

fn login_server_state(verifier: Arc<GoogleVerifier>) -> Arc<crate::ServerState> {
    let config = crate::config::ServerConfig {
        ws_listen_addr: "127.0.0.1:0".parse().unwrap(),
        http_listen_addr: "127.0.0.1:0".parse().unwrap(),
        relay_listen_addr: "127.0.0.1:0".parse().unwrap(),
        db_path: ":memory:".into(),
        google_client_id: AUDIENCE.into(),
        google_auth_origin: "https://match.example".into(),
        admin_token: String::new(),
        motd: String::new(),
        update_url: String::new(),
        tls_cert_path: String::new(),
        tls_key_path: String::new(),
        relay_public_addr: String::new(),
        stun_listen_addr: "127.0.0.1:0".parse().unwrap(),
        stun_listen_addr_alt: "127.0.0.1:0".parse().unwrap(),
        stun_public_addrs: String::new(),
        log_dir: String::new(),
        skip_gpgs_verify: true,
        pow_difficulty: 8,
        max_relay_sessions: 100,
        relay_policy: Default::default(),
        max_connections: 500,
        admin_http_listen_addr: None,
        force_relay: false,
        installation_auth_audience: String::new(),
        trusted_proxy_ips: vec![],
    };
    let mut state = crate::build_state(config).unwrap();
    Arc::get_mut(&mut state).unwrap().google_login = Some(
        crate::google_login::GoogleLogin::with_verifier("https://match.example", verifier).unwrap(),
    );
    state
}

#[tokio::test]
async fn signed_google_accounts_join_verified_rooms_and_own_renewals() {
    use futures_util::{SinkExt, StreamExt};
    use tokio_tungstenite::tungstenite::Message;
    type Socket = tokio_tungstenite::WebSocketStream<
        tokio_tungstenite::MaybeTlsStream<tokio::net::TcpStream>,
    >;
    async fn receive(socket: &mut Socket, kind: &str) -> Value {
        tokio::time::timeout(Duration::from_secs(3), async {
            loop {
                let message = socket.next().await.unwrap().unwrap();
                if let Ok(text) = message.to_text() {
                    let value: Value = serde_json::from_str(text).unwrap();
                    assert_ne!(value["type"], "ERROR", "{value}");
                    if value["type"] == kind {
                        return value;
                    }
                }
            }
        })
        .await
        .unwrap()
    }
    async fn send(socket: &mut Socket, value: Value) {
        socket
            .send(Message::Text(value.to_string().into()))
            .await
            .unwrap();
    }
    let fixture = Fixture::new().await;
    let mut state = login_server_state(fixture.verifier.clone());
    Arc::get_mut(&mut state).unwrap().config.skip_gpgs_verify = false;
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let app = crate::ws_handler::ws_router(state.clone());
    struct Stop(tokio::task::JoinHandle<()>);
    impl Drop for Stop {
        fn drop(&mut self) {
            self.0.abort();
        }
    }
    let _stop = Stop(tokio::spawn(async move {
        axum::serve(
            listener,
            app.into_make_service_with_connect_info::<std::net::SocketAddr>(),
        )
        .await
        .unwrap();
    }));
    let mut sockets = Vec::new();
    let mut players = Vec::new();
    for index in 0..3 {
        let login = state.google_login.as_ref().unwrap();
        let challenge = login
            .challenge("127.0.0.1".parse().unwrap(), std::time::Instant::now())
            .unwrap();
        let mut payload = claims();
        payload["sub"] = json!(format!("owned-google-{index}"));
        payload["nonce"] = json!(crate::google_login::login_nonce(
            "https://match.example",
            &challenge.challenge
        )
        .unwrap());
        let credential = login
            .exchange(
                &challenge.challenge,
                &token("key-1", &payload),
                Instant::now() + Duration::from_secs(2),
            )
            .await
            .unwrap();
        let (mut socket, _) = tokio_tungstenite::connect_async(format!("ws://{address}/ws"))
            .await
            .unwrap();
        send(
            &mut socket,
            json!({"type":"AUTHENTICATE", "protocol_version":crate::protocol::CURRENT_PROTOCOL,
            "client_version":"fixture", "callsign":format!("pilot-{index}"), "platform":"test",
            "auth_method":"google_session", "session_credential":credential.session_token}),
        )
        .await;
        let reply = receive(&mut socket, "AUTH_OK").await;
        let player: uuid::Uuid = reply["player_id"].as_str().unwrap().parse().unwrap();
        assert!(state.sessions.get(&player).unwrap().account_verified);
        players.push(player);
        sockets.push(socket);
    }
    send(
        &mut sockets[0],
        json!({"type":"CREATE_LOBBY", "game":"d2", "max_players":4,
        "verified_only":true, "game_info":{"mission":"Counterstrike!", "mode":"coop"}}),
    )
    .await;
    let room: uuid::Uuid = receive(&mut sockets[0], "LOBBY_UPDATE").await["lobby_id"]
        .as_str()
        .unwrap()
        .parse()
        .unwrap();
    send(
        &mut sockets[1],
        json!({"type":"JOIN_LOBBY", "lobby_id":room}),
    )
    .await;
    receive(&mut sockets[1], "LOBBY_UPDATE").await;
    // Existing accepted friendship is independent of the authentication provider
    state
        .db
        .add_friend_request(&players[0], &players[2])
        .unwrap();
    state.db.accept_friend(&players[2], &players[0]).unwrap();
    send(
        &mut sockets[2],
        json!({"type":"JOIN_FRIEND_GAME", "friend_player_id":players[0]}),
    )
    .await;
    assert_eq!(
        receive(&mut sockets[2], "JOIN_FRIEND_GAME_RESP").await["success"],
        true
    );
    assert_eq!(state.lobbies.get(&room).unwrap().players.len(), 3);

    // Seed only the already-established gameplay allocation; auth and room admission above are real wire flows
    let game_epoch = uuid::Uuid::new_v4();
    let members = {
        let mut lobby = state.lobbies.get_mut(&room).unwrap();
        lobby.state = crate::lobby::LobbyState::InGame;
        lobby.game_epoch = game_epoch;
        [0, 1].map(|index| {
            let player = players[index];
            let slot = lobby
                .players
                .iter()
                .find(|entry| entry.player_id == player)
                .unwrap()
                .slot;
            (
                slot,
                player,
                state.sessions.get(&player).unwrap().connection_id,
            )
        })
    };
    let now = std::time::Instant::now();
    let owner = crate::relay::RelayOwnership {
        game_epoch,
        members,
    };
    state.relay_sessions.insert(
        123,
        crate::relay::RelaySession::owned(
            room,
            owner,
            now - Duration::from_secs(7141),
            Duration::from_secs(7200),
            true,
        ),
    );
    crate::relay_lifecycle::tick(&state, now);
    let first = receive(&mut sockets[0], "RELAY_REKEY_OFFER").await;
    let second = receive(&mut sockets[1], "RELAY_REKEY_OFFER").await;
    assert_eq!(first["new_token"], second["new_token"]);
    assert_eq!(state.relay_renewals.lock().unwrap().len(), 1);
    // Production renewal must retire both generations if either account loses verified status
    state
        .sessions
        .get_mut(&players[1])
        .unwrap()
        .account_verified = false;
    crate::relay_lifecycle::tick(&state, now + Duration::from_millis(1));
    assert!(state.relay_renewals.lock().unwrap().is_empty());
    assert!(state.relay_sessions.is_empty());
}

#[tokio::test]
async fn google_websocket_enforces_bans_and_live_credential_expiry() {
    use futures_util::{SinkExt, StreamExt};
    use tokio_tungstenite::tungstenite::Message;
    let fixture = Fixture::new().await;
    let mut state = login_server_state(fixture.verifier.clone());
    Arc::get_mut(&mut state).unwrap().config.skip_gpgs_verify = false;
    let login = state.google_login.as_ref().unwrap();
    let challenge = login
        .challenge("127.0.0.1".parse().unwrap(), std::time::Instant::now())
        .unwrap();
    let mut payload = claims();
    payload["nonce"] = json!(crate::google_login::login_nonce(
        "https://match.example",
        &challenge.challenge
    )
    .unwrap());
    let credential = login
        .exchange(
            &challenge.challenge,
            &token("key-1", &payload),
            Instant::now() + Duration::from_secs(2),
        )
        .await
        .unwrap();
    let pid = state
        .db
        .find_or_create_player_by_google("google-account-123", "pilot")
        .unwrap();
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let app = crate::ws_handler::ws_router(state.clone());
    struct Stop(tokio::task::JoinHandle<()>);
    impl Drop for Stop {
        fn drop(&mut self) {
            self.0.abort();
        }
    }
    let _stop = Stop(tokio::spawn(async move {
        axum::serve(
            listener,
            app.into_make_service_with_connect_info::<std::net::SocketAddr>(),
        )
        .await
        .unwrap();
    }));
    let connect = |secret: String| async move {
        let (mut socket, _) = tokio_tungstenite::connect_async(format!("ws://{address}/ws"))
            .await
            .unwrap();
        socket.send(Message::Text(json!({"type":"AUTHENTICATE", "protocol_version":crate::protocol::CURRENT_PROTOCOL,
            "client_version":"fixture", "callsign":"renamed", "platform":"test", "auth_method":"google_session",
            "session_credential":secret}).to_string().into())).await.unwrap();
        let message = tokio::time::timeout(Duration::from_secs(2), socket.next())
            .await
            .unwrap()
            .unwrap()
            .unwrap();
        let reply: Value = serde_json::from_str(message.to_text().unwrap()).unwrap();
        (socket, reply)
    };
    state.db.ban_player(&pid, "fixture ban", None).unwrap();
    let (_, denied) = connect(credential.session_token.clone()).await;
    assert_eq!(denied["type"], "AUTH_FAIL");
    assert!(!state.sessions.contains_key(&pid));
    state.db.unban_player(&pid).unwrap();
    for expire in [false, true] {
        let (mut socket, reply) = connect(credential.session_token.clone()).await;
        assert_eq!(reply["type"], "AUTH_OK");
        assert_eq!(reply["player_id"], pid.to_string());
        if expire {
            // Test-only clock substitution exercises the same live validity check as absolute expiry
            login.expire_fixture_credential(&credential.session_token);
        } else {
            state.db.ban_player(&pid, "live ban", None).unwrap();
        }
        tokio::time::timeout(Duration::from_secs(3), async {
            while let Some(Ok(message)) = socket.next().await {
                if message.is_close() {
                    break;
                }
            }
        })
        .await
        .unwrap();
        assert!(!state.sessions.contains_key(&pid));
        state.db.unban_player(&pid).unwrap();
    }
    let (_, denied) = connect(credential.session_token.clone()).await;
    assert_eq!(denied["type"], "AUTH_FAIL");
}

#[tokio::test]
async fn google_http_source_rejections_preserve_global_login_budget() {
    use axum::{body::Body, extract::ConnectInfo, http::Request};
    use tower::ServiceExt;

    let fixture = Fixture::new().await;
    let mut state = login_server_state(fixture.verifier.clone());
    Arc::get_mut(&mut state).unwrap().config.skip_gpgs_verify = false;
    // Long fixture window makes the quota assertion independent of machine speed
    *state.auth_work.lock().unwrap() = crate::resource_budget::WorkBudget::new(64, 3600);
    let blocked: std::net::SocketAddr = "192.0.2.1:1234".parse().unwrap();
    let other: std::net::SocketAddr = "192.0.2.2:1234".parse().unwrap();
    for _ in 0..64 {
        assert!(state.rate_limiter.check_ip_login(blocked.ip()));
    }
    let app = crate::ws_handler::ws_router(state.clone());
    let request = |peer| {
        Request::builder()
            .method("POST")
            .uri("/api/v1/auth/challenge")
            .header("content-type", "application/json")
            .extension(ConnectInfo(peer))
            .body(Body::from("{}"))
            .unwrap()
    };
    assert_eq!(
        app.clone().oneshot(request(other)).await.unwrap().status(),
        200
    );
    for _ in 0..64 {
        let response = app.clone().oneshot(request(blocked)).await.unwrap();
        assert_eq!(response.status(), 429);
        assert_eq!(response.headers()["cache-control"], "no-store");
    }
    assert_eq!(
        app.clone().oneshot(request(other)).await.unwrap().status(),
        200
    );
    // Only the two admitted requests consumed the shared budget
    let mut budget = state.auth_work.lock().unwrap();
    for _ in 0..62 {
        assert!(budget.accept(std::time::Instant::now()));
    }
    assert!(!budget.accept(std::time::Instant::now()));
    drop(budget);
    assert_eq!(app.oneshot(request(other)).await.unwrap().status(), 429);
}

#[tokio::test]
async fn google_http_login_exchange_logout_and_admission() {
    use axum::http::StatusCode;
    use futures_util::{SinkExt, StreamExt};
    use tokio_tungstenite::tungstenite::Message;
    let fixture = Fixture::new().await;
    let mut state = login_server_state(fixture.verifier.clone());
    // Exercise production admission with the owned in-memory fixture, never provider bypass
    Arc::get_mut(&mut state).unwrap().config.skip_gpgs_verify = false;
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let base = format!("http://{address}/api/v1/auth");
    let app = crate::ws_handler::ws_router(state.clone());
    let server = tokio::spawn(async move {
        axum::serve(
            listener,
            app.into_make_service_with_connect_info::<std::net::SocketAddr>(),
        )
        .await
        .unwrap();
    });
    // Abort even on assertion failure so this owned listener cannot outlive the fixture
    struct Stop(tokio::task::JoinHandle<()>);
    impl Drop for Stop {
        fn drop(&mut self) {
            self.0.abort();
        }
    }
    let _stop = Stop(server);
    let client = reqwest::Client::new();
    let response = client
        .post(format!("{base}/challenge"))
        .json(&json!({}))
        .send()
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(response.headers()["cache-control"], "no-store");
    let challenge: Value = response.json().await.unwrap();
    let mut payload = claims();
    payload["nonce"] = json!(crate::google_login::login_nonce(
        "https://match.example",
        challenge["challenge"].as_str().unwrap()
    )
    .unwrap());
    let exchange =
        json!({"challenge":challenge["challenge"], "google_id_token":token("key-1", &payload)});
    let response = client
        .post(format!("{base}/google"))
        .json(&exchange)
        .send()
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(response.headers()["cache-control"], "no-store");
    let credential: Value = response.json().await.unwrap();
    let secret = credential["session_token"].as_str().unwrap();
    let login = state.google_login.as_ref().unwrap();
    assert!(login
        .credential(secret, std::time::Instant::now())
        .is_some());
    let (mut ws, _) = tokio_tungstenite::connect_async(format!("ws://{address}/ws"))
        .await
        .unwrap();
    let authenticate = json!({"type":"AUTHENTICATE", "protocol_version":crate::protocol::CURRENT_PROTOCOL,
        "client_version":"fixture", "callsign":"pilot", "platform":"test",
        "auth_method":"google_session", "session_credential":secret});
    ws.send(Message::Text(authenticate.to_string().into()))
        .await
        .unwrap();
    let reply = tokio::time::timeout(Duration::from_secs(2), ws.next())
        .await
        .unwrap()
        .unwrap()
        .unwrap();
    let reply: Value = serde_json::from_str(reply.to_text().unwrap()).unwrap();
    assert_eq!(reply["type"], "AUTH_OK");
    let pid = uuid::Uuid::parse_str(reply["player_id"].as_str().unwrap()).unwrap();
    assert!(state.sessions.get(&pid).unwrap().account_verified);
    assert_eq!(
        state.sessions.get(&pid).unwrap().auth_method,
        "google_session"
    );
    assert_eq!(
        client
            .post(format!("{base}/google"))
            .json(&exchange)
            .send()
            .await
            .unwrap()
            .status(),
        StatusCode::UNAUTHORIZED
    );
    for endpoint in ["cancel", "cancel", "logout", "logout"] {
        let body = if endpoint == "cancel" {
            json!({"challenge":challenge["challenge"]})
        } else {
            json!({"session_token":secret})
        };
        assert_eq!(
            client
                .post(format!("{base}/{endpoint}"))
                .json(&body)
                .send()
                .await
                .unwrap()
                .status(),
            StatusCode::NO_CONTENT
        );
    }
    assert!(login
        .credential(secret, std::time::Instant::now())
        .is_none());
    tokio::time::timeout(Duration::from_secs(3), async {
        while let Some(Ok(message)) = ws.next().await {
            if message.is_close() {
                break;
            }
        }
    })
    .await
    .unwrap();
    assert!(!state.sessions.contains_key(&pid));
    let (mut ws, _) = tokio_tungstenite::connect_async(format!("ws://{address}/ws"))
        .await
        .unwrap();
    ws.send(Message::Text(authenticate.to_string().into()))
        .await
        .unwrap();
    let reply = tokio::time::timeout(Duration::from_secs(2), ws.next())
        .await
        .unwrap()
        .unwrap()
        .unwrap();
    let reply: Value = serde_json::from_str(reply.to_text().unwrap()).unwrap();
    assert_eq!(reply["type"], "AUTH_FAIL");
    assert_eq!(
        client
            .post(format!("{base}/challenge"))
            .body("{}")
            .send()
            .await
            .unwrap()
            .status(),
        StatusCode::UNSUPPORTED_MEDIA_TYPE
    );
    assert_eq!(
        client
            .post(format!("{base}/google"))
            .header("content-type", "application/json")
            .body("x".repeat(12 * 1024 + 1))
            .send()
            .await
            .unwrap()
            .status(),
        StatusCode::PAYLOAD_TOO_LARGE
    );
    assert_eq!(
        client
            .post(format!("{base}/google"))
            .json(&json!({"unexpected":"field"}))
            .send()
            .await
            .unwrap()
            .status(),
        StatusCode::BAD_REQUEST
    );
    let permit = state.auth_slots.acquire_many(8).await.unwrap();
    let response = client
        .post(format!("{base}/challenge"))
        .json(&json!({}))
        .send()
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
    assert_eq!(response.headers()["cache-control"], "no-store");
    drop(permit);
    // A client that never finishes its body cannot hold an auth permit indefinitely
    {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};
        let mut slow = tokio::net::TcpStream::connect(address).await.unwrap();
        slow.write_all(b"POST /api/v1/auth/google HTTP/1.1\r\nHost: fixture\r\nContent-Type: application/json\r\nContent-Length: 1000\r\n\r\n{").await.unwrap();
        let mut buffer = [0u8; 512];
        let count = tokio::time::timeout(Duration::from_secs(7), slow.read(&mut buffer))
            .await
            .unwrap()
            .unwrap();
        assert!(std::str::from_utf8(&buffer[..count])
            .unwrap()
            .starts_with("HTTP/1.1 408"));
        assert_eq!(state.auth_slots.available_permits(), 8);
    }
    while state
        .rate_limiter
        .check_ip_login("127.0.0.1".parse().unwrap())
    {}
    // An untrusted peer cannot evade the source quota with a forwarding header
    assert_eq!(
        client
            .post(format!("{base}/challenge"))
            .header("x-real-ip", "192.0.2.8")
            .json(&json!({}))
            .send()
            .await
            .unwrap()
            .status(),
        StatusCode::TOO_MANY_REQUESTS
    );
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 1);
}

fn claims() -> Value {
    let now = jsonwebtoken::get_current_timestamp();
    json!({"iss":"https://accounts.google.com","aud":AUDIENCE,"sub":"google-account-123",
        "iat":now,"exp":now+3600,"nonce":NONCE})
}

fn token(kid: &str, claims: &Value) -> String {
    let mut header = Header::new(Algorithm::RS256);
    header.kid = Some(kid.into());
    signed(&header, claims)
}

fn signed(header: &Header, claims: &Value) -> String {
    encode(
        header,
        claims,
        &EncodingKey::from_rsa_pem(PRIVATE_KEY.as_bytes()).unwrap(),
    )
    .unwrap()
}

struct FixtureState {
    requests: AtomicUsize,
    delay_ms: AtomicU64,
    body: Mutex<String>,
    status: Mutex<axum::http::StatusCode>,
    cache_control: Mutex<String>,
}

struct Fixture {
    state: Arc<FixtureState>,
    verifier: Arc<GoogleVerifier>,
    task: tokio::task::JoinHandle<()>,
}

impl Drop for Fixture {
    fn drop(&mut self) {
        self.task.abort();
    }
}

impl Fixture {
    async fn new() -> Self {
        let state = Arc::new(FixtureState {
            requests: AtomicUsize::new(0),
            delay_ms: AtomicU64::new(0),
            body: Mutex::new(json!({"keys":[jwk("key-1")]}).to_string()),
            status: Mutex::new(axum::http::StatusCode::OK),
            cache_control: Mutex::new("public, max-age=3600".into()),
        });
        let app = Router::new()
            .route(
                "/keys",
                get(|State(state): State<Arc<FixtureState>>| async move {
                    state.requests.fetch_add(1, Ordering::SeqCst);
                    tokio::time::sleep(Duration::from_millis(
                        state.delay_ms.load(Ordering::SeqCst),
                    ))
                    .await;
                    let status = *state.status.lock().unwrap();
                    let cache_control = state.cache_control.lock().unwrap().clone();
                    let body = state.body.lock().unwrap().clone();
                    (status, [("cache-control", cache_control)], body).into_response()
                }),
            )
            .with_state(Arc::clone(&state));
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let url = format!("http://{}/keys", listener.local_addr().unwrap());
        let task = tokio::spawn(async { axum::serve(listener, app).await.unwrap() });
        Self {
            state,
            verifier: Arc::new(GoogleVerifier::build(AUDIENCE, &url).unwrap()),
            task,
        }
    }

    async fn verify(&self, token: &str) -> Result<GoogleIdentity, VerifyError> {
        self.verifier
            .verify(token, NONCE, Instant::now() + Duration::from_secs(2))
            .await
    }

    fn allow_refresh(&self, expire: bool) {
        let mut cache = self.verifier.cache.lock().unwrap();
        cache.next_refresh = Instant::now();
        if expire {
            cache.expires = Instant::now();
        }
    }
}

#[tokio::test]
#[ignore = "Owned JVM HTTPS integration fixture; launched explicitly by GoogleLoginHttpsTest"]
async fn android_https_login_fixture() {
    use axum::{extract::Query, routing::post, Json};
    use std::io::Write;
    let fixture = Fixture::new().await;
    let origin = std::env::var("DXX_GOOGLE_FIXTURE_ORIGIN").expect("fixture origin required");
    let mut state = login_server_state(fixture.verifier.clone());
    let mutable = Arc::get_mut(&mut state).unwrap();
    mutable.config.skip_gpgs_verify = false;
    mutable.google_login = Some(
        crate::google_login::GoogleLogin::with_verifier(&origin, fixture.verifier.clone()).unwrap(),
    );
    let check_state = state.clone();
    // Signing and lookup helpers exist only in this ignored test binary, never production routes
    let app = crate::ws_handler::ws_router(state)
        .route("/fixture/token", axum::routing::get(|Query(query): Query<std::collections::HashMap<String, String>>| async move {
            let mut payload = claims();
            payload["nonce"] = json!(query.get("nonce").unwrap());
            token("key-1", &payload)
        }))
        .route("/fixture/check", post(move |Json(body): Json<Value>| {
            let state = check_state.clone();
            async move {
                Json(json!({"active": state.google_login.as_ref().unwrap().credential(body["session_token"].as_str().unwrap(), std::time::Instant::now()).is_some()}))
            }
        }));
    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    println!(
        "DXX_GOOGLE_FIXTURE_PORT={}",
        listener.local_addr().unwrap().port()
    );
    std::io::stdout().flush().unwrap();
    let server = axum::serve(
        listener,
        app.into_make_service_with_connect_info::<std::net::SocketAddr>(),
    );
    tokio::select! {
        result = server => result.unwrap(),
        _ = tokio::time::sleep(Duration::from_secs(120)) => {},
    }
}

#[tokio::test]
async fn login_cancellation_revokes_lost_responses_and_inflight_verification() {
    use crate::google_login::{login_nonce, GoogleLogin, LoginError};
    let fixture = Fixture::new().await;
    fixture.state.delay_ms.store(100, Ordering::SeqCst);
    let origin = "https://match.example";
    let login = GoogleLogin::with_verifier(origin, fixture.verifier.clone()).unwrap();
    let source = "127.0.0.1".parse().unwrap();
    let challenge = login
        .challenge(source, std::time::Instant::now())
        .unwrap()
        .challenge;
    let mut payload = claims();
    payload["nonce"] = json!(login_nonce(origin, &challenge).unwrap());
    let proof = token("key-1", &payload);
    let (result, ()) = tokio::join!(
        login.exchange(&challenge, &proof, Instant::now() + Duration::from_secs(2)),
        async {
            tokio::time::timeout(Duration::from_secs(1), async {
                while fixture.state.requests.load(Ordering::SeqCst) == 0 {
                    tokio::task::yield_now().await;
                }
            })
            .await
            .unwrap();
            login.cancel(&challenge);
        }
    );
    assert!(matches!(result, Err(LoginError::InvalidCredential)));

    // More than both account-session and source-challenge capacity proves cleanup recovers slots
    for _ in 0..20 {
        let challenge = login
            .challenge(source, std::time::Instant::now())
            .unwrap()
            .challenge;
        payload["nonce"] = json!(login_nonce(origin, &challenge).unwrap());
        let proof = token("key-1", &payload);
        let credential = login
            .exchange(&challenge, &proof, Instant::now() + Duration::from_secs(2))
            .await
            .unwrap();
        login.cancel(&"f".repeat(64));
        assert!(login
            .credential(&credential.session_token, std::time::Instant::now())
            .is_some());
        // The client need not know the issued secret to revoke the lost response
        login.cancel(&challenge);
        login.cancel(&challenge);
        assert!(login
            .credential(&credential.session_token, std::time::Instant::now())
            .is_none());
        assert!(matches!(
            login
                .exchange(&challenge, &proof, Instant::now() + Duration::from_secs(2))
                .await,
            Err(LoginError::InvalidCredential)
        ));
    }
    let challenge = login
        .challenge(source, std::time::Instant::now())
        .unwrap()
        .challenge;
    login.cancel(&challenge);
    payload["nonce"] = json!(login_nonce(origin, &challenge).unwrap());
    assert!(matches!(
        login
            .exchange(
                &challenge,
                &token("key-1", &payload),
                Instant::now() + Duration::from_secs(2)
            )
            .await,
        Err(LoginError::InvalidCredential)
    ));
}

#[tokio::test]
async fn login_exchange_consumes_challenges_and_binds_recipient() {
    use crate::google_login::{login_nonce, GoogleLogin, LoginError};
    let fixture = Fixture::new().await;
    let origin = "https://match.example";
    let login = GoogleLogin::with_verifier(origin, fixture.verifier.clone()).unwrap();
    let source = "127.0.0.1".parse().unwrap();
    let challenge = login
        .challenge(source, std::time::Instant::now())
        .unwrap()
        .challenge;
    let mut payload = claims();
    payload["nonce"] = json!(login_nonce(origin, &challenge).unwrap());
    let proof = token("key-1", &payload);
    let deadline = Instant::now() + Duration::from_secs(2);
    let (first, second) = tokio::join!(
        login.exchange(&challenge, &proof, deadline),
        login.exchange(&challenge, &proof, deadline),
    );
    assert_eq!(usize::from(first.is_ok()) + usize::from(second.is_ok()), 1);
    let credential = first.or(second).unwrap();
    let identity = login
        .credential(&credential.session_token, std::time::Instant::now())
        .unwrap();
    assert_eq!(identity.subject, "google-account-123");
    assert!(login
        .credential(&credential.session_token, identity.expires)
        .is_none());
    assert!(login
        .credential(&proof, std::time::Instant::now())
        .is_none());
    login.revoke(&credential.session_token);
    assert!(login
        .credential(&credential.session_token, std::time::Instant::now())
        .is_none());

    // A malicious destination cannot forward its token to the honest destination
    let challenge = login
        .challenge(source, std::time::Instant::now())
        .unwrap()
        .challenge;
    payload["nonce"] = json!(login_nonce("https://other.example", &challenge).unwrap());
    assert!(matches!(
        login
            .exchange(&challenge, &token("key-1", &payload), deadline)
            .await,
        Err(LoginError::InvalidCredential)
    ));
    payload["nonce"] = json!(login_nonce(origin, &challenge).unwrap());
    assert!(matches!(
        login
            .exchange(&challenge, &token("key-1", &payload), deadline)
            .await,
        Err(LoginError::InvalidCredential)
    ));
    let expired = login
        .challenge(source, std::time::Instant::now() - Duration::from_secs(301))
        .unwrap();
    payload["nonce"] = json!(login_nonce(origin, &expired.challenge).unwrap());
    assert!(matches!(
        login
            .exchange(&expired.challenge, &token("key-1", &payload), deadline)
            .await,
        Err(LoginError::InvalidCredential)
    ));
}

#[tokio::test]
async fn verified_google_sessions_share_account_mapping_and_bans() {
    use crate::google_login::{login_nonce, GoogleLogin, LoginError};
    let fixture = Fixture::new().await;
    let origin = "https://match.example";
    let login = GoogleLogin::with_verifier(origin, fixture.verifier.clone()).unwrap();
    let db = crate::db::Database::open(":memory:").unwrap();
    let mut account = None;
    let mut credentials = Vec::new();
    for index in 0..9 {
        let challenge = login
            .challenge("127.0.0.1".parse().unwrap(), std::time::Instant::now())
            .unwrap();
        let mut payload = claims();
        payload["nonce"] = json!(login_nonce(origin, &challenge.challenge).unwrap());
        let result = login
            .exchange(
                &challenge.challenge,
                &token("key-1", &payload),
                Instant::now() + Duration::from_secs(2),
            )
            .await;
        if index == 8 {
            assert!(matches!(result, Err(LoginError::Capacity)));
            break;
        }
        let credential = result.unwrap();
        let identity = login
            .credential(&credential.session_token, std::time::Instant::now())
            .unwrap();
        let pid = db
            .find_or_create_player_by_google(&identity.subject, &format!("pilot-{index}"))
            .unwrap();
        if let Some(expected) = account {
            assert_eq!(pid, expected);
            assert!(db.is_banned(&pid).unwrap());
        } else {
            account = Some(pid);
            db.ban_player(&pid, "fixture ban", None).unwrap();
        }
        credentials.push(credential.session_token);
    }
    // Neither identical names nor identical provider ID strings link different namespaces
    let old_pgs = db
        .find_or_create_player_by_gpgs("google-account-123", "pilot-7")
        .unwrap();
    assert_ne!(Some(old_pgs), account);
    let other_google = db
        .find_or_create_player_by_google("other-account", "pilot-7")
        .unwrap();
    assert_ne!(Some(other_google), account);
    login.revoke(&credentials[0]);
    let challenge = login
        .challenge("127.0.0.1".parse().unwrap(), std::time::Instant::now())
        .unwrap();
    let mut payload = claims();
    payload["nonce"] = json!(login_nonce(origin, &challenge.challenge).unwrap());
    assert!(login
        .exchange(
            &challenge.challenge,
            &token("key-1", &payload),
            Instant::now() + Duration::from_secs(2)
        )
        .await
        .is_ok());
}

#[tokio::test]
async fn signed_google_tokens_require_all_identity_and_binding_claims() {
    let fixture = Fixture::new().await;
    let valid = claims();
    let identity = fixture.verify(&token("key-1", &valid)).await.unwrap();
    assert_eq!(identity.subject(), "google-account-123");
    assert_eq!(identity.expires_at(), valid["exp"].as_u64().unwrap());
    let mut alternate_issuer = valid.clone();
    alternate_issuer["iss"] = json!("accounts.google.com");
    assert_eq!(
        fixture
            .verify(&token("key-1", &alternate_issuer))
            .await
            .unwrap()
            .subject(),
        identity.subject()
    );
    for field in ["iss", "aud", "sub", "exp", "iat", "nonce"] {
        let mut missing = valid.clone();
        missing.as_object_mut().unwrap().remove(field);
        assert!(
            fixture.verify(&token("key-1", &missing)).await.is_err(),
            "missing {field}"
        );
    }
    for (field, value) in [
        ("iss", json!("https://attacker.invalid")),
        ("aud", json!("other-client.apps.googleusercontent.com")),
        ("aud", json!([AUDIENCE, "other-client"])),
        ("nonce", json!("a-different-login-nonce-with-32-bytes")),
        ("sub", json!("")),
        ("sub", json!("x".repeat(256))),
        ("sub", json!("bad\nsubject")),
        ("exp", json!(1)),
        ("exp", json!("9999999999")),
        ("iat", json!(jsonwebtoken::get_current_timestamp() + 300)),
        ("nbf", json!(jsonwebtoken::get_current_timestamp() + 300)),
    ] {
        let mut invalid = valid.clone();
        invalid[field] = value;
        assert!(
            fixture.verify(&token("key-1", &invalid)).await.is_err(),
            "invalid {field}"
        );
    }
    let mut forged = token("key-1", &valid).into_bytes();
    let signature_start = forged.iter().rposition(|c| *c == b'.').unwrap() + 1;
    forged[signature_start] = if forged[signature_start] == b'A' {
        b'B'
    } else {
        b'A'
    };
    assert!(fixture
        .verify(std::str::from_utf8(&forged).unwrap())
        .await
        .is_err());
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 1);
}

#[tokio::test]
async fn hostile_headers_are_rejected_without_network_requests() {
    let fixture = Fixture::new().await;
    let mut header = Header::new(Algorithm::RS256);
    header.kid = Some("key-1".into());
    for field in ["jku", "x5u", "crit", "kid"] {
        let mut invalid = header.clone();
        match field {
            "jku" => invalid.jku = Some("http://127.0.0.1/private".into()),
            "x5u" => invalid.x5u = Some("http://127.0.0.1/private".into()),
            "crit" => invalid.crit = Some(vec!["unknown".into()]),
            _ => invalid.kid = Some("x".repeat(129)),
        }
        assert!(fixture.verify(&signed(&invalid, &claims())).await.is_err());
    }
    let mut hmac = Header::new(Algorithm::HS256);
    hmac.kid = Some("key-1".into());
    let confused = encode(
        &hmac,
        &claims(),
        &EncodingKey::from_secret(MODULUS.as_bytes()),
    )
    .unwrap();
    for invalid in [
        confused,
        "not.a.token".into(),
        "x".repeat(MAX_TOKEN_BYTES + 1),
    ] {
        assert!(fixture.verify(&invalid).await.is_err());
    }
    assert!(fixture
        .verifier
        .verify(
            &token("key-1", &claims()),
            "",
            Instant::now() + Duration::from_secs(1)
        )
        .await
        .is_err());
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 0);
}

#[tokio::test]
async fn concurrent_unknown_keys_have_one_refresh_and_rotation_replaces_keys() {
    let fixture = Fixture::new().await;
    assert!(fixture.verify(&token("key-1", &claims())).await.is_ok());
    *fixture.state.body.lock().unwrap() = json!({"keys":[jwk("key-2")]}).to_string();
    fixture.allow_refresh(false);
    let mut tasks = tokio::task::JoinSet::new();
    for index in 0..100 {
        let verifier = Arc::clone(&fixture.verifier);
        let token = token(&format!("unknown-{index}"), &claims());
        tasks.spawn(async move {
            assert!(verifier
                .verify(&token, NONCE, Instant::now() + Duration::from_secs(2))
                .await
                .is_err());
        });
    }
    while let Some(result) = tasks.join_next().await {
        result.unwrap();
    }
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 2);
    assert!(fixture.verify(&token("key-2", &claims())).await.is_ok());
    assert!(fixture.verify(&token("key-1", &claims())).await.is_err());
    assert_eq!(fixture.verifier.cache.lock().unwrap().keys.len(), 1);

    fixture.allow_refresh(true);
    *fixture.state.status.lock().unwrap() = axum::http::StatusCode::SERVICE_UNAVAILABLE;
    assert!(fixture.verify(&token("key-2", &claims())).await.is_err());
    assert!(fixture.verify(&token("key-2", &claims())).await.is_err());
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 3);
    fixture.allow_refresh(true);
    *fixture.state.status.lock().unwrap() = axum::http::StatusCode::OK;
    assert!(fixture.verify(&token("key-2", &claims())).await.is_ok());
}

#[tokio::test]
async fn slow_refresh_does_not_block_valid_cached_keys_and_deadline_consumes_budget() {
    let fixture = Fixture::new().await;
    let valid = token("key-1", &claims());
    assert!(fixture.verify(&valid).await.is_ok());
    fixture.allow_refresh(false);
    fixture.state.delay_ms.store(1000, Ordering::SeqCst);
    let verifier = Arc::clone(&fixture.verifier);
    let pending = tokio::spawn(async move {
        verifier
            .verify(
                &token("unknown", &claims()),
                NONCE,
                Instant::now() + Duration::from_millis(250),
            )
            .await
            .err()
    });
    tokio::time::timeout(Duration::from_secs(1), async {
        while fixture.state.requests.load(Ordering::SeqCst) != 2 {
            tokio::time::sleep(Duration::from_millis(1)).await;
        }
    })
    .await
    .unwrap();
    assert!(
        tokio::time::timeout(Duration::from_millis(100), fixture.verify(&valid))
            .await
            .unwrap()
            .is_ok()
    );
    assert_eq!(pending.await.unwrap(), Some(VerifyError::Deadline));
    assert!(fixture
        .verify(&token("unknown-again", &claims()))
        .await
        .is_err());
    assert_eq!(fixture.state.requests.load(Ordering::SeqCst), 2);
    assert!(fixture.verify(&valid).await.is_ok());
    assert!(matches!(
        fixture.verifier.verify(&valid, NONCE, Instant::now()).await,
        Err(VerifyError::Deadline)
    ));
}

#[tokio::test]
async fn malformed_and_uncacheable_key_responses_fail_closed() {
    let fixture = Fixture::new().await;
    let valid = token("key-1", &claims());
    let mut wrong_type = jwk("key-1");
    wrong_type["kty"] = json!("oct");
    for body in [
        "x".repeat(MAX_KEYS_BYTES + 1),
        "not json".into(),
        json!({"keys":[]}).to_string(),
        json!({"keys":[jwk("key-1"),jwk("key-1")]}).to_string(),
        json!({"keys":(0..17).map(|i| jwk(&format!("key-{i}"))).collect::<Vec<_>>()} ).to_string(),
        json!({"keys":[wrong_type]}).to_string(),
    ] {
        fixture.allow_refresh(true);
        *fixture.state.body.lock().unwrap() = body;
        assert!(fixture.verify(&valid).await.is_err());
    }
    *fixture.state.body.lock().unwrap() = json!({"keys":[jwk("key-1")]}).to_string();
    for policy in ["no-store", "public, max-age=0", "max-age=invalid"] {
        fixture.allow_refresh(true);
        *fixture.state.cache_control.lock().unwrap() = policy.into();
        assert!(fixture.verify(&valid).await.is_err());
    }
    let mut headers = reqwest::header::HeaderMap::new();
    headers.insert(
        reqwest::header::CACHE_CONTROL,
        "public, max-age=120".parse().unwrap(),
    );
    headers.insert(reqwest::header::AGE, "90".parse().unwrap());
    assert_eq!(cache_lifetime(&headers), Duration::from_secs(30));
    headers.insert(reqwest::header::AGE, "121".parse().unwrap());
    assert_eq!(cache_lifetime(&headers), Duration::ZERO);
    headers.remove(reqwest::header::AGE);
    headers.insert(
        reqwest::header::CACHE_CONTROL,
        "max-age=0, max-age=3600".parse().unwrap(),
    );
    assert_eq!(cache_lifetime(&headers), Duration::ZERO);
}
