//! Live UDP coverage of the candidate ownership proof, before control-plane integration

use std::net::SocketAddr;
use std::sync::Arc;
use std::time::{Duration, Instant};

use dxx_matchmaking::candidate_auth::{
    AttemptScope, CandidateAttempts, MAX_RESPONSES_PER_OBSERVATION,
};
use dxx_matchmaking::stun::{authenticated_stun_loop, AUTH_REQUEST_LEN, AUTH_RESPONSE_LEN};
use tokio::net::UdpSocket;
use tokio::task::JoinHandle;
use uuid::Uuid;

struct ProofListeners {
    attempts: Arc<CandidateAttempts>,
    addresses: Vec<SocketAddr>,
    tasks: Vec<JoinHandle<()>>,
}

impl Drop for ProofListeners {
    fn drop(&mut self) {
        for task in &self.tasks {
            task.abort();
        }
    }
}

impl ProofListeners {
    async fn start() -> Self {
        let attempts = Arc::new(CandidateAttempts::default());
        let mut addresses = Vec::new();
        let mut tasks = Vec::new();
        for listener in 0..2 {
            let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
            addresses.push(socket.local_addr().unwrap());
            let attempts = Arc::clone(&attempts);
            tasks.push(tokio::spawn(async move {
                authenticated_stun_loop(&socket, &attempts, listener).await;
            }));
        }
        Self {
            attempts,
            addresses,
            tasks,
        }
    }
}

fn scope() -> AttemptScope {
    AttemptScope {
        player_id: Uuid::new_v4(),
        connection_id: Uuid::new_v4(),
        lobby_id: Uuid::new_v4(),
        admission_id: Uuid::new_v4(),
        public_ip: "127.0.0.1".parse().unwrap(),
        local_development: true,
    }
}

// Identical vector lives in StunProofClientTest.kt
fn request(credential: Uuid) -> Vec<u8> {
    let mut bytes = hex::decode(
        "000100142112a4420102030405060708090a0b0cc0010010102132435465768798a9bacbdcedfe0f",
    )
    .unwrap();
    bytes[24..40].copy_from_slice(credential.as_bytes());
    bytes
}

async fn response(socket: &UdpSocket, server: SocketAddr) -> Vec<u8> {
    let mut bytes = [0u8; AUTH_RESPONSE_LEN + 1];
    let (length, source) =
        tokio::time::timeout(Duration::from_secs(2), socket.recv_from(&mut bytes))
            .await
            .unwrap()
            .unwrap();
    assert_eq!(source, server);
    assert_eq!(length, AUTH_RESPONSE_LEN);
    bytes[..length].to_vec()
}

async fn silent(socket: &UdpSocket) {
    assert!(
        tokio::time::timeout(Duration::from_millis(100), socket.recv(&mut [0u8; 64]))
            .await
            .is_err()
    );
}

fn receipt(response: &[u8]) -> Uuid {
    assert_eq!(&response[..8], &hex::decode("010100202112a442").unwrap());
    assert_eq!(
        &response[8..20],
        &hex::decode("0102030405060708090a0b0c").unwrap()
    );
    assert_eq!(&response[20..26], &hex::decode("002000080001").unwrap());
    assert_eq!(&response[32..36], &hex::decode("c0020010").unwrap());
    Uuid::from_slice(&response[36..52]).unwrap()
}

#[tokio::test]
async fn two_listeners_prove_the_observed_endpoint_without_accepting_any_claimed_address() {
    let listeners = ProofListeners::start().await;
    let scope = scope();
    let challenge = listeners.attempts.issue(scope, Instant::now()).unwrap();
    let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let source = socket.local_addr().unwrap();
    let mut receipts = Vec::new();
    for server in &listeners.addresses {
        socket
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
        let bytes = response(&socket, *server).await;
        let port = u16::from_be_bytes(bytes[26..28].try_into().unwrap()) ^ 0x2112;
        let ip = u32::from_be_bytes(bytes[28..32].try_into().unwrap()) ^ 0x2112A442;
        assert_eq!(port, source.port());
        assert_eq!(
            std::net::Ipv4Addr::from(ip),
            "127.0.0.1".parse::<std::net::Ipv4Addr>().unwrap()
        );
        receipts.push(receipt(&bytes));
    }
    assert_ne!(receipts[0], receipts[1]);
    assert!(listeners
        .attempts
        .consume(
            scope,
            challenge.attempt_id,
            &[challenge.credential],
            Instant::now()
        )
        .is_none());
    let verified = listeners
        .attempts
        .consume(scope, challenge.attempt_id, &receipts, Instant::now())
        .unwrap();
    assert_eq!(verified.endpoints, vec![source]);
    assert_eq!(verified.nat_type, "full_cone");
    socket
        .send_to(&request(challenge.credential), listeners.addresses[0])
        .await
        .unwrap();
    silent(&socket).await;
}

