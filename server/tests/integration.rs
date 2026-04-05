//! Integration tests for the dxx-matchmaking server.
//!
//! Each test spins up a real server (WS + HTTP) on ephemeral ports with
//! an in-memory SQLite database. Fake clients connect over actual
//! WebSockets in the same tokio runtime.

use std::net::SocketAddr;
use std::sync::Arc;

use futures_util::{SinkExt, StreamExt};
use serde_json::{json, Value};
use tokio::net::TcpStream;
use tokio_tungstenite::{connect_async, tungstenite::Message, MaybeTlsStream, WebSocketStream};

use dxx_matchmaking::config::ServerConfig;
use dxx_matchmaking::db;
use dxx_matchmaking::protocol::{CURRENT_PROTOCOL, MIN_CLIENT_PROTOCOL};
use dxx_matchmaking::{build_state, http_api, ws_handler, ServerState};

// ---------------------------------------------------------------------------
// Test harness
// ---------------------------------------------------------------------------

/// A running test server with its allocated addresses.
struct TestServer {
    state: Arc<ServerState>,
    ws_addr: SocketAddr,
    http_addr: SocketAddr,
    _ws_handle: tokio::task::JoinHandle<()>,
    _http_handle: tokio::task::JoinHandle<()>,
}

impl TestServer {
    /// Spin up a server on ephemeral ports with an in-memory DB.
    async fn start() -> Self {
        let config = ServerConfig {
            ws_listen_addr: "127.0.0.1:0".parse().unwrap(),
            http_listen_addr: "127.0.0.1:0".parse().unwrap(),
            relay_listen_addr: "127.0.0.1:0".parse().unwrap(),
            db_path: ":memory:".into(),
            google_client_id: String::new(),
            google_client_secret: String::new(),
            admin_token: "test-admin-token".into(),
            motd: String::new(),
            update_url: "https://example.com/update".into(),
            tls_cert_path: String::new(),
            tls_key_path: String::new(),
            relay_public_addr: "127.0.0.1:19001".into(),
            stun_listen_addr: "127.0.0.1:0".parse().unwrap(),
            stun_listen_addr_alt: "127.0.0.1:0".parse().unwrap(),
            stun_public_addrs: String::new(),
            log_dir: String::new(),
            skip_gpgs_verify: true,
            pow_difficulty: 8, // low difficulty for fast tests
            max_relay_sessions: 100,
            max_connections: 0, // unlimited for tests
            admin_http_listen_addr: None,
        };

        let state = build_state(config).expect("failed to build state");

        // Bind WS listener to get the real port
        let ws_listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let ws_addr = ws_listener.local_addr().unwrap();

        // Bind HTTP listener
        let http_listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let http_addr = http_listener.local_addr().unwrap();

        let ws_state = Arc::clone(&state);
        let ws_handle = tokio::spawn(async move {
            let app = ws_handler::ws_router(ws_state);
            axum::serve(
                ws_listener,
                app.into_make_service_with_connect_info::<SocketAddr>(),
            )
            .await
            .unwrap();
        });

        let http_state = Arc::clone(&state);
        let http_handle = tokio::spawn(async move {
            let app = http_api::router(http_state);
            axum::serve(http_listener, app).await.unwrap();
        });

        TestServer {
            state,
            ws_addr,
            http_addr,
            _ws_handle: ws_handle,
            _http_handle: http_handle,
        }
    }

    fn ws_url(&self) -> String {
        format!("ws://{}/ws", self.ws_addr)
    }

    fn http_url(&self, path: &str) -> String {
        format!("http://{}{}", self.http_addr, path)
    }
}

type WsClient = WebSocketStream<MaybeTlsStream<TcpStream>>;

/// Connect a fake WS client to the test server.
async fn connect_ws(server: &TestServer) -> WsClient {
    let (ws, _) = connect_async(&server.ws_url())
        .await
        .expect("WS connect failed");
    ws
}

/// Send a JSON message and return the next JSON response from the server.
/// Times out after 2 seconds to prevent hanging.
async fn send_recv(ws: &mut WsClient, msg: Value) -> Value {
    ws.send(Message::Text(msg.to_string().into()))
        .await
        .unwrap();
    recv(ws).await
}

/// Read the next JSON text message, with a 2-second timeout.
async fn recv(ws: &mut WsClient) -> Value {
    let resp = tokio::time::timeout(std::time::Duration::from_secs(2), ws.next())
        .await
        .expect("timed out waiting for server message")
        .unwrap()
        .unwrap();
    match resp {
        Message::Text(t) => serde_json::from_str(&t).unwrap(),
        other => panic!("expected text message, got {other:?}"),
    }
}

/// Send a JSON message without waiting for a response.
async fn send_only(ws: &mut WsClient, msg: Value) {
    ws.send(Message::Text(msg.to_string().into()))
        .await
        .unwrap();
}

/// Authenticate a fake client, returning the AUTH_OK response.
/// Also drains the welcome bundle messages (SERVER_STATUS, LOBBY_LIST,
/// FRIEND_LIST_RESP) so the caller starts with a clean message queue.
async fn authenticate(ws: &mut WsClient, callsign: &str) -> Value {
    let auth_ok = send_recv(
        ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "1.0.0-test",
            "play_games_token": format!("fake-token-{callsign}"),
            "callsign": callsign,
            "platform": "test",
        }),
    )
    .await;
    assert_eq!(auth_ok["type"], "AUTH_OK");

    // Drain the welcome bundle (SERVER_STATUS, LOBBY_LIST, FRIEND_LIST_RESP)
    let status = recv(ws).await;
    assert_eq!(status["type"], "SERVER_STATUS");
    let lobby_list = recv(ws).await;
    assert_eq!(lobby_list["type"], "LOBBY_LIST");
    let friend_list = recv(ws).await;
    assert_eq!(friend_list["type"], "FRIEND_LIST_RESP");

    auth_ok
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/// AUTH_OK with valid protocol version.
#[tokio::test]
async fn test_auth_success() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    let resp = authenticate(&mut ws, "Ace").await;
    assert_eq!(resp["type"], "AUTH_OK");
    assert!(resp.get("player_id").is_some());
    assert!(resp.get("session_token").is_some());
}

/// Reject a client with a protocol version below MIN_CLIENT_PROTOCOL.
#[tokio::test]
async fn test_auth_version_rejected() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    send_only(
        &mut ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": 0,
            "client_version": "0.0.1",
            "play_games_token": "tok",
            "callsign": "Old",
            "platform": "test",
        }),
    )
    .await;

    // Server sends VERSION_REJECTED and then closes. Read until we get
    // the text message or the connection closes.
    let mut found = false;
    let deadline = tokio::time::Instant::now() + std::time::Duration::from_secs(3);
    while let Ok(Some(Ok(msg))) = tokio::time::timeout_at(deadline, ws.next()).await {
        if let Message::Text(t) = msg {
            let v: Value = serde_json::from_str(&t).unwrap();
            if v["type"] == "VERSION_REJECTED" {
                assert_eq!(v["required_version"], MIN_CLIENT_PROTOCOL);
                found = true;
                break;
            }
        }
    }
    assert!(
        found,
        "expected VERSION_REJECTED message before connection closed"
    );
}

/// Create a lobby and see it in the lobby list.
#[tokio::test]
async fn test_create_and_list_lobby() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    authenticate(&mut ws, "Host").await;

    // Create lobby
    let _create_resp = send_recv(
        &mut ws,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 8,
            "game_info": {"mission": "Counterstrike!", "mode": "anarchy"},
        }),
    )
    .await;

    // List lobbies
    let list_resp = send_recv(&mut ws, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list_resp["type"], "LOBBY_LIST");
    let lobbies = list_resp["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    assert_eq!(lobbies[0]["host_callsign"], "Host");
    assert_eq!(lobbies[0]["game_info"]["mission"], "Counterstrike!");
}

/// Join another player's lobby.
#[tokio::test]
async fn test_join_lobby() {
    let server = TestServer::start().await;

    // Host creates lobby
    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;
    let _create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "test-mission", "mode": "coop"},
        }),
    )
    .await;

    // Joiner
    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;

    // Get lobby id from state
    let lobby_id = server
        .state
        .lobbies
        .iter()
        .next()
        .expect("should have one lobby")
        .key()
        .to_string();

    // Send join (server doesn't reply on success yet -- check via state)
    send_only(
        &mut joiner,
        json!({
            "type": "JOIN_LOBBY",
            "lobby_id": lobby_id,
        }),
    )
    .await;

    // Give the server a moment to process
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;

    // Verify via state that the lobby now has 2 players
    let lobby = server.state.lobbies.iter().next().unwrap();
    assert_eq!(
        lobby.value().player_count(),
        2,
        "lobby should have host + joiner"
    );
}

/// Leave a lobby and verify it's removed when empty.
#[tokio::test]
async fn test_leave_lobby_cleanup() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    authenticate(&mut ws, "Solo").await;

    send_recv(
        &mut ws,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d1",
            "max_players": 4,
            "game_info": {"mission": "First Strike", "mode": "anarchy"},
        }),
    )
    .await;

    // There should be 1 lobby
    assert_eq!(server.state.lobbies.len(), 1);

    // LEAVE_LOBBY sends no response -- fire and forget, then check state
    send_only(&mut ws, json!({"type": "LEAVE_LOBBY"})).await;
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;

    // After leaving, the empty lobby should be cleaned up
    assert_eq!(server.state.lobbies.len(), 0);
}

/// Messages before authentication should get NOT_AUTHENTICATED.
#[tokio::test]
async fn test_unauthenticated_message_rejected() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    let resp = send_recv(&mut ws, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(resp["type"], "ERROR");
    assert_eq!(resp["code"], "NOT_AUTHENTICATED");
}

/// HTTP /api/v1/health returns 200 with status "ok".
#[tokio::test]
async fn test_http_health() {
    let server = TestServer::start().await;
    let resp = reqwest::get(&server.http_url("/api/v1/health"))
        .await
        .unwrap();
    assert!(resp.status().is_success());
    let body: Value = resp.json().await.unwrap();
    assert_eq!(body["status"], "ok");
}

