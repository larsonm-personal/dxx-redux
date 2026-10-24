//! Fixed-shape STUN discovery with admission credentials and return-path receipts
//! Two listeners observe mappings; the control plane redeems receipts before distributing endpoints

use std::net::SocketAddr;
use tokio::net::UdpSocket;
use tracing::warn;

// STUN constants (RFC 5389)
const STUN_MAGIC: u32 = 0x2112_A442;
const STUN_BINDING_REQUEST: u16 = 0x0001;
const STUN_BINDING_RESPONSE: u16 = 0x0101;
const STUN_ATTR_XOR_MAPPED_ADDRESS: u16 = 0x0020;

// Private discovery extension, kept in sync with StunProofClient.kt
pub const AUTH_REQUEST_LEN: usize = 40;
pub const AUTH_RESPONSE_LEN: usize = 52;
const STUN_ATTR_CANDIDATE_CREDENTIAL: u16 = 0xC001;
const STUN_ATTR_CANDIDATE_RECEIPT: u16 = 0xC002;

/// Fixed-shape, credentialed requests only; no caller-selected return address
/// The control plane must redeem receipts before authorizing any peer probes
pub async fn authenticated_stun_loop(
    socket: &UdpSocket,
    attempts: &crate::candidate_auth::CandidateAttempts,
    listener: usize,
) {
    if listener >= crate::candidate_auth::MAX_OBSERVATIONS {
        return;
    }
    // One extra byte distinguishes oversized/truncated packets from exact requests
    let mut buf = [0u8; AUTH_REQUEST_LEN + 1];
    let mut budget = crate::resource_budget::SourceBudget::new(512, 32);
    let mut next_error_log = std::time::Instant::now();
    loop {
        let (len, source) = match socket.recv_from(&mut buf).await {
            Ok(packet) => packet,
            Err(error) => {
                // Some platforms report oversized/ICMP datagrams as receive errors
                let now = std::time::Instant::now();
                if now >= next_error_log {
                    warn!(%error, "Authenticated STUN recv error");
                    next_error_log = now + std::time::Duration::from_secs(60);
                }
                continue;
            }
        };
        if len != AUTH_REQUEST_LEN
            || buf[0..2] != STUN_BINDING_REQUEST.to_be_bytes()
            || buf[2..4] != 20u16.to_be_bytes()
            || buf[4..8] != STUN_MAGIC.to_be_bytes()
            || buf[20..22] != STUN_ATTR_CANDIDATE_CREDENTIAL.to_be_bytes()
            || buf[22..24] != 16u16.to_be_bytes()
        {
            continue;
        }
        let now = std::time::Instant::now();
        if !budget.accept(source.ip(), now) {
            continue;
        }
        let credential = uuid::Uuid::from_slice(&buf[24..40]).unwrap();
        let Some(receipt) = attempts.observe(credential, listener, source, now) else {
            continue;
        };
        let SocketAddr::V4(source) = source else {
            continue;
        };
        let response = authenticated_response(buf[8..20].try_into().unwrap(), source, receipt);
        let _ = socket.send_to(&response, source).await;
    }
}

fn authenticated_response(
    transaction: &[u8; 12],
    source: std::net::SocketAddrV4,
    receipt: uuid::Uuid,
) -> [u8; AUTH_RESPONSE_LEN] {
    let mut response = [0u8; AUTH_RESPONSE_LEN];
    response[0..2].copy_from_slice(&STUN_BINDING_RESPONSE.to_be_bytes());
    response[2..4].copy_from_slice(&32u16.to_be_bytes());
    response[4..8].copy_from_slice(&STUN_MAGIC.to_be_bytes());
    response[8..20].copy_from_slice(transaction);
    response[20..22].copy_from_slice(&STUN_ATTR_XOR_MAPPED_ADDRESS.to_be_bytes());
    response[22..24].copy_from_slice(&8u16.to_be_bytes());
    response[25] = 1;
    response[26..28].copy_from_slice(&(source.port() ^ (STUN_MAGIC >> 16) as u16).to_be_bytes());
    response[28..32].copy_from_slice(&(u32::from(*source.ip()) ^ STUN_MAGIC).to_be_bytes());
    response[32..34].copy_from_slice(&STUN_ATTR_CANDIDATE_RECEIPT.to_be_bytes());
    response[34..36].copy_from_slice(&16u16.to_be_bytes());
    response[36..52].copy_from_slice(receipt.as_bytes());
    response
}

#[cfg(test)]
mod authenticated_tests {
    use super::*;

    #[test]
    fn response_matches_android_wire_vector() {
        let transaction = hex::decode("0102030405060708090a0b0c").unwrap();
        let response = authenticated_response(
            transaction.as_slice().try_into().unwrap(),
            "8.8.4.4:4242".parse().unwrap(),
            "00112233-4455-6677-8899-aabbccddeeff".parse().unwrap(),
        );
        assert_eq!(hex::encode(response), "010100202112a4420102030405060708090a0b0c0020000800013180291aa046c002001000112233445566778899aabbccddeeff");
    }
}