#[tokio::test]
async fn a_listener_never_rotates_endpoint_or_receipt_and_emits_at_most_three_responses() {
    let listeners = ProofListeners::start().await;
    let scope = scope();
    let challenge = listeners.attempts.issue(scope, Instant::now()).unwrap();
    let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let other_port = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let server = listeners.addresses[0];
    let mut receipts = Vec::new();
    for _ in 0..MAX_RESPONSES_PER_OBSERVATION {
        socket
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
        receipts.push(receipt(&response(&socket, server).await));
        other_port
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
        silent(&other_port).await;
    }
    assert!(receipts.iter().all(|value| *value == receipts[0]));
    for _ in 0..20 {
        socket
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
        other_port
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
    }
    silent(&socket).await;
    silent(&other_port).await;
    let verified = listeners
        .attempts
        .consume(scope, challenge.attempt_id, &receipts[..1], Instant::now())
        .unwrap();
    assert_eq!(verified.endpoints, vec![socket.local_addr().unwrap()]);
}

#[tokio::test]
async fn symmetric_mapping_proves_two_distinct_ports_without_predicting_others() {
    let listeners = ProofListeners::start().await;
    let scope = scope();
    let challenge = listeners.attempts.issue(scope, Instant::now()).unwrap();
    let mut endpoints = Vec::new();
    let mut receipts = Vec::new();
    let a = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let b = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    for (socket, server) in [&a, &b].iter().zip(&listeners.addresses) {
        socket
            .send_to(&request(challenge.credential), server)
            .await
            .unwrap();
        receipts.push(receipt(&response(socket, *server).await));
        endpoints.push(socket.local_addr().unwrap());
    }
    let verified = listeners
        .attempts
        .consume(scope, challenge.attempt_id, &receipts, Instant::now())
        .unwrap();
    assert_eq!(verified.endpoints, endpoints);
    assert_eq!(verified.nat_type, "symmetric");
}

#[tokio::test]
async fn malformed_uncredentialed_and_wrong_ip_requests_emit_no_response_or_observation() {
    let listeners = ProofListeners::start().await;
    let scope = scope();
    let challenge = listeners.attempts.issue(scope, Instant::now()).unwrap();
    let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let server = listeners.addresses[0];
    let valid = request(challenge.credential);
    let mut malformed = vec![
        valid[..20].to_vec(),
        valid[..39].to_vec(),
        [valid.clone(), vec![0]].concat(),
    ];
    for index in [0, 1, 2, 3, 4, 7, 20, 21, 22, 23] {
        let mut bytes = valid.clone();
        bytes[index] ^= 0x80;
        malformed.push(bytes);
    }
    malformed.push(request(challenge.attempt_id));
    malformed.push(request(Uuid::new_v4()));
    // A valid secret never authorizes a source IP different from the control connection
    let remote_scope = AttemptScope {
        player_id: Uuid::new_v4(),
        public_ip: "8.8.8.8".parse().unwrap(),
        local_development: false,
        ..scope
    };
    let remote = listeners
        .attempts
        .issue(remote_scope, Instant::now())
        .unwrap();
    malformed.push(request(remote.credential));
    for bytes in malformed {
        socket.send_to(&bytes, server).await.unwrap();
        silent(&socket).await;
    }
    socket.send_to(&valid, server).await.unwrap();
    let proof = receipt(&response(&socket, server).await);
    assert!(listeners
        .attempts
        .consume(scope, challenge.attempt_id, &[proof], Instant::now())
        .is_some());
}

#[tokio::test]
async fn revocation_replacement_and_cross_player_reports_cannot_redeem_old_observations() {
    let listeners = ProofListeners::start().await;
    let scope = scope();
    let challenge = listeners.attempts.issue(scope, Instant::now()).unwrap();
    let socket = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let server = listeners.addresses[0];
    socket
        .send_to(&request(challenge.credential), server)
        .await
        .unwrap();
    let old_receipt = receipt(&response(&socket, server).await);
    let other = AttemptScope {
        player_id: Uuid::new_v4(),
        ..scope
    };
    let other_challenge = listeners.attempts.issue(other, Instant::now()).unwrap();
    assert!(listeners
        .attempts
        .consume(
            other,
            other_challenge.attempt_id,
            &[old_receipt],
            Instant::now()
        )
        .is_none());
    listeners.attempts.revoke(scope);
    socket
        .send_to(&request(challenge.credential), server)
        .await
        .unwrap();
    silent(&socket).await;
    let next = AttemptScope {
        admission_id: Uuid::new_v4(),
        ..scope
    };
    let fresh = listeners.attempts.issue(next, Instant::now()).unwrap();
    assert!(listeners
        .attempts
        .consume(next, fresh.attempt_id, &[old_receipt], Instant::now())
        .is_none());
    listeners.attempts.revoke(scope);
    socket
        .send_to(&request(fresh.credential), server)
        .await
        .unwrap();
    let fresh_receipt = receipt(&response(&socket, server).await);
    assert_ne!(fresh_receipt, old_receipt);
    listeners.attempts.revoke_lobby(next.lobby_id);
    socket
        .send_to(&request(fresh.credential), server)
        .await
        .unwrap();
    silent(&socket).await;
}

#[test]
fn request_matches_android_wire_vector() {
    let credential = "10213243-5465-7687-98a9-bacbdcedfe0f".parse().unwrap();
    let bytes = request(credential);
    assert_eq!(bytes.len(), AUTH_REQUEST_LEN);
    assert_eq!(
        hex::encode(bytes),
        "000100142112a4420102030405060708090a0b0cc0010010102132435465768798a9bacbdcedfe0f"
    );
}