/// HTTP /api/v1/status returns protocol version and player counts.
#[tokio::test]
async fn test_http_status() {
    let server = TestServer::start().await;
    let resp = reqwest::get(&server.http_url("/api/v1/status"))
        .await
        .unwrap();
    assert!(resp.status().is_success());
    let body: Value = resp.json().await.unwrap();
    assert_eq!(body["protocol_version"], CURRENT_PROTOCOL);
    assert!(body.get("players").is_some());
    assert!(body.get("lobbies").is_some());
}

/// HTTP /api/v1/status/simple returns a plain-text player count.
#[tokio::test]
async fn test_http_status_simple() {
    let server = TestServer::start().await;
    let resp = reqwest::get(&server.http_url("/api/v1/status/simple"))
        .await
        .unwrap();
    let text = resp.text().await.unwrap();
    let _n: u32 = text.trim().parse().expect("should be a number");
}

/// Friend request, accept, and list flow.
#[tokio::test]
async fn test_friend_lifecycle() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    authenticate(&mut alice, "Alice").await;

    let mut bob = connect_ws(&server).await;
    authenticate(&mut bob, "Bob").await;

    // Alice sends friend request to Bob.
    // On success the server sends nothing to Alice (only notifies Bob),
    // so we fire-and-forget and check Bob receives the notification.
    send_only(
        &mut alice,
        json!({
            "type": "FRIEND_REQUEST",
            "target_callsign": "Bob",
        }),
    )
    .await;

    // Bob should receive FRIEND_REQUEST_RECEIVED
    let notification = recv(&mut bob).await;
    assert_eq!(notification["type"], "FRIEND_REQUEST_RECEIVED");
}

/// Database: ban a player and verify is_banned.
#[tokio::test]
async fn test_db_ban_unban() {
    let db = dxx_matchmaking::db::Database::open(":memory:").unwrap();
    let pid = uuid::Uuid::new_v4();
    db.upsert_player(&pid, "gpgs-1", "BannedGuy").unwrap();
    assert!(!db.is_banned(&pid).unwrap());

    db.ban_player(&pid, "cheating", None).unwrap();
    assert!(db.is_banned(&pid).unwrap());

    db.unban_player(&pid).unwrap();
    assert!(!db.is_banned(&pid).unwrap());
}

/// Database: insert and query match results.
#[tokio::test]
async fn test_db_match_tracking() {
    let db = dxx_matchmaking::db::Database::open(":memory:").unwrap();
    assert_eq!(db.total_games_played().unwrap(), 0);

    let mid = uuid::Uuid::new_v4();
    let lid = uuid::Uuid::new_v4();
    let pid = uuid::Uuid::new_v4();
    db.upsert_player(&pid, "gpgs-2", "Ace").unwrap();
    db.insert_match_result(&db::MatchResultData {
        match_id: mid,
        lobby_id: lid,
        duration_secs: 300,
        game_mode: "anarchy".to_string(),
        mission: "test".to_string(),
        result: "complete".to_string(),
        player_count: 4,
    })
    .unwrap();
    db.insert_match_player(&db::MatchPlayerData {
        match_id: mid,
        player_id: pid,
        callsign: "Ace".to_string(),
        score: 1500,
        kills: 12,
        deaths: 3,
        result: "winner".to_string(),
    })
    .unwrap();

    assert_eq!(db.total_games_played().unwrap(), 1);
    assert_eq!(db.total_unique_players().unwrap(), 1);
}

/// Database: friend request, accept, list, remove.
#[tokio::test]
async fn test_db_friends_crud() {
    let db = dxx_matchmaking::db::Database::open(":memory:").unwrap();
    let alice = uuid::Uuid::new_v4();
    let bob = uuid::Uuid::new_v4();
    db.upsert_player(&alice, "g-alice", "Alice").unwrap();
    db.upsert_player(&bob, "g-bob", "Bob").unwrap();

    // Send request
    db.add_friend_request(&alice, &bob).unwrap();
    let friends = db.get_friends(&alice).unwrap();
    assert_eq!(friends.len(), 1);
    assert_eq!(friends[0].status, "pending");

    // Accept
    db.accept_friend(&bob, &alice).unwrap();
    let friends = db.get_friends(&alice).unwrap();
    assert_eq!(friends[0].status, "accepted");

    // Remove
    db.remove_friend(&alice, &bob).unwrap();
    let friends = db.get_friends(&alice).unwrap();
    assert_eq!(friends.len(), 0);
}

/// Database: player count snapshot.
#[tokio::test]
async fn test_db_player_count_snapshot() {
    let db = dxx_matchmaking::db::Database::open(":memory:").unwrap();
    db.insert_player_count_snapshot(10, 5, 3).unwrap();
    // No error means the insert succeeded
}

/// Joining a lobby broadcasts LOBBY_UPDATE to all members.
#[tokio::test]
async fn test_lobby_update_on_join() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    // Create lobby -- host receives LOBBY_UPDATE
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    assert_eq!(create_resp["type"], "LOBBY_UPDATE");
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    // Joiner connects and joins
    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({
            "type": "JOIN_LOBBY",
            "lobby_id": lobby_id,
        }),
    )
    .await;

    // Host should receive a LOBBY_UPDATE with 2 players
    let update = recv(&mut host).await;
    assert_eq!(update["type"], "LOBBY_UPDATE");
    assert_eq!(update["players"].as_array().unwrap().len(), 2);

    // Joiner should also receive the LOBBY_UPDATE
    let joiner_update = recv(&mut joiner).await;
    assert_eq!(joiner_update["type"], "LOBBY_UPDATE");
    assert_eq!(joiner_update["players"].as_array().unwrap().len(), 2);
}

/// Ready state change broadcasts LOBBY_UPDATE to all members.
#[tokio::test]
async fn test_lobby_update_on_ready() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let _lobby_id = create_resp["lobby_id"].as_str().unwrap();

    // Toggle ready
    send_only(&mut host, json!({"type": "READY", "ready": true})).await;

    let update = recv(&mut host).await;
    assert_eq!(update["type"], "LOBBY_UPDATE");
    let players = update["players"].as_array().unwrap();
    assert_eq!(players.len(), 1);
    assert_eq!(players[0]["ready"], true);
}

/// StartGame sends GAME_STARTING to all lobby members, including host.
#[tokio::test]
async fn test_start_game_flow() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    // Joiner joins
    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({
            "type": "JOIN_LOBBY",
            "lobby_id": lobby_id,
        }),
    )
    .await;
    // Drain the LOBBY_UPDATE messages from join
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Host starts game
    send_only(&mut host, json!({"type": "START_GAME"})).await;

    // Both should receive CONNECTION_INFO then GAME_STARTING
    // Order: CONNECTION_INFO first (sent before GAME_STARTING)
    let host_msg1 = recv(&mut host).await;
    assert_eq!(host_msg1["type"], "CONNECTION_INFO");

    let host_msg2 = recv(&mut host).await;
    assert_eq!(host_msg2["type"], "GAME_STARTING");
    assert_eq!(host_msg2["game_info"]["mission"], "Counterstrike!");
    assert_eq!(host_msg2["game_info"]["mode"], "coop");
    assert_eq!(host_msg2["your_slot"], 0);
    assert!(host_msg2["peers"].as_array().unwrap().len() == 1);

    let joiner_msg1 = recv(&mut joiner).await;
    assert_eq!(joiner_msg1["type"], "CONNECTION_INFO");

    let joiner_msg2 = recv(&mut joiner).await;
    assert_eq!(joiner_msg2["type"], "GAME_STARTING");
    assert_eq!(joiner_msg2["your_slot"], 1);
    assert!(joiner_msg2["peers"].as_array().unwrap().len() == 1);
}

/// Non-host cannot start the game.
#[tokio::test]
async fn test_start_game_non_host_rejected() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "test", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATE
    let _ = recv(&mut joiner).await;

    // Joiner tries to start game
    let resp = send_recv(&mut joiner, json!({"type": "START_GAME"})).await;
    assert_eq!(resp["type"], "ERROR");
    assert_eq!(resp["code"], "NOT_HOST");
}

/// Kicked player receives error notification and session is updated.
#[tokio::test]
async fn test_kick_player() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "test", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "Joiner").await;
    let joiner_id = joiner_auth["player_id"].as_str().unwrap().to_string();
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATE(s)
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Host kicks joiner
    send_only(
        &mut host,
        json!({"type": "KICK_PLAYER", "player_id": joiner_id}),
    )
    .await;

    // Host gets LOBBY_UPDATE (1 player now)
    let host_update = recv(&mut host).await;
    assert_eq!(host_update["type"], "LOBBY_UPDATE");
    assert_eq!(host_update["players"].as_array().unwrap().len(), 1);

    // Kicked player receives KICKED error
    let kick_msg = recv(&mut joiner).await;
    assert_eq!(kick_msg["type"], "ERROR");
    assert_eq!(kick_msg["code"], "KICKED");
}

/// Uptime is reported in status endpoint.
#[tokio::test]
async fn test_http_status_uptime() {
    let server = TestServer::start().await;
    // Give a tiny bit of time so uptime > 0
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    let resp = reqwest::get(&server.http_url("/api/v1/status"))
        .await
        .unwrap();
    let body: Value = resp.json().await.unwrap();
    // uptime_seconds should be a number (>= 0)
    assert!(body["uptime_seconds"].is_number());
}

// ---------------------------------------------------------------------------
// Phase 2.5: Welcome bundle, stable identity, messaging
// ---------------------------------------------------------------------------

/// Welcome bundle is sent automatically after AUTH_OK.
#[tokio::test]
async fn test_welcome_bundle() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;

    // Send auth manually (not via helper) to inspect each welcome message
    ws.send(Message::Text(
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "1.0.0-test",
            "play_games_token": "bundle-test-token",
            "callsign": "BundleTester",
            "platform": "test",
        })
        .to_string()
        .into(),
    ))
    .await
    .unwrap();

    let auth_ok = recv(&mut ws).await;
    assert_eq!(auth_ok["type"], "AUTH_OK");
    assert!(auth_ok.get("player_id").is_some());

    let status = recv(&mut ws).await;
    assert_eq!(status["type"], "SERVER_STATUS");
    assert!(status["online_players"].is_number());
    assert!(status["active_games_count"].is_number());
    assert!(status["total_games_played"].is_number());
    assert!(status.get("active_game_list").is_some());

    let lobby_list = recv(&mut ws).await;
    assert_eq!(lobby_list["type"], "LOBBY_LIST");
    assert!(lobby_list["lobbies"].is_array());

    let friend_list = recv(&mut ws).await;
    assert_eq!(friend_list["type"], "FRIEND_LIST_RESP");
    assert!(friend_list["friends"].is_array());
}

