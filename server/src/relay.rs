//! Version 3 relay: per-direction AES-256-GCM keys and proven UDP bindings.
//! Keys are delivered only over the authenticated control connection. The relay
//! terminates encryption; this is not end-to-end confidentiality from the server.
use std::collections::BTreeMap;
use std::net::SocketAddr;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use ring::{
    aead,
    rand::{SecureRandom, SystemRandom},
};
use tokio::net::UdpSocket;
use uuid::Uuid;

use crate::ServerState;

pub const HEADER_LEN: usize = 19;
pub const MAX_PAYLOAD: usize = 4096;
pub const MAX_PACKET: usize = HEADER_LEN + MAX_PAYLOAD + 16;
pub const DATA: u8 = 0;
pub const BIND: u8 = 1;
pub const CHALLENGE: u8 = 2;
pub const PROOF: u8 = 3;
pub const BOUND: u8 = 4;
const MAGIC: &[u8; 4] = b"DXR3";

/// Sliding replay window permits bounded packet reordering without nonce reuse
#[derive(Default)]
pub struct ReplayWindow {
    highest: u64,
    seen: u64,
}
impl ReplayWindow {
    pub fn accept(&mut self, seq: u64) -> bool {
        if seq == 0 || seq > i64::MAX as u64 {
            return false;
        }
        if seq > self.highest {
            let shift = seq - self.highest;
            self.seen = if shift >= 64 {
                1
            } else {
                (self.seen << shift) | 1
            };
            self.highest = seq;
            true
        } else {
            let delta = self.highest - seq;
            if delta >= 64 || self.seen & (1 << delta) != 0 {
                return false;
            }
            self.seen |= 1 << delta;
            true
        }
    }
}

#[derive(Clone, serde::Serialize)]
pub struct RelayCredentials {
    pub send_key: String,
    pub receive_key: String,
    pub own_slot: u8,
}

impl std::fmt::Debug for RelayCredentials {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RelayCredentials")
            .field("own_slot", &self.own_slot)
            .finish_non_exhaustive()
    }
}

struct PendingBind {
    address: SocketAddr,
    challenge: [u8; 16],
    expires: Instant,
    request_seq: u64,
}
struct Peer {
    player: Uuid,
    uplink: [u8; 32],
    downlink: [u8; 32],
    endpoint: Option<SocketAddr>,
    pending: Option<PendingBind>,
    replay: ReplayWindow,
    outgoing: u64,
    budget_start: Instant,
    budget_packets: u32,
    budget_bytes: usize,
}
struct RelayState {
    peers: BTreeMap<u8, Peer>,
    last_activity: Instant,
}
pub struct RelaySession {
    pub room_id: Uuid,
    /// New keys and epoch for every allocation, including restart and game start
    pub epoch: Uuid,
    inner: Mutex<RelayState>,
}

fn random<const N: usize>() -> [u8; N] {
    let mut bytes = [0; N];
    SystemRandom::new()
        .fill(&mut bytes)
        .expect("OS randomness unavailable");
    bytes
}

pub fn seal(
    key: &[u8; 32],
    token: u32,
    kind: u8,
    from: u8,
    to: u8,
    seq: u64,
    payload: &[u8],
) -> Option<Vec<u8>> {
    if seq == 0 || seq > i64::MAX as u64 || payload.len() > MAX_PAYLOAD {
        return None;
    }
    let mut header = Vec::from(MAGIC.as_slice());
    header.extend_from_slice(&token.to_le_bytes());
    header.extend_from_slice(&[kind, from, to]);
    header.extend_from_slice(&seq.to_le_bytes());
    let mut nonce = [0u8; 12];
    nonce[4..].copy_from_slice(&seq.to_be_bytes());
    let key = aead::LessSafeKey::new(aead::UnboundKey::new(&aead::AES_256_GCM, key).ok()?);
    let mut cipher = payload.to_vec();
    key.seal_in_place_append_tag(
        aead::Nonce::assume_unique_for_key(nonce),
        aead::Aad::from(&header),
        &mut cipher,
    )
    .ok()?;
    header.extend_from_slice(&cipher);
    Some(header)
}