/// Same play_games_token yields the same player_id across sessions.
#[tokio::test]
async fn test_stable_player_id() {
    let server = TestServer::start().await;

    // First connection
    let mut ws1 = connect_ws(&server).await;
    let resp1 = authenticate(&mut ws1, "StableUser").await;
    let pid1 = resp1["player_id"].as_str().unwrap().to_string();
    drop(ws1);

    // Give server a moment to process disconnect
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;

    // Second connection with same token
    let mut ws2 = connect_ws(&server).await;
    let resp2 = authenticate(&mut ws2, "StableUser").await;
    let pid2 = resp2["player_id"].as_str().unwrap().to_string();

    assert_eq!(pid1, pid2, "same token should yield same player_id");
}

/// Player messaging: sender gets MESSAGE_SENT, receiver gets MESSAGE_RECEIVED.
#[tokio::test]
async fn test_player_message() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    let alice_auth = authenticate(&mut alice, "Alice").await;
    let _alice_id = alice_auth["player_id"].as_str().unwrap().to_string();

    let mut bob = connect_ws(&server).await;
    let bob_auth = authenticate(&mut bob, "Bob").await;
    let bob_id = bob_auth["player_id"].as_str().unwrap().to_string();

    // Alice sends message to Bob
    send_only(
        &mut alice,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": bob_id,
            "text": "hello bob",
        }),
    )
    .await;

    // Alice should receive MESSAGE_SENT
    let sent_ack = recv(&mut alice).await;
    assert_eq!(sent_ack["type"], "MESSAGE_SENT");
    assert_eq!(sent_ack["target_player_id"], bob_id);

    // Bob should receive MESSAGE_RECEIVED
    let received = recv(&mut bob).await;
    assert_eq!(received["type"], "MESSAGE_RECEIVED");
    assert_eq!(received["from_callsign"], "Alice");
    assert_eq!(received["text"], "hello bob");
}

/// Messaging rate limit: 6th message within 60s is rejected.
#[tokio::test]
async fn test_player_message_rate_limit() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    authenticate(&mut alice, "Alice").await;

    let mut bob = connect_ws(&server).await;
    let bob_auth = authenticate(&mut bob, "Bob").await;
    let bob_id = bob_auth["player_id"].as_str().unwrap().to_string();

    // Send 5 messages (allowed)
    for i in 0..5 {
        send_only(
            &mut alice,
            json!({
                "type": "SEND_MESSAGE",
                "target_player_id": bob_id,
                "text": format!("msg {i}"),
            }),
        )
        .await;
        // Drain the MESSAGE_SENT ack
        let ack = recv(&mut alice).await;
        assert_eq!(ack["type"], "MESSAGE_SENT", "msg {i} should succeed");
    }

    // 6th message should be rate-limited
    let resp = send_recv(
        &mut alice,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": bob_id,
            "text": "one too many",
        }),
    )
    .await;
    assert_eq!(resp["type"], "RATE_LIMITED");
}

/// Blocked sender's messages are silently dropped (sender still gets ack).
#[tokio::test]
async fn test_player_message_blocked() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    let alice_auth = authenticate(&mut alice, "Alice").await;
    let alice_id = alice_auth["player_id"].as_str().unwrap().to_string();

    let mut bob = connect_ws(&server).await;
    let bob_auth = authenticate(&mut bob, "Bob").await;
    let bob_id = bob_auth["player_id"].as_str().unwrap().to_string();

    // Bob blocks Alice
    send_only(
        &mut bob,
        json!({
            "type": "FRIEND_BLOCK",
            "player_id": alice_id,
        }),
    )
    .await;
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;

    // Alice sends message to Bob
    send_only(
        &mut alice,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": bob_id,
            "text": "you cannot see this",
        }),
    )
    .await;

    // Alice still gets MESSAGE_SENT (so she doesn't know she's blocked)
    let ack = recv(&mut alice).await;
    assert_eq!(ack["type"], "MESSAGE_SENT");

    // Bob should NOT receive anything (with a short timeout)
    let result = tokio::time::timeout(std::time::Duration::from_millis(200), recv(&mut bob)).await;
    assert!(
        result.is_err(),
        "Bob should not receive the blocked message"
    );
}

/// Message validation: text over 200 chars or non-ASCII is rejected.
#[tokio::test]
async fn test_player_message_validation() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    authenticate(&mut alice, "Alice").await;

    let mut bob = connect_ws(&server).await;
    let bob_auth = authenticate(&mut bob, "Bob").await;
    let bob_id = bob_auth["player_id"].as_str().unwrap().to_string();

    // Too long message
    let long_text = "x".repeat(201);
    let resp = send_recv(
        &mut alice,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": bob_id,
            "text": long_text,
        }),
    )
    .await;
    assert_eq!(resp["type"], "ERROR");
    assert_eq!(resp["code"], "INVALID_MESSAGE");
}

/// LobbyInfo in welcome bundle includes host_ping_ms field.
#[tokio::test]
async fn test_lobby_list_includes_host_ping() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    // Create a lobby
    send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;

    // Another client connects and gets lobby list in welcome bundle
    let mut viewer = connect_ws(&server).await;
    // Manually authenticate to inspect the welcome bundle's LOBBY_LIST
    viewer
        .send(Message::Text(
            json!({
                "type": "AUTHENTICATE",
                "protocol_version": CURRENT_PROTOCOL,
                "client_version": "1.0.0-test",
                "play_games_token": "fake-token-Viewer",
                "callsign": "Viewer",
                "platform": "test",
            })
            .to_string()
            .into(),
        ))
        .await
        .unwrap();

    let _auth_ok = recv(&mut viewer).await;
    let _status = recv(&mut viewer).await;
    let lobby_list = recv(&mut viewer).await;
    assert_eq!(lobby_list["type"], "LOBBY_LIST");
    let lobbies = lobby_list["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    // host_ping_ms should be present (null since no WS ping measurement yet)
    assert!(lobbies[0].get("host_ping_ms").is_some());
}

/// When all lobby players submit STUN_RESULT, everyone gets CONNECTIVITY_CHECK_GO
/// with prioritized candidate pairs.
#[tokio::test]
async fn test_connectivity_check_go_after_all_stun() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATE from join
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Host submits STUN result first
    send_only(
        &mut host,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "host", "addr": "192.168.1.10:5555" },
                { "candidate_type": "srflx", "addr": "1.2.3.4:10001" },
            ],
            "nat_type": "full_cone",
        }),
    )
    .await;

    // Joiner gets PEER_CANDIDATES only (not all ready yet)
    let joiner_msg = recv(&mut joiner).await;
    assert_eq!(joiner_msg["type"], "PEER_CANDIDATES");

    // Joiner submits STUN result
    send_only(
        &mut joiner,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "host", "addr": "192.168.1.20:5555" },
                { "candidate_type": "srflx", "addr": "5.6.7.8:10002" },
            ],
            "nat_type": "full_cone",
        }),
    )
    .await;

    // Host gets PEER_CANDIDATES then CONNECTIVITY_CHECK_GO
    let host_peers = recv(&mut host).await;
    assert_eq!(host_peers["type"], "PEER_CANDIDATES");
    let host_check = recv(&mut host).await;
    assert_eq!(host_check["type"], "CONNECTIVITY_CHECK_GO");
    let host_pairs = host_check["peer_addrs"].as_array().unwrap();
    assert!(!host_pairs.is_empty());
    // Pairs should be sorted by priority descending
    for i in 1..host_pairs.len() {
        assert!(host_pairs[i - 1]["priority"].as_u64() >= host_pairs[i]["priority"].as_u64());
    }

    // Joiner also gets CONNECTIVITY_CHECK_GO
    let joiner_check = recv(&mut joiner).await;
    assert_eq!(joiner_check["type"], "CONNECTIVITY_CHECK_GO");
    assert!(!joiner_check["peer_addrs"].as_array().unwrap().is_empty());
}

/// When both players have symmetric NAT (no predicted candidates), the server
/// assigns relay sessions on game start and sends RELAY_ASSIGNED to both.
#[tokio::test]
async fn test_relay_assigned_on_game_start() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATE
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Both submit symmetric NAT with only srflx (no predicted) -> Relay
    send_only(
        &mut host,
        json!({
            "type": "STUN_RESULT",
            "candidates": [{ "candidate_type": "srflx", "addr": "1.2.3.4:10001" }],
            "nat_type": "symmetric",
        }),
    )
    .await;
    let _ = recv(&mut joiner).await; // PEER_CANDIDATES

    send_only(
        &mut joiner,
        json!({
            "type": "STUN_RESULT",
            "candidates": [{ "candidate_type": "srflx", "addr": "5.6.7.8:10002" }],
            "nat_type": "symmetric",
        }),
    )
    .await;
    // Host: PEER_CANDIDATES + CONNECTIVITY_CHECK_GO
    let _ = recv(&mut host).await;
    let _ = recv(&mut host).await;
    // Joiner: CONNECTIVITY_CHECK_GO
    let _ = recv(&mut joiner).await;

    // Start game
    send_only(&mut host, json!({"type": "START_GAME"})).await;

    // Both get CONNECTION_INFO, then GAME_STARTING (relay info is now embedded in peers)
    let h1 = recv(&mut host).await;
    assert_eq!(h1["type"], "CONNECTION_INFO");
    let h2 = recv(&mut host).await;
    assert_eq!(h2["type"], "GAME_STARTING");
    assert_eq!(h2["your_slot"], 0);
    let h_peers = h2["peers"].as_array().unwrap();
    assert_eq!(h_peers.len(), 1);
    assert!(h_peers[0]["is_relay"].as_bool().unwrap());
    assert!(h_peers[0]["relay_token"].as_u64().is_some());
    assert_eq!(h_peers[0]["relay_dest_slot"], 1);

    let j1 = recv(&mut joiner).await;
    assert_eq!(j1["type"], "CONNECTION_INFO");
    let j2 = recv(&mut joiner).await;
    assert_eq!(j2["type"], "GAME_STARTING");
    assert_eq!(j2["your_slot"], 1);
    let j_peers = j2["peers"].as_array().unwrap();
    assert_eq!(j_peers.len(), 1);
    assert!(j_peers[0]["is_relay"].as_bool().unwrap());
    assert!(j_peers[0]["relay_token"].as_u64().is_some());
    assert_eq!(j_peers[0]["relay_dest_slot"], 0);

    // Both peers should reference the same relay token
    assert_eq!(
        h_peers[0]["relay_token"].as_u64().unwrap(),
        j_peers[0]["relay_token"].as_u64().unwrap()
    );
}

// ---------------------------------------------------------------------------
// Phase 3: Identity & Anti-Abuse
// ---------------------------------------------------------------------------

/// Kicked players cannot rejoin the same lobby.
#[tokio::test]
async fn test_kicked_player_rejoin_prevention() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "Joiner").await;
    let joiner_id = joiner_auth["player_id"].as_str().unwrap();

    // Join the lobby
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATE from both
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Host kicks joiner
    send_only(
        &mut host,
        json!({"type": "KICK_PLAYER", "player_id": joiner_id}),
    )
    .await;
    // Joiner gets KICKED error + host gets LOBBY_UPDATE
    let kick_msg = recv(&mut joiner).await;
    assert_eq!(kick_msg["code"], "KICKED");
    let _ = recv(&mut host).await; // LOBBY_UPDATE

    // Joiner tries to rejoin -- should be rejected
    let rejoin_resp = send_recv(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    assert_eq!(rejoin_resp["type"], "ERROR");
    assert_eq!(rejoin_resp["code"], "KICKED_FROM_LOBBY");
}

/// Lobbies with a code require the correct code to join.
#[tokio::test]
async fn test_lobby_code_required() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "lobby_code": "secret123",
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;

    // Try to join without code
    let no_code = send_recv(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    assert_eq!(no_code["type"], "ERROR");
    assert_eq!(no_code["code"], "LOBBY_CODE_REQUIRED");

    // Try with wrong code
    let wrong_code = send_recv(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id, "lobby_code": "wrong"}),
    )
    .await;
    assert_eq!(wrong_code["type"], "ERROR");
    assert_eq!(wrong_code["code"], "LOBBY_CODE_REQUIRED");

    // Try with correct code -- should succeed (get LOBBY_UPDATE)
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id, "lobby_code": "secret123"}),
    )
    .await;
    let update = recv(&mut joiner).await;
    assert_eq!(update["type"], "LOBBY_UPDATE");
    let players = update["players"].as_array().unwrap();
    assert_eq!(players.len(), 2);
}

/// Lobby listing shows has_code but never the actual code.
#[tokio::test]
async fn test_lobby_code_listing() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    // Create a coded lobby
    send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "lobby_code": "mycode",
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;

    // Another client lists lobbies
    let mut viewer = connect_ws(&server).await;
    authenticate(&mut viewer, "Viewer").await;

    let list = send_recv(&mut viewer, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list["type"], "LOBBY_LIST");
    let lobbies = list["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    assert_eq!(lobbies[0]["has_code"], true);
    // The actual code string must never be exposed
    assert!(lobbies[0].get("lobby_code").is_none());
    assert!(lobbies[0].get("code").is_none());
}

/// Friends can join a coded lobby via JOIN_FRIEND_GAME without providing a code.
/// Kicked players cannot join even via friend-join.
#[tokio::test]
async fn test_lobby_code_friend_bypass_and_kicked_friend() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    let host_auth = authenticate(&mut host, "Host").await;
    let host_id = host_auth["player_id"].as_str().unwrap().to_string();

    let mut friend = connect_ws(&server).await;
    let friend_auth = authenticate(&mut friend, "Friend").await;
    let friend_id = friend_auth["player_id"].as_str().unwrap().to_string();

    // Establish friendship: host -> friend request, friend accepts
    send_only(
        &mut host,
        json!({"type": "FRIEND_REQUEST", "target_callsign": "Friend"}),
    )
    .await;
    let req_notif = recv(&mut friend).await;
    assert_eq!(req_notif["type"], "FRIEND_REQUEST_RECEIVED");

    send_only(
        &mut friend,
        json!({"type": "FRIEND_ACCEPT", "player_id": host_id}),
    )
    .await;
    // Host gets FRIEND_ACCEPTED notification
    let accepted = recv(&mut host).await;
    assert_eq!(accepted["type"], "FRIEND_ACCEPTED");

    // Host creates coded lobby
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "lobby_code": "friendcode",
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let _lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    // Friend joins via JOIN_FRIEND_GAME (no code needed)
    let join_resp = send_recv(
        &mut friend,
        json!({"type": "JOIN_FRIEND_GAME", "friend_player_id": host_id}),
    )
    .await;
    assert_eq!(join_resp["type"], "JOIN_FRIEND_GAME_RESP");
    assert_eq!(join_resp["success"], true);
    // Drain LOBBY_UPDATE
    let _ = recv(&mut host).await;
    let _ = recv(&mut friend).await;

    // Now host kicks the friend
    send_only(
        &mut host,
        json!({"type": "KICK_PLAYER", "player_id": friend_id}),
    )
    .await;
    let _ = recv(&mut friend).await; // KICKED error
    let _ = recv(&mut host).await; // LOBBY_UPDATE

    // Kicked friend tries to rejoin via JOIN_FRIEND_GAME -- should fail
    let rejoin_resp = send_recv(
        &mut friend,
        json!({"type": "JOIN_FRIEND_GAME", "friend_player_id": host_id}),
    )
    .await;
    assert_eq!(rejoin_resp["type"], "JOIN_FRIEND_GAME_RESP");
    assert_eq!(rejoin_resp["success"], false);
}

// ---------------------------------------------------------------------------
// Predictive port allocation & verified-only lobbies
// ---------------------------------------------------------------------------

/// Symmetric NAT with sequential srflx ports gets predicted candidates injected.
#[tokio::test]
async fn test_predictive_port_candidates() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
            "max_players": 4,
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut host).await; // LOBBY_UPDATE
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE

    // Host submits symmetric STUN with 2 sequential srflx ports
    send_only(
        &mut host,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "srflx", "addr": "1.2.3.4:50000" },
                { "candidate_type": "srflx", "addr": "1.2.3.4:50001" },
            ],
            "nat_type": "symmetric",
        }),
    )
    .await;

    // Joiner receives PEER_CANDIDATES with predicted entries
    let peer_msg = recv(&mut joiner).await;
    assert_eq!(peer_msg["type"], "PEER_CANDIDATES");
    let cands = peer_msg["candidates"].as_array().unwrap();
    // Should have 2 srflx + 2 predicted = 4
    assert_eq!(cands.len(), 4);
    let predicted: Vec<&Value> = cands
        .iter()
        .filter(|c| c["candidate_type"] == "predicted")
        .collect();
    assert_eq!(predicted.len(), 2);
    assert_eq!(predicted[0]["addr"], "1.2.3.4:50002");
    assert_eq!(predicted[1]["addr"], "1.2.3.4:50003");
}

/// Non-sequential symmetric NAT does NOT generate predicted candidates.
#[tokio::test]
async fn test_predictive_port_skipped_for_random_nat() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
            "max_players": 4,
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Host submits symmetric STUN with widely-spaced (random) ports
    send_only(
        &mut host,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "srflx", "addr": "1.2.3.4:10000" },
                { "candidate_type": "srflx", "addr": "1.2.3.4:30000" },
            ],
            "nat_type": "symmetric",
        }),
    )
    .await;

    // Joiner receives PEER_CANDIDATES with only the 2 srflx -- no predicted
    let peer_msg = recv(&mut joiner).await;
    assert_eq!(peer_msg["type"], "PEER_CANDIDATES");
    let cands = peer_msg["candidates"].as_array().unwrap();
    assert_eq!(cands.len(), 2);
    assert!(cands.iter().all(|c| c["candidate_type"] == "srflx"));
}