pub fn open(key: &[u8; 32], packet: &[u8]) -> Option<Vec<u8>> {
    if packet.len() < HEADER_LEN + 16 || packet.len() > MAX_PACKET || &packet[..4] != MAGIC {
        return None;
    }
    let seq = u64::from_le_bytes(packet[11..19].try_into().ok()?);
    if seq == 0 || seq > i64::MAX as u64 {
        return None;
    }
    let mut nonce = [0u8; 12];
    nonce[4..].copy_from_slice(&seq.to_be_bytes());
    let key = aead::LessSafeKey::new(aead::UnboundKey::new(&aead::AES_256_GCM, key).ok()?);
    let mut cipher = packet[HEADER_LEN..].to_vec();
    let plain = key
        .open_in_place(
            aead::Nonce::assume_unique_for_key(nonce),
            aead::Aad::from(&packet[..HEADER_LEN]),
            &mut cipher,
        )
        .ok()?;
    Some(plain.to_vec())
}

impl RelaySession {
    pub fn new(room_id: Uuid, members: [(u8, Uuid); 2], now: Instant) -> Self {
        assert!(members[0].0 != members[1].0 && members.iter().all(|m| m.0 < 8));
        let peers = members
            .into_iter()
            .map(|(slot, player)| {
                (
                    slot,
                    Peer {
                        player,
                        uplink: random(),
                        downlink: random(),
                        endpoint: None,
                        pending: None,
                        replay: ReplayWindow::default(),
                        outgoing: 0,
                        budget_start: now,
                        budget_packets: 0,
                        budget_bytes: 0,
                    },
                )
            })
            .collect();
        Self {
            room_id,
            epoch: Uuid::new_v4(),
            inner: Mutex::new(RelayState {
                peers,
                last_activity: now,
            }),
        }
    }
    pub fn credentials(&self, slot: u8) -> Option<RelayCredentials> {
        let state = self.inner.lock().unwrap();
        let peer = state.peers.get(&slot)?;
        Some(RelayCredentials {
            own_slot: slot,
            send_key: hex::encode(peer.uplink),
            receive_key: hex::encode(peer.downlink),
        })
    }
    pub fn contains_player(&self, player: Uuid) -> bool {
        self.inner
            .lock()
            .unwrap()
            .peers
            .values()
            .any(|p| p.player == player)
    }
    fn reply(
        peer: &mut Peer,
        token: u32,
        kind: u8,
        from: u8,
        to: u8,
        payload: &[u8],
    ) -> Option<Vec<u8>> {
        peer.outgoing = peer.outgoing.checked_add(1)?;
        seal(
            &peer.downlink,
            token,
            kind,
            from,
            to,
            peer.outgoing,
            payload,
        )
    }
    /// No I/O under the lock; unproved or stale packets cannot mutate a route
    pub fn process(
        &self,
        packet: &[u8],
        source: SocketAddr,
        now: Instant,
    ) -> Option<(SocketAddr, Vec<u8>)> {
        if packet.len() < HEADER_LEN + 16 || packet.len() > MAX_PACKET || &packet[..4] != MAGIC {
            return None;
        }
        let token = u32::from_le_bytes(packet[4..8].try_into().ok()?);
        let (kind, from, to) = (packet[8], packet[9], packet[10]);
        let seq = u64::from_le_bytes(packet[11..19].try_into().ok()?);
        let mut state = self.inner.lock().unwrap();
        if from == to || !state.peers.contains_key(&to) {
            return None;
        }
        let sender = state.peers.get_mut(&from)?;
        // Bound per-session work before authentication, without changing route/replay state
        if now.duration_since(sender.budget_start) >= Duration::from_secs(1) {
            sender.budget_start = now;
            sender.budget_packets = 0;
            sender.budget_bytes = 0;
        }
        if sender.budget_packets >= 256 || sender.budget_bytes + packet.len() > 1024 * 1024 {
            return None;
        }
        sender.budget_packets += 1;
        sender.budget_bytes += packet.len();
        let payload = open(&sender.uplink, packet)?;
        if !sender.replay.accept(seq) {
            return None;
        }
        match kind {
            BIND if payload.len() == 16 => {
                // A reordered older bind must not replace a newer pending route
                if seq != sender.replay.highest {
                    return None;
                }
                if sender.endpoint == Some(source) {
                    let result =
                        Self::reply(sender, token, BOUND, to, from, &[]).map(|p| (source, p));
                    state.last_activity = now;
                    return result;
                }
                // One outstanding challenge for up to 5 seconds; no unbounded reflection
                if sender.pending.as_ref().is_some_and(|p| now < p.expires) {
                    return None;
                }
                let challenge = random();
                sender.pending = Some(PendingBind {
                    address: source,
                    challenge,
                    expires: now + Duration::from_secs(5),
                    request_seq: seq,
                });
                Self::reply(sender, token, CHALLENGE, to, from, &challenge).map(|p| (source, p))
            }
            PROOF => {
                let pending = sender.pending.as_ref()?;
                if pending.address != source
                    || now >= pending.expires
                    || seq <= pending.request_seq
                    || seq != sender.replay.highest
                    || payload != pending.challenge
                {
                    return None;
                }
                sender.endpoint = Some(source);
                sender.pending = None;
                let result = Self::reply(sender, token, BOUND, to, from, &[]).map(|p| (source, p));
                state.last_activity = now;
                result
            }
            DATA if sender.endpoint == Some(source) => {
                let destination = state.peers.get_mut(&to)?;
                let addr = destination.endpoint?;
                let result =
                    Self::reply(destination, token, DATA, from, to, &payload).map(|p| (addr, p));
                state.last_activity = now;
                result
            }
            _ => None,
        }
    }
}