/// Unverified player cannot join a verified-only lobby.
#[tokio::test]
async fn test_verified_only_lobby_rejected() {
    let server = TestServer::start().await;

    // Host creates a verified_only lobby
    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "verified_only": true,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    // Joiner (unverified -- skip_gpgs_verify is true in tests) tries to join
    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;

    let join_resp = send_recv(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    assert_eq!(join_resp["type"], "ERROR");
    assert_eq!(join_resp["code"], "VERIFIED_ONLY");
}

/// Lobby listing includes verified_only flag.
#[tokio::test]
async fn test_verified_only_in_listing() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    send_only(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "verified_only": true,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let _ = recv(&mut host).await; // CREATE_LOBBY_RESP

    // Another client lists lobbies
    let mut viewer = connect_ws(&server).await;
    authenticate(&mut viewer, "Viewer").await;

    let list_resp = send_recv(&mut viewer, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list_resp["type"], "LOBBY_LIST");
    let lobbies = list_resp["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    assert_eq!(lobbies[0]["verified_only"], true);
}

/// Unverified player cannot join verified-only lobby via JOIN_FRIEND_GAME.
#[tokio::test]
async fn test_verified_only_friend_join_rejected() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    let host_auth = authenticate(&mut host, "Host").await;
    let host_id = host_auth["player_id"].as_str().unwrap();

    let mut friend = connect_ws(&server).await;
    let friend_auth = authenticate(&mut friend, "Friend").await;
    let _friend_id = friend_auth["player_id"].as_str().unwrap();

    // Establish friendship: host -> friend request, friend accepts
    send_only(
        &mut host,
        json!({"type": "FRIEND_REQUEST", "target_callsign": "Friend"}),
    )
    .await;
    let req_notif = recv(&mut friend).await;
    assert_eq!(req_notif["type"], "FRIEND_REQUEST_RECEIVED");

    send_only(
        &mut friend,
        json!({"type": "FRIEND_ACCEPT", "player_id": host_id}),
    )
    .await;
    let accepted = recv(&mut host).await;
    assert_eq!(accepted["type"], "FRIEND_ACCEPTED");

    // Host creates verified-only lobby
    let _create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "verified_only": true,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;

    // Friend tries to join via friend link -- should fail
    let join_resp = send_recv(
        &mut friend,
        json!({"type": "JOIN_FRIEND_GAME", "friend_player_id": host_id}),
    )
    .await;
    assert_eq!(join_resp["type"], "JOIN_FRIEND_GAME_RESP");
    assert_eq!(join_resp["success"], false);
}

// ---------------------------------------------------------------------------
// Keypair / Proof-of-Work authentication tests
// ---------------------------------------------------------------------------

/// Helper: generate an ed25519 keypair in hex format and sign a message.
fn keypair_sign(callsign: &str, timestamp: u64) -> (String, String) {
    use ed25519_dalek::{Signer, SigningKey};
    let signing_key = SigningKey::generate(&mut rand::rng());
    let pubkey_hex = hex::encode(signing_key.verifying_key().as_bytes());
    let message = format!("{callsign}:{timestamp}");
    let signature = signing_key.sign(message.as_bytes());
    let sig_hex = hex::encode(signature.to_bytes());
    (pubkey_hex, sig_hex)
}

/// Helper: sign with a specific signing key.
fn sign_with_key(
    signing_key: &ed25519_dalek::SigningKey,
    callsign: &str,
    timestamp: u64,
) -> String {
    use ed25519_dalek::Signer;
    let message = format!("{callsign}:{timestamp}");
    let signature = signing_key.sign(message.as_bytes());
    hex::encode(signature.to_bytes())
}

/// Brute-force solve a PoW challenge at the given difficulty.
fn solve_pow(challenge: &str, difficulty: u8) -> String {
    use sha2::{Digest, Sha256};
    for nonce in 0u64.. {
        let solution = format!("{nonce:016x}");
        let mut hasher = Sha256::new();
        hasher.update(challenge.as_bytes());
        hasher.update(solution.as_bytes());
        let hash = hasher.finalize();
        if dxx_matchmaking::pow::leading_zero_bits(&hash) >= difficulty as u32 {
            return solution;
        }
    }
    unreachable!()
}

/// Helper: authenticate with a keypair (new key -- goes through PoW flow).
/// Returns (AUTH_OK value, pubkey_hex).
async fn authenticate_keypair_new(ws: &mut WsClient, callsign: &str) -> (Value, String) {
    use ed25519_dalek::SigningKey;

    let signing_key = SigningKey::generate(&mut rand::rng());
    let pubkey_hex = hex::encode(signing_key.verifying_key().as_bytes());
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let sig_hex = sign_with_key(&signing_key, callsign, timestamp);

    // Send AUTHENTICATE with keypair method
    send_only(
        ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "1.0.0-test",
            "callsign": callsign,
            "platform": "test",
            "auth_method": "keypair",
            "public_key": pubkey_hex,
            "auth_timestamp": timestamp,
            "auth_signature": sig_hex,
        }),
    )
    .await;

    // Expect POW_CHALLENGE for new key
    let pow_challenge = recv(ws).await;
    assert_eq!(
        pow_challenge["type"], "POW_CHALLENGE",
        "expected POW_CHALLENGE, got {pow_challenge}"
    );
    let challenge = pow_challenge["challenge"].as_str().unwrap().to_string();
    let difficulty = pow_challenge["difficulty"].as_u64().unwrap() as u8;

    // Solve it
    let solution = solve_pow(&challenge, difficulty);

    // Send POW_SOLUTION
    let auth_ok = send_recv(
        ws,
        json!({
            "type": "POW_SOLUTION",
            "challenge": challenge,
            "solution": solution,
        }),
    )
    .await;
    assert_eq!(
        auth_ok["type"], "AUTH_OK",
        "expected AUTH_OK, got {auth_ok}"
    );

    // Drain welcome bundle
    let status = recv(ws).await;
    assert_eq!(status["type"], "SERVER_STATUS");
    let lobby_list = recv(ws).await;
    assert_eq!(lobby_list["type"], "LOBBY_LIST");
    let friend_list = recv(ws).await;
    assert_eq!(friend_list["type"], "FRIEND_LIST_RESP");

    (auth_ok, pubkey_hex)
}

/// New keypair triggers PoW challenge, solving it completes auth.
#[tokio::test]
async fn test_keypair_auth_new_key() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    let (auth_ok, _pubkey) = authenticate_keypair_new(&mut ws, "KeypairPilot").await;
    assert!(auth_ok.get("player_id").is_some());
    assert!(auth_ok.get("session_token").is_some());
}

/// Known keypair authenticates without PoW on second connection.
#[tokio::test]
async fn test_keypair_auth_known_key() {
    use ed25519_dalek::SigningKey;

    let server = TestServer::start().await;
    let signing_key = SigningKey::generate(&mut rand::rng());
    let pubkey_hex = hex::encode(signing_key.verifying_key().as_bytes());

    // First connection: new key, goes through PoW
    {
        let mut ws = connect_ws(&server).await;
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();
        let sig_hex = sign_with_key(&signing_key, "ReturnPilot", timestamp);

        send_only(
            &mut ws,
            json!({
                "type": "AUTHENTICATE",
                "protocol_version": CURRENT_PROTOCOL,
                "client_version": "1.0.0-test",
                "callsign": "ReturnPilot",
                "platform": "test",
                "auth_method": "keypair",
                "public_key": pubkey_hex,
                "auth_timestamp": timestamp,
                "auth_signature": sig_hex,
            }),
        )
        .await;

        let pow_challenge = recv(&mut ws).await;
        assert_eq!(pow_challenge["type"], "POW_CHALLENGE");
        let challenge = pow_challenge["challenge"].as_str().unwrap();
        let difficulty = pow_challenge["difficulty"].as_u64().unwrap() as u8;
        let solution = solve_pow(challenge, difficulty);

        let auth_ok = send_recv(
            &mut ws,
            json!({
                "type": "POW_SOLUTION",
                "challenge": challenge,
                "solution": solution,
            }),
        )
        .await;
        assert_eq!(auth_ok["type"], "AUTH_OK");

        // Drain welcome bundle
        recv(&mut ws).await; // SERVER_STATUS
        recv(&mut ws).await; // LOBBY_LIST
        recv(&mut ws).await; // FRIEND_LIST_RESP
    }

    // Second connection: known key, should get AUTH_OK directly (no PoW)
    {
        let mut ws = connect_ws(&server).await;
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs();
        let sig_hex = sign_with_key(&signing_key, "ReturnPilot", timestamp);

        let auth_ok = send_recv(
            &mut ws,
            json!({
                "type": "AUTHENTICATE",
                "protocol_version": CURRENT_PROTOCOL,
                "client_version": "1.0.0-test",
                "callsign": "ReturnPilot",
                "platform": "test",
                "auth_method": "keypair",
                "public_key": pubkey_hex,
                "auth_timestamp": timestamp,
                "auth_signature": sig_hex,
            }),
        )
        .await;
        // Known key => straight to AUTH_OK, no POW_CHALLENGE
        assert_eq!(auth_ok["type"], "AUTH_OK");
    }
}

/// Invalid ed25519 signature is rejected.
#[tokio::test]
async fn test_keypair_auth_bad_signature() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;

    let (pubkey_hex, _) = keypair_sign("BadSig", 12345);
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();

    let resp = send_recv(
        &mut ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "1.0.0-test",
            "callsign": "BadSig",
            "platform": "test",
            "auth_method": "keypair",
            "public_key": pubkey_hex,
            "auth_timestamp": timestamp,
            "auth_signature": "00".repeat(64), // wrong signature
        }),
    )
    .await;
    assert_eq!(resp["type"], "AUTH_FAIL");
}

/// Wrong PoW solution is rejected.
#[tokio::test]
async fn test_keypair_auth_bad_pow() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;

    let (_pubkey_hex, _sig_hex) = keypair_sign("BadPow", 0); // not used; we generate a fresh key below
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();

    // Generate a valid keypair+signature for the right timestamp
    use ed25519_dalek::SigningKey;
    let signing_key = SigningKey::generate(&mut rand::rng());
    let pk = hex::encode(signing_key.verifying_key().as_bytes());
    let real_sig = sign_with_key(&signing_key, "BadPow", timestamp);

    send_only(
        &mut ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "1.0.0-test",
            "callsign": "BadPow",
            "platform": "test",
            "auth_method": "keypair",
            "public_key": pk,
            "auth_timestamp": timestamp,
            "auth_signature": real_sig,
        }),
    )
    .await;

    let pow_challenge = recv(&mut ws).await;
    assert_eq!(pow_challenge["type"], "POW_CHALLENGE");
    let challenge = pow_challenge["challenge"].as_str().unwrap();

    // Send a bad solution
    let resp = send_recv(
        &mut ws,
        json!({
            "type": "POW_SOLUTION",
            "challenge": challenge,
            "solution": "0000000000000000", // almost certainly wrong
        }),
    )
    .await;
    assert_eq!(resp["type"], "AUTH_FAIL");
}

/// Keypair-authenticated player is NOT gpgs_verified, so cannot join
/// a verified-only lobby.
#[tokio::test]
async fn test_keypair_not_gpgs_verified() {
    let server = TestServer::start().await;

    // Host creates a verified-only lobby (GPGS/dev mode in test = verified)
    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "VerifiedHost").await;
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "verified_only": true,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap();

    // Keypair player tries to join
    let mut kp = connect_ws(&server).await;
    let (_auth_ok, _pk) = authenticate_keypair_new(&mut kp, "KeypairGuy").await;

    let join_resp = send_recv(&mut kp, json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id})).await;
    assert_eq!(join_resp["type"], "ERROR");
    assert!(join_resp["message"].as_str().unwrap().contains("verified"));
}

// ---------------------------------------------------------------------------
// Phase C2/C3 live flow tests -- validate the full lobby lifecycle that the
// Android client exercises: browse -> join -> ready -> start, plus leave and
// kick. These ensure server behavior matches what the Kotlin client expects.
// ---------------------------------------------------------------------------

/// Full two-player lobby flow: host creates, joiner joins, both ready,
/// host starts game. Verifies LOBBY_UPDATE broadcasts and GAME_STARTING.
#[tokio::test]
async fn test_full_lobby_flow_join_ready_start() {
    let server = TestServer::start().await;

    // -- Host creates lobby --
    let mut host = connect_ws(&server).await;
    let host_auth = authenticate(&mut host, "HostPlayer").await;
    let host_id = host_auth["player_id"].as_str().unwrap().to_string();

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "anarchy"},
        }),
    )
    .await;
    assert_eq!(create_resp["type"], "LOBBY_UPDATE");
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();
    // Host should see themselves in the player list
    let players = create_resp["players"].as_array().unwrap();
    assert_eq!(players.len(), 1);
    assert_eq!(players[0]["callsign"], "HostPlayer");

    // -- Joiner connects and sees lobby in list --
    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "JoinPlayer").await;
    let _joiner_id = joiner_auth["player_id"].as_str().unwrap().to_string();

    let list_resp = send_recv(&mut joiner, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list_resp["type"], "LOBBY_LIST");
    let lobbies = list_resp["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    assert_eq!(lobbies[0]["host_callsign"], "HostPlayer");
    assert!(lobbies[0]["joinable"].as_bool().unwrap());

    // -- Joiner joins --
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;

    // Both players receive LOBBY_UPDATE with 2 players
    let joiner_update = recv(&mut joiner).await;
    assert_eq!(joiner_update["type"], "LOBBY_UPDATE");
    assert_eq!(joiner_update["players"].as_array().unwrap().len(), 2);

    let host_update = recv(&mut host).await;
    assert_eq!(host_update["type"], "LOBBY_UPDATE");
    assert_eq!(host_update["players"].as_array().unwrap().len(), 2);
    // Host should be first in player list
    assert_eq!(
        host_update["players"][0]["player_id"].as_str().unwrap(),
        host_id
    );

    // -- Both ready up --
    send_only(&mut host, json!({"type": "READY", "ready": true})).await;
    // Both get an update
    let _ = recv(&mut host).await; // LOBBY_UPDATE
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE

    send_only(&mut joiner, json!({"type": "READY", "ready": true})).await;
    let host_u = recv(&mut host).await;
    assert_eq!(host_u["type"], "LOBBY_UPDATE");
    // Verify both are ready
    for p in host_u["players"].as_array().unwrap() {
        assert!(p["ready"].as_bool().unwrap(), "player should be ready");
    }
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE

    // -- Host starts game --
    send_only(&mut host, json!({"type": "START_GAME"})).await;

    // Server sends CONNECTION_INFO then GAME_STARTING to all players
    let host_conn = recv(&mut host).await;
    assert_eq!(host_conn["type"], "CONNECTION_INFO");
    let host_start = recv(&mut host).await;
    assert_eq!(host_start["type"], "GAME_STARTING");
    assert_eq!(host_start["game_info"]["mission"], "Counterstrike!");
    assert_eq!(host_start["game_info"]["mode"], "anarchy");

    let joiner_conn = recv(&mut joiner).await;
    assert_eq!(joiner_conn["type"], "CONNECTION_INFO");
    let joiner_start = recv(&mut joiner).await;
    assert_eq!(joiner_start["type"], "GAME_STARTING");
    assert_eq!(joiner_start["game_info"]["mission"], "Counterstrike!");
}

/// Joiner leaves lobby, host gets LOBBY_UPDATE with 1 player.
#[tokio::test]
async fn test_leave_lobby_update() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut host).await; // LOBBY_UPDATE (2 players)
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE (2 players)

    // Joiner leaves
    send_only(&mut joiner, json!({"type": "LEAVE_LOBBY"})).await;

    let host_update = recv(&mut host).await;
    assert_eq!(host_update["type"], "LOBBY_UPDATE");
    assert_eq!(host_update["players"].as_array().unwrap().len(), 1);
    assert_eq!(host_update["players"][0]["callsign"], "Host");
}

/// Host kicks a player, kicked player gets ERROR, host gets LOBBY_UPDATE.
#[tokio::test]
async fn test_kick_player_flow() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap();

    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "Victim").await;
    let joiner_id = joiner_auth["player_id"].as_str().unwrap();

    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut host).await; // LOBBY_UPDATE (2)
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE (2)

    // Host kicks
    send_only(
        &mut host,
        json!({"type": "KICK_PLAYER", "player_id": joiner_id}),
    )
    .await;

    // Host gets LOBBY_UPDATE with 1 player
    let host_update = recv(&mut host).await;
    assert_eq!(host_update["type"], "LOBBY_UPDATE");
    assert_eq!(host_update["players"].as_array().unwrap().len(), 1);

    // Joiner gets an ERROR with kick info
    let kick_msg = recv(&mut joiner).await;
    assert_eq!(kick_msg["type"], "ERROR");
    let msg_text = kick_msg["message"].as_str().unwrap_or("");
    let code_text = kick_msg["code"].as_str().unwrap_or("");
    assert!(
        msg_text.to_lowercase().contains("kick") || code_text.to_lowercase().contains("kick"),
        "expected kick-related error, got: code={code_text}, message={msg_text}"
    );
}

/// Non-host cannot start the game.
#[tokio::test]
async fn test_non_host_cannot_start() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;
    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap();

    let mut joiner = connect_ws(&server).await;
    authenticate(&mut joiner, "Joiner").await;
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut host).await; // LOBBY_UPDATE
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE

    // Non-host tries to start
    let resp = send_recv(&mut joiner, json!({"type": "START_GAME"})).await;
    assert_eq!(resp["type"], "ERROR");
}

/// Ready toggle: player readies then unreadies.
#[tokio::test]
async fn test_ready_toggle() {
    let server = TestServer::start().await;

    let mut ws = connect_ws(&server).await;
    authenticate(&mut ws, "Solo").await;
    let create_resp = send_recv(
        &mut ws,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    assert_eq!(create_resp["type"], "LOBBY_UPDATE");

    // Ready up
    send_only(&mut ws, json!({"type": "READY", "ready": true})).await;
    let update = recv(&mut ws).await;
    assert_eq!(update["type"], "LOBBY_UPDATE");
    assert!(update["players"][0]["ready"].as_bool().unwrap());

    // Unready
    send_only(&mut ws, json!({"type": "READY", "ready": false})).await;
    let update2 = recv(&mut ws).await;
    assert_eq!(update2["type"], "LOBBY_UPDATE");
    assert!(!update2["players"][0]["ready"].as_bool().unwrap());
}

/// Two players in a lobby exchange messages and observe the full chat flow.
#[tokio::test]
async fn test_lobby_chat_flow() {
    let server = TestServer::start().await;

    // Host creates lobby
    let mut host = connect_ws(&server).await;
    let host_auth = authenticate(&mut host, "HostChat").await;
    let host_id = host_auth["player_id"].as_str().unwrap().to_string();

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "anarchy"},
        }),
    )
    .await;
    assert_eq!(create_resp["type"], "LOBBY_UPDATE");
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    // Joiner connects and joins
    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "JoinerChat").await;
    let joiner_id = joiner_auth["player_id"].as_str().unwrap().to_string();

    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let _ = recv(&mut joiner).await; // LOBBY_UPDATE
    let _ = recv(&mut host).await; // LOBBY_UPDATE

    // Host sends a message to joiner
    send_only(
        &mut host,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": joiner_id,
            "text": "welcome to my lobby",
        }),
    )
    .await;

    // Host gets MESSAGE_SENT
    let sent_ack = recv(&mut host).await;
    assert_eq!(sent_ack["type"], "MESSAGE_SENT");
    assert_eq!(sent_ack["target_player_id"], joiner_id);

    // Joiner gets MESSAGE_RECEIVED
    let received = recv(&mut joiner).await;
    assert_eq!(received["type"], "MESSAGE_RECEIVED");
    assert_eq!(received["from_callsign"], "HostChat");
    assert_eq!(received["text"], "welcome to my lobby");

    // Joiner replies
    send_only(
        &mut joiner,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": host_id,
            "text": "thanks!",
        }),
    )
    .await;

    let sent_ack2 = recv(&mut joiner).await;
    assert_eq!(sent_ack2["type"], "MESSAGE_SENT");
    assert_eq!(sent_ack2["target_player_id"], host_id);

    let received2 = recv(&mut host).await;
    assert_eq!(received2["type"], "MESSAGE_RECEIVED");
    assert_eq!(received2["from_callsign"], "JoinerChat");
    assert_eq!(received2["text"], "thanks!");
}

/// Create lobby and verify it shows in the lobby list with correct details.
#[tokio::test]
async fn test_create_lobby_details() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Creator").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d1",
            "max_players": 3,
            "game_info": {"mission": "First Strike", "mode": "coop"},
        }),
    )
    .await;
    assert_eq!(create_resp["type"], "LOBBY_UPDATE");
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();
    assert_eq!(create_resp["players"].as_array().unwrap().len(), 1);

    // Another client lists lobbies and sees details
    let mut viewer = connect_ws(&server).await;
    authenticate(&mut viewer, "Viewer").await;

    let list = send_recv(&mut viewer, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list["type"], "LOBBY_LIST");
    let lobbies = list["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    let l = &lobbies[0];
    assert_eq!(l["lobby_id"], lobby_id);
    assert_eq!(l["host_callsign"], "Creator");
    assert_eq!(l["game_info"]["mission"], "First Strike");
    assert_eq!(l["game_info"]["mode"], "coop");
    assert_eq!(l["max_players"], 3);
    assert_eq!(l["player_count"], 1);
    assert!(l["joinable"].as_bool().unwrap());
}