pub fn revoke_room(state: &ServerState, room: Uuid) {
    state
        .relay_sessions
        .retain(|_, session| session.room_id != room);
}
pub fn revoke_player(state: &ServerState, player: Uuid) {
    state
        .relay_sessions
        .retain(|_, session| !session.contains_player(player));
}
pub fn cleanup_stale_sessions(state: &ServerState) -> usize {
    cleanup_sessions_older_than(state, Duration::from_secs(120))
}
pub fn cleanup_sessions_older_than(state: &ServerState, cutoff: Duration) -> usize {
    let before = state.relay_sessions.len();
    let now = Instant::now();
    state.relay_sessions.retain(|_, session| {
        now.duration_since(session.inner.lock().unwrap().last_activity) < cutoff
    });
    before - state.relay_sessions.len()
}
pub async fn run(
    addr: SocketAddr,
    state: Arc<ServerState>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    run_socket(UdpSocket::bind(addr).await?, state).await?;
    Ok(())
}
pub async fn run_socket(socket: UdpSocket, state: Arc<ServerState>) -> std::io::Result<()> {
    let mut buf = vec![0u8; MAX_PACKET + 1];
    loop {
        let (len, source) = socket.recv_from(&mut buf).await?;
        if !(HEADER_LEN + 16..=MAX_PACKET).contains(&len) || &buf[..4] != MAGIC {
            continue;
        }
        let token = u32::from_le_bytes(buf[4..8].try_into().unwrap());
        let forward = state
            .relay_sessions
            .get(&token)
            .and_then(|s| s.process(&buf[..len], source, Instant::now()));
        if let Some((address, packet)) = forward {
            socket.send_to(&packet, address).await?;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn key(value: &str) -> [u8; 32] {
        hex::decode(value).unwrap().try_into().unwrap()
    }
    fn bind(
        session: &RelaySession,
        token: u32,
        own: u8,
        peer: u8,
        addr: SocketAddr,
        seq: &mut u64,
        now: Instant,
    ) {
        let credentials = session.credentials(own).unwrap();
        *seq += 1;
        let request = seal(
            &key(&credentials.send_key),
            token,
            BIND,
            own,
            peer,
            *seq,
            &[0; 16],
        )
        .unwrap();
        let (_, challenge) = session.process(&request, addr, now).unwrap();
        let proof = open(&key(&credentials.receive_key), &challenge).unwrap();
        *seq += 1;
        let packet = seal(
            &key(&credentials.send_key),
            token,
            PROOF,
            own,
            peer,
            *seq,
            &proof,
        )
        .unwrap();
        let (_, reply) = session.process(&packet, addr, now).unwrap();
        assert_eq!(reply[8], BOUND);
    }
    #[test]
    fn cross_language_aes_gcm_fixture() {
        let key: [u8; 32] = std::array::from_fn(|i| i as u8);
        let frame = seal(&key, 0x11223344, DATA, 0, 2, 1, &key).unwrap();
        assert_eq!(hex::encode(&frame),"4458523344332211000002010000000000000015d7bdff40f1361906275b32e0ab34f8058e0e4770a742936b1919c23a2d6d415e459fba6bf2d29a8c42b21b34632496");
        assert_eq!(open(&key, &frame).unwrap(), key);
    }
    #[test]
    fn all_global_pairs_require_binding_and_reject_replay() {
        for a in 0..8 {
            for b in a + 1..8 {
                let now = Instant::now();
                let session = RelaySession::new(
                    Uuid::new_v4(),
                    [(a, Uuid::new_v4()), (b, Uuid::new_v4())],
                    now,
                );
                let ca = session.credentials(a).unwrap();
                let cb = session.credentials(b).unwrap();
                let addr_a = "127.0.0.1:10001".parse().unwrap();
                let addr_b = "127.0.0.1:10002".parse().unwrap();
                let mut seq_a = 0;
                let mut seq_b = 0;
                let premature = seal(&key(&ca.send_key), 42, DATA, a, b, 1, b"unbound").unwrap();
                assert!(session.process(&premature, addr_a, now).is_none());
                seq_a += 1;
                bind(&session, 42, a, b, addr_a, &mut seq_a, now);
                bind(&session, 42, b, a, addr_b, &mut seq_b, now);
                seq_a += 1;
                let payload = vec![0x22; MAX_PAYLOAD];
                let packet = seal(&key(&ca.send_key), 42, DATA, a, b, seq_a, &payload).unwrap();
                let (dest, forward) = session.process(&packet, addr_a, now).unwrap();
                assert_eq!(dest, addr_b);
                assert_eq!(open(&key(&cb.receive_key), &forward).unwrap(), payload);
                assert!(session.process(&packet, addr_a, now).is_none());
                let mut corrupt = packet.clone();
                corrupt[HEADER_LEN] ^= 1;
                assert!(session.process(&corrupt, addr_a, now).is_none());
                // A proven new endpoint replaces the old one; late old traffic cannot undo it
                let moved = "127.0.0.2:10003".parse().unwrap();
                bind(&session, 42, a, b, moved, &mut seq_a, now);
                assert!(session.process(&packet, addr_a, now).is_none());
                seq_b += 1;
                let reverse = seal(&key(&cb.send_key), 42, DATA, b, a, seq_b, b"reply").unwrap();
                assert_eq!(session.process(&reverse, addr_b, now).unwrap().0, moved);
                // New allocation means new epoch and keys, even when lookup ID is reused
                let replacement = RelaySession::new(
                    session.room_id,
                    [(a, Uuid::new_v4()), (b, Uuid::new_v4())],
                    now,
                );
                assert_ne!(replacement.epoch, session.epoch);
                assert!(replacement.process(&packet, addr_a, now).is_none());
            }
        }
    }
    #[test]
    fn replay_window_accepts_reordering_but_not_duplicates_or_old_packets() {
        let mut w = ReplayWindow::default();
        assert!(w.accept(100));
        assert!(w.accept(98));
        assert!(w.accept(99));
        assert!(!w.accept(98));
        assert!(!w.accept(36));
        assert!(!w.accept(0));
        assert!(w.accept(200));
        assert!(!w.accept(100));
    }
}