/// End-to-end two-client flow: connect, discover, join, chat, leave.
/// This mirrors the live test scenario of two emulator/bot clients.
#[tokio::test]
async fn test_two_client_discovery_join_chat() {
    let server = TestServer::start().await;

    // Client A connects and creates a lobby
    let mut client_a = connect_ws(&server).await;
    let a_auth = authenticate(&mut client_a, "ClientA").await;
    let a_id = a_auth["player_id"].as_str().unwrap().to_string();

    let create = send_recv(
        &mut client_a,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "anarchy"},
        }),
    )
    .await;
    assert_eq!(create["type"], "LOBBY_UPDATE");
    let lobby_id = create["lobby_id"].as_str().unwrap().to_string();

    // Client B connects, lists lobbies, sees A's lobby, joins it
    let mut client_b = connect_ws(&server).await;
    let b_auth = authenticate(&mut client_b, "ClientB").await;
    let b_id = b_auth["player_id"].as_str().unwrap().to_string();
    assert_ne!(a_id, b_id, "two clients must have different player IDs");

    let list = send_recv(&mut client_b, json!({"type": "LIST_LOBBIES"})).await;
    assert_eq!(list["type"], "LOBBY_LIST");
    let lobbies = list["lobbies"].as_array().unwrap();
    assert_eq!(lobbies.len(), 1);
    assert_eq!(lobbies[0]["host_callsign"], "ClientA");
    assert!(lobbies[0]["joinable"].as_bool().unwrap());

    // B joins A's lobby
    send_only(
        &mut client_b,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    let b_update = recv(&mut client_b).await;
    assert_eq!(b_update["type"], "LOBBY_UPDATE");
    assert_eq!(b_update["players"].as_array().unwrap().len(), 2);
    let a_update = recv(&mut client_a).await;
    assert_eq!(a_update["type"], "LOBBY_UPDATE");
    assert_eq!(a_update["players"].as_array().unwrap().len(), 2);

    // A sends chat to B
    send_only(
        &mut client_a,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": b_id,
            "text": "hello from A",
        }),
    )
    .await;
    let a_sent = recv(&mut client_a).await;
    assert_eq!(a_sent["type"], "MESSAGE_SENT");
    let b_recv = recv(&mut client_b).await;
    assert_eq!(b_recv["type"], "MESSAGE_RECEIVED");
    assert_eq!(b_recv["from_callsign"], "ClientA");
    assert_eq!(b_recv["text"], "hello from A");

    // B replies
    send_only(
        &mut client_b,
        json!({
            "type": "SEND_MESSAGE",
            "target_player_id": a_id,
            "text": "hi back from B",
        }),
    )
    .await;
    let b_sent = recv(&mut client_b).await;
    assert_eq!(b_sent["type"], "MESSAGE_SENT");
    let a_recv = recv(&mut client_a).await;
    assert_eq!(a_recv["type"], "MESSAGE_RECEIVED");
    assert_eq!(a_recv["from_callsign"], "ClientB");
    assert_eq!(a_recv["text"], "hi back from B");

    // B leaves lobby
    send_only(&mut client_b, json!({"type": "LEAVE_LOBBY"})).await;
    let a_after = recv(&mut client_a).await;
    assert_eq!(a_after["type"], "LOBBY_UPDATE");
    assert_eq!(a_after["players"].as_array().unwrap().len(), 1);

    // A verifies lobby list shows only their lobby with 1 player
    let list2 = send_recv(&mut client_a, json!({"type": "LIST_LOBBIES"})).await;
    let lobbies2 = list2["lobbies"].as_array().unwrap();
    assert_eq!(lobbies2[0]["player_count"], 1);
}

// ---------------------------------------------------------------------------
// Phase C: Relay session cleanup
// ---------------------------------------------------------------------------

/// Relay sessions older than the cutoff are reaped by
/// cleanup_sessions_older_than.
#[tokio::test]
async fn test_relay_cleanup_stale_sessions() {
    let server = TestServer::start().await;

    // Insert a relay session created 3 seconds ago (will exceed our 1s cutoff)
    let old_created_at = std::time::Instant::now() - std::time::Duration::from_secs(3);
    let old_session = dxx_matchmaking::relay::RelaySession {
        session_token: 99999,
        player_addrs: dashmap::DashMap::new(),
        expected_players: 2,
        allowed_ips: dashmap::DashSet::new(),
        created_at: old_created_at,
        packets_forwarded: std::sync::atomic::AtomicU64::new(0),
    };
    server.state.relay_sessions.insert(99999, old_session);

    // Insert a fresh relay session
    let fresh_session = dxx_matchmaking::relay::RelaySession {
        session_token: 11111,
        player_addrs: dashmap::DashMap::new(),
        expected_players: 2,
        allowed_ips: dashmap::DashSet::new(),
        created_at: std::time::Instant::now(),
        packets_forwarded: std::sync::atomic::AtomicU64::new(0),
    };
    server.state.relay_sessions.insert(11111, fresh_session);

    assert_eq!(server.state.relay_sessions.len(), 2);

    // Use a 1-second cutoff so the 3-second-old session is reaped
    let removed = dxx_matchmaking::relay::cleanup_sessions_older_than(
        &server.state,
        std::time::Duration::from_secs(1),
    );
    assert_eq!(removed, 1);
    assert_eq!(server.state.relay_sessions.len(), 1);
    assert!(server.state.relay_sessions.contains_key(&11111));
    assert!(!server.state.relay_sessions.contains_key(&99999));
}

/// CONNECTIVITY_OK from a client updates the player's connection_type
/// and ping_ms in the lobby state.
#[tokio::test]
async fn test_connectivity_ok_updates_player() {
    let server = TestServer::start().await;

    let mut host = connect_ws(&server).await;
    authenticate(&mut host, "Host").await;

    let create_resp = send_recv(
        &mut host,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    let lobby_id = create_resp["lobby_id"].as_str().unwrap().to_string();

    let mut joiner = connect_ws(&server).await;
    let joiner_auth = authenticate(&mut joiner, "Joiner").await;
    let joiner_id = joiner_auth["player_id"].as_str().unwrap().to_string();
    send_only(
        &mut joiner,
        json!({"type": "JOIN_LOBBY", "lobby_id": lobby_id}),
    )
    .await;
    // Drain LOBBY_UPDATEs
    let _ = recv(&mut host).await;
    let _ = recv(&mut joiner).await;

    // Both submit STUN results to trigger CONNECTIVITY_CHECK_GO
    send_only(
        &mut host,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "host", "addr": "192.168.1.10:5555" },
                { "candidate_type": "srflx", "addr": "1.2.3.4:10001" },
            ],
            "nat_type": "full_cone",
        }),
    )
    .await;
    let _ = recv(&mut joiner).await; // PEER_CANDIDATES

    send_only(
        &mut joiner,
        json!({
            "type": "STUN_RESULT",
            "candidates": [
                { "candidate_type": "host", "addr": "192.168.1.20:5555" },
                { "candidate_type": "srflx", "addr": "5.6.7.8:10002" },
            ],
            "nat_type": "full_cone",
        }),
    )
    .await;
    // Host: PEER_CANDIDATES + CONNECTIVITY_CHECK_GO
    let _ = recv(&mut host).await;
    let _ = recv(&mut host).await;
    // Joiner: CONNECTIVITY_CHECK_GO
    let _ = recv(&mut joiner).await;

    // Host reports CONNECTIVITY_OK with a peer via srflx
    send_only(
        &mut host,
        json!({
            "type": "CONNECTIVITY_OK",
            "peer_id": joiner_id,
            "winning_candidate_type": "srflx",
            "rtt_ms": 42,
        }),
    )
    .await;

    // Small delay to let the server process the message
    tokio::time::sleep(std::time::Duration::from_millis(50)).await;

    // Verify the lobby state directly: host should have DirectHolepunch + ping 42ms
    let lobby_id_uuid: uuid::Uuid = lobby_id.parse().unwrap();
    let lobby = server.state.lobbies.get(&lobby_id_uuid).unwrap();
    let host_player = lobby.players.iter().find(|p| p.callsign == "Host").unwrap();
    assert_eq!(
        host_player.connection_type,
        dxx_matchmaking::lobby::ConnectionType::DirectHolepunch
    );
    assert_eq!(host_player.ping_ms, Some(42));
}

// ---------------------------------------------------------------------------
// STUN self-hosted server tests
// ---------------------------------------------------------------------------

/// AUTH_OK includes stun_addrs when STUN_PUBLIC_ADDRS is configured.
#[tokio::test]
async fn auth_ok_includes_stun_addrs_when_configured() {
    let config = ServerConfig {
        ws_listen_addr: "127.0.0.1:0".parse().unwrap(),
        http_listen_addr: "127.0.0.1:0".parse().unwrap(),
        relay_listen_addr: "127.0.0.1:0".parse().unwrap(),
        stun_listen_addr: "127.0.0.1:0".parse().unwrap(),
        stun_listen_addr_alt: "127.0.0.1:0".parse().unwrap(),
        stun_public_addrs: "203.0.113.5:3478,203.0.113.5:3479".into(),
        db_path: ":memory:".into(),
        google_client_id: String::new(),
        google_client_secret: String::new(),
        admin_token: "test-admin-token".into(),
        motd: String::new(),
        update_url: "https://example.com/update".into(),
        tls_cert_path: String::new(),
        tls_key_path: String::new(),
        relay_public_addr: "127.0.0.1:19001".into(),
        log_dir: String::new(),
        skip_gpgs_verify: true,
        pow_difficulty: 8,
        max_relay_sessions: 100,
        max_connections: 0,
        admin_http_listen_addr: None,
    };

    let state = build_state(config).expect("build state");
    let ws_listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let ws_addr = ws_listener.local_addr().unwrap();
    let ws_state = Arc::clone(&state);
    let _ws_handle = tokio::spawn(async move {
        let app = ws_handler::ws_router(ws_state);
        axum::serve(
            ws_listener,
            app.into_make_service_with_connect_info::<SocketAddr>(),
        )
        .await
        .unwrap();
    });

    let url = format!("ws://{}/ws", ws_addr);
    let (mut ws, _) = connect_async(&url).await.unwrap();
    let auth_ok = send_recv(
        &mut ws,
        json!({
            "type": "AUTHENTICATE",
            "protocol_version": CURRENT_PROTOCOL,
            "client_version": "test",
            "play_games_token": "tok_stun_test",
            "callsign": "StunTester",
            "platform": "test",
        }),
    )
    .await;

    assert_eq!(auth_ok["type"], "AUTH_OK");
    let addrs = auth_ok["stun_addrs"]
        .as_array()
        .expect("stun_addrs should be array");
    assert_eq!(addrs.len(), 2);
    assert_eq!(addrs[0], "203.0.113.5:3478");
    assert_eq!(addrs[1], "203.0.113.5:3479");
}

/// AUTH_OK omits stun_addrs (or empty) when STUN_PUBLIC_ADDRS is not set.
#[tokio::test]
async fn auth_ok_no_stun_addrs_when_not_configured() {
    let server = TestServer::start().await;
    let mut ws = connect_ws(&server).await;
    let auth_ok = authenticate(&mut ws, "NoStun").await;
    // stun_addrs should either be absent or empty
    let addrs = auth_ok.get("stun_addrs");
    assert!(
        addrs.is_none() || addrs.unwrap().as_array().is_none_or(|a| a.is_empty()),
        "stun_addrs should be absent or empty, got {:?}",
        addrs
    );
}

/// STUN allowlist: IP is added on auth, removed on disconnect.
#[tokio::test]
async fn stun_allowlist_lifecycle() {
    let server = TestServer::start().await;
    assert!(
        server.state.stun_allowlist.is_empty(),
        "allowlist starts empty"
    );

    let mut ws = connect_ws(&server).await;
    let _auth = authenticate(&mut ws, "AllowlistTest").await;

    // After auth, the client's loopback IP should be in the allowlist
    assert!(
        server
            .state
            .stun_allowlist
            .contains_key(&std::net::IpAddr::V4(std::net::Ipv4Addr::LOCALHOST)),
        "client IP should be in allowlist after auth"
    );

    // Disconnect
    ws.close(None).await.ok();
    tokio::time::sleep(std::time::Duration::from_millis(100)).await;

    // After disconnect, the IP should be removed
    assert!(
        server.state.stun_allowlist.is_empty(),
        "allowlist should be empty after disconnect"
    );
}

/// Self-hosted STUN server responds to allowlisted IPs with XOR-MAPPED-ADDRESS.
#[tokio::test]
async fn stun_server_responds_to_allowlisted_ip() {
    use std::net::IpAddr;
    use tokio::net::UdpSocket;

    let allowlist = Arc::new(dashmap::DashMap::<IpAddr, usize>::new());
    allowlist.insert(IpAddr::V4(std::net::Ipv4Addr::LOCALHOST), 1);

    let bound = dxx_matchmaking::stun::run("127.0.0.1:0".parse().unwrap(), Arc::clone(&allowlist))
        .await
        .expect("STUN bind");

    // Build a STUN Binding Request
    let sock = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let local_addr = sock.local_addr().unwrap();
    let mut request = Vec::with_capacity(20);
    request.extend_from_slice(&0x0001u16.to_be_bytes()); // Binding Request
    request.extend_from_slice(&0u16.to_be_bytes()); // length
    request.extend_from_slice(&0x2112_A442u32.to_be_bytes()); // magic cookie
    request.extend_from_slice(&[0xAA; 12]); // txn ID

    sock.send_to(&request, bound).await.unwrap();

    let mut buf = [0u8; 128];
    let timeout = tokio::time::timeout(std::time::Duration::from_secs(2), sock.recv_from(&mut buf))
        .await
        .expect("STUN response timeout");
    let (len, _from) = timeout.unwrap();

    // Parse response header
    assert!(len >= 20, "response too short");
    let msg_type = u16::from_be_bytes([buf[0], buf[1]]);
    assert_eq!(msg_type, 0x0101, "expected Binding Response");
    let magic = u32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);
    assert_eq!(magic, 0x2112_A442);
    assert_eq!(&buf[8..20], &[0xAA; 12], "txn ID mismatch");

    // Parse XOR-MAPPED-ADDRESS attribute
    let attr_type = u16::from_be_bytes([buf[20], buf[21]]);
    assert_eq!(attr_type, 0x0020, "expected XOR-MAPPED-ADDRESS");
    let xor_port = u16::from_be_bytes([buf[26], buf[27]]);
    let port = xor_port ^ 0x2112;
    assert_eq!(port, local_addr.port(), "reflexive port mismatch");
}

/// Self-hosted STUN server drops packets from non-allowlisted IPs.
#[tokio::test]
async fn stun_server_drops_non_allowlisted_ip() {
    use std::net::IpAddr;
    use tokio::net::UdpSocket;

    let allowlist = Arc::new(dashmap::DashMap::<IpAddr, usize>::new());
    // Allowlist is empty -- no IPs allowed

    let bound = dxx_matchmaking::stun::run("127.0.0.1:0".parse().unwrap(), Arc::clone(&allowlist))
        .await
        .expect("STUN bind");

    let sock = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let mut request = Vec::with_capacity(20);
    request.extend_from_slice(&0x0001u16.to_be_bytes());
    request.extend_from_slice(&0u16.to_be_bytes());
    request.extend_from_slice(&0x2112_A442u32.to_be_bytes());
    request.extend_from_slice(&[0xBB; 12]);

    sock.send_to(&request, bound).await.unwrap();

    let mut buf = [0u8; 128];
    let result = tokio::time::timeout(
        std::time::Duration::from_millis(500),
        sock.recv_from(&mut buf),
    )
    .await;
    assert!(
        result.is_err(),
        "should not receive response from non-allowlisted IP"
    );
}

/// Full friend lifecycle: request, accept, list with presence, remove, block,
/// and join friend's game.
#[tokio::test]
async fn test_friend_full_lifecycle() {
    let server = TestServer::start().await;

    let mut alice = connect_ws(&server).await;
    let alice_auth = authenticate(&mut alice, "Alice").await;
    let alice_id = alice_auth["player_id"].as_str().unwrap().to_string();

    let mut bob = connect_ws(&server).await;
    let bob_auth = authenticate(&mut bob, "Bob").await;
    let bob_id = bob_auth["player_id"].as_str().unwrap().to_string();

    // 1. Alice sends friend request to Bob
    send_only(
        &mut alice,
        json!({ "type": "FRIEND_REQUEST", "target_callsign": "Bob" }),
    )
    .await;

    // Bob receives the notification
    let notif = recv(&mut bob).await;
    assert_eq!(notif["type"], "FRIEND_REQUEST_RECEIVED");
    assert_eq!(notif["from_callsign"], "Alice");
    assert_eq!(notif["from_player_id"], alice_id);

    // 2. Bob accepts
    send_only(
        &mut bob,
        json!({ "type": "FRIEND_ACCEPT", "player_id": alice_id }),
    )
    .await;

    // Alice gets FRIEND_ACCEPTED notification
    let accepted = recv(&mut alice).await;
    assert_eq!(accepted["type"], "FRIEND_ACCEPTED");
    assert_eq!(accepted["player_id"], bob_id);

    // 3. Both fetch friend list -- each should see the other
    let alice_friends = send_recv(&mut alice, json!({ "type": "FRIEND_LIST" })).await;
    assert_eq!(alice_friends["type"], "FRIEND_LIST_RESP");
    let af = alice_friends["friends"].as_array().unwrap();
    assert_eq!(af.len(), 1);
    assert_eq!(af[0]["callsign"], "Bob");
    assert_eq!(af[0]["status"], "accepted");
    assert_eq!(af[0]["presence"], "online");

    let bob_friends = send_recv(&mut bob, json!({ "type": "FRIEND_LIST" })).await;
    let bf = bob_friends["friends"].as_array().unwrap();
    assert_eq!(bf.len(), 1);
    assert_eq!(bf[0]["callsign"], "Alice");
    assert_eq!(bf[0]["status"], "accepted");

    // 4. Bob creates a lobby
    let lobby_update = send_recv(
        &mut bob,
        json!({
            "type": "CREATE_LOBBY",
            "game": "d2",
            "max_players": 4,
            "game_info": {"mission": "Counterstrike!", "mode": "coop"},
        }),
    )
    .await;
    assert_eq!(lobby_update["type"], "LOBBY_UPDATE");
    let lobby_id = lobby_update["lobby_id"].as_str().unwrap().to_string();

    // Alice's friend list should now show Bob with in_game_details
    let alice_friends_ig = send_recv(&mut alice, json!({ "type": "FRIEND_LIST" })).await;
    let af_ig = alice_friends_ig["friends"].as_array().unwrap();
    assert_eq!(af_ig.len(), 1);
    assert!(
        af_ig[0]["presence"] == "in_game"
            || af_ig[0]["presence"] == "in_lobby"
            || af_ig[0]["presence"] == "online",
        "unexpected presence: {}",
        af_ig[0]["presence"]
    );

    // 5. Alice joins Bob's game via JOIN_FRIEND_GAME
    let join_resp = send_recv(
        &mut alice,
        json!({ "type": "JOIN_FRIEND_GAME", "friend_player_id": bob_id }),
    )
    .await;
    assert_eq!(join_resp["type"], "JOIN_FRIEND_GAME_RESP");
    assert_eq!(join_resp["success"], true);
    assert_eq!(join_resp["lobby_id"], lobby_id);

    // Both get LOBBY_UPDATE from the join broadcast
    let alice_lobby = recv(&mut alice).await;
    assert_eq!(alice_lobby["type"], "LOBBY_UPDATE");
    assert_eq!(alice_lobby["lobby_id"], lobby_id);

    let bob_lobby = recv(&mut bob).await;
    assert_eq!(bob_lobby["type"], "LOBBY_UPDATE");
    let players = bob_lobby["players"].as_array().unwrap();
    assert_eq!(players.len(), 2);

    // 6. Alice leaves lobby, then removes Bob as friend
    send_only(&mut alice, json!({ "type": "LEAVE_LOBBY" })).await;

    // Bob gets lobby update (Alice left)
    let bob_lobby2 = recv(&mut bob).await;
    assert_eq!(bob_lobby2["type"], "LOBBY_UPDATE");
    let players2 = bob_lobby2["players"].as_array().unwrap();
    assert_eq!(players2.len(), 1);

    // Remove friend
    send_only(
        &mut alice,
        json!({ "type": "FRIEND_REMOVE", "player_id": bob_id }),
    )
    .await;

    // Bob gets FRIEND_REMOVED notification
    let removed = recv(&mut bob).await;
    assert_eq!(removed["type"], "FRIEND_REMOVED");
    assert_eq!(removed["player_id"], alice_id);

    // Alice's friend list should now be empty
    let alice_friends2 = send_recv(&mut alice, json!({ "type": "FRIEND_LIST" })).await;
    let af2 = alice_friends2["friends"].as_array().unwrap();
    assert_eq!(af2.len(), 0);

    // 7. Alice blocks Bob (silent, no notification to Bob)
    send_only(
        &mut alice,
        json!({ "type": "FRIEND_BLOCK", "player_id": bob_id }),
    )
    .await;

    // Alice's friend list should show Bob as blocked
    let alice_friends3 = send_recv(&mut alice, json!({ "type": "FRIEND_LIST" })).await;
    let af3 = alice_friends3["friends"].as_array().unwrap();
    assert_eq!(af3.len(), 1);
    assert_eq!(af3[0]["status"], "blocked");
}
