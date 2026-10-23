//! Version 3 relay: per-direction AES-256-GCM keys and proven UDP bindings.
//! Keys are delivered only over the authenticated control connection. The relay
//! terminates encryption; this is not end-to-end confidentiality from the server.
use std::collections::{BTreeMap, HashMap};
use std::net::{IpAddr, SocketAddr};
use std::sync::atomic::{AtomicBool, Ordering};
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
const CONSENT_REFRESH: Duration = Duration::from_secs(15);
const CONSENT_LIFETIME: Duration = Duration::from_secs(30);

/// Sliding replay window permits bounded packet reordering without nonce reuse
#[derive(Clone, Copy, Default)]
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
    pub allocation_epoch: Uuid,
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
    proved_at: Option<Instant>,
    pending: Option<PendingBind>,
    replay: ReplayWindow,
    outgoing: u64,
    budget_start: Instant,
    budget_packets: u32,
    budget_bytes: usize,
    forwarded_packets: u64,
    forwarded_wire_bytes: u64,
}
impl Peer {
    fn consented(&self, now: Instant) -> bool {
        self.endpoint.is_some()
            && self
                .proved_at
                .is_some_and(|proved| now.saturating_duration_since(proved) < CONSENT_LIFETIME)
    }
}
struct RelayState {
    peers: BTreeMap<u8, Peer>,
    last_activity: Instant,
    expires_at: Instant,
}
#[derive(Clone)]
pub struct RelayOwnership {
    pub game_epoch: Uuid,
    pub members: [(u8, Uuid, Uuid); 2],
}
pub struct RelaySession {
    pub room_id: Uuid,
    /// New keys and epoch for every allocation, including restart and game start
    pub epoch: Uuid,
    pub created_at: Instant,
    pub owner: Option<RelayOwnership>,
    pub primary: AtomicBool,
    pub renewal_attempted: AtomicBool,
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
        Self::create(room_id, members, now, Duration::from_secs(7200), None, true)
    }
    pub fn owned(
        room_id: Uuid,
        owner: RelayOwnership,
        now: Instant,
        lifetime: Duration,
        primary: bool,
    ) -> Self {
        Self::create(
            room_id,
            owner.members.map(|(slot, player, _)| (slot, player)),
            now,
            lifetime,
            Some(owner),
            primary,
        )
    }
    fn create(
        room_id: Uuid,
        members: [(u8, Uuid); 2],
        now: Instant,
        lifetime: Duration,
        owner: Option<RelayOwnership>,
        primary: bool,
    ) -> Self {
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
                        proved_at: None,
                        pending: None,
                        replay: ReplayWindow::default(),
                        outgoing: 0,
                        budget_start: now,
                        budget_packets: 0,
                        budget_bytes: 0,
                        forwarded_packets: 0,
                        forwarded_wire_bytes: 0,
                    },
                )
            })
            .collect();
        Self {
            room_id,
            epoch: Uuid::new_v4(),
            created_at: now,
            owner,
            primary: AtomicBool::new(primary),
            renewal_attempted: AtomicBool::new(false),
            inner: Mutex::new(RelayState {
                peers,
                last_activity: now,
                expires_at: now + lifetime,
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
            allocation_epoch: self.epoch,
        })
    }
    pub fn expires_at(&self) -> Instant {
        self.inner.lock().unwrap().expires_at
    }
    pub fn retire_at(&self, deadline: Instant) {
        let mut state = self.inner.lock().unwrap();
        state.expires_at = state.expires_at.min(deadline);
    }
    pub fn both_bound(&self, now: Instant) -> bool {
        self.inner
            .lock()
            .unwrap()
            .peers
            .values()
            .all(|peer| peer.consented(now))
    }
    pub fn is_bound(&self, slot: u8, now: Instant) -> bool {
        self.inner
            .lock()
            .unwrap()
            .peers
            .get(&slot)
            .is_some_and(|p| p.consented(now))
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
    // Cheap routing checks are filters only; every admitted packet still needs AEAD
    fn plausible_source(&self, packet: &[u8], source: SocketAddr, now: Instant) -> bool {
        let state = self.inner.lock().unwrap();
        if now >= state.expires_at {
            return false;
        }
        let Some(peer) = state.peers.get(&packet[9]) else {
            return false;
        };
        if packet[9] == packet[10] || !state.peers.contains_key(&packet[10]) {
            return false;
        }
        match packet[8] {
            DATA => {
                let mut replay = peer.replay;
                peer.endpoint == Some(source)
                    && peer.consented(now)
                    && replay.accept(u64::from_le_bytes(packet[11..19].try_into().unwrap()))
            }
            BIND => packet.len() == HEADER_LEN + 32,
            PROOF => {
                packet.len() == HEADER_LEN + 32
                    && peer.pending.as_ref().is_some_and(|p| p.address == source)
            }
            _ => false,
        }
    }
    /// No I/O under the lock; unproved or stale packets cannot mutate a route
    #[cfg(test)]
    fn process(
        &self,
        packet: &[u8],
        source: SocketAddr,
        now: Instant,
    ) -> Option<(SocketAddr, Vec<u8>)> {
        self.process_bounded(packet, source, now, None, None)
    }
    fn process_bounded(
        &self,
        packet: &[u8],
        source: SocketAddr,
        now: Instant,
        budget: Option<&Mutex<crate::resource_budget::RelayBudget>>,
        economy: Option<&Mutex<crate::relay_economy::RelayEconomy>>,
    ) -> Option<(SocketAddr, Vec<u8>)> {
        if packet.len() < HEADER_LEN + 16 || packet.len() > MAX_PACKET || &packet[..4] != MAGIC {
            return None;
        }
        let token = u32::from_le_bytes(packet[4..8].try_into().ok()?);
        let (kind, from, to) = (packet[8], packet[9], packet[10]);
        let seq = u64::from_le_bytes(packet[11..19].try_into().ok()?);
        let mut state = self.inner.lock().unwrap();
        if now >= state.expires_at {
            return None;
        }
        if from == to || !state.peers.contains_key(&to) {
            return None;
        }
        let sender = state.peers.get_mut(&from)?;
        // Invalid ciphertext and replay must never debit a claimed peer's quota
        let payload = open(&sender.uplink, packet)?;
        if !sender.replay.accept(seq) {
            return None;
        }
        if let Some(budget) = budget {
            if !budget
                .lock()
                .unwrap()
                .accept(sender.player, self.room_id, packet.len(), now)
            {
                return None;
            }
        }
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
        let sender_id = sender.player;
        let result = match kind {
            BIND if payload.len() == 16 => {
                // A reordered older bind must not replace a newer pending route
                if seq != sender.replay.highest {
                    return None;
                }
                if sender.endpoint == Some(source)
                    && sender.proved_at.is_some_and(|proved| {
                        now.saturating_duration_since(proved) < CONSENT_REFRESH
                    })
                {
                    Self::reply(sender, token, BOUND, to, from, &[]).map(|p| (source, p))
                } else {
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
            }
            PROOF => {
                let pending = sender.pending.as_ref()?;
                if pending.address != source
                    || now >= pending.expires
                    || seq <= pending.request_seq
                    || (sender.endpoint != Some(source) && seq != sender.replay.highest)
                    || payload != pending.challenge
                {
                    return None;
                }
                sender.endpoint = Some(source);
                sender.proved_at = Some(now);
                sender.pending = None;
                Self::reply(sender, token, BOUND, to, from, &[]).map(|p| (source, p))
            }
            DATA if sender.endpoint == Some(source) && sender.consented(now) => {
                let destination = state.peers.get_mut(&to)?;
                if !destination.consented(now) {
                    return None;
                }
                let addr = destination.endpoint?;
                Self::reply(destination, token, DATA, from, to, &payload).map(|p| (addr, p))
            }
            _ => None,
        }?;
        let wire_bytes = result.1.len() + crate::relay_economy::WIRE_OVERHEAD;
        if economy.is_some_and(|e| !e.lock().unwrap().reserve(sender_id, wire_bytes, now)) {
            return None;
        }
        let sender = state.peers.get_mut(&from)?;
        sender.forwarded_packets = sender.forwarded_packets.saturating_add(1);
        sender.forwarded_wire_bytes = sender
            .forwarded_wire_bytes
            .saturating_add(wire_bytes as u64);
        if kind != BIND || (sender.endpoint == Some(source) && sender.consented(now)) {
            state.last_activity = now;
        }
        Some(result)
    }

    pub fn usage(&self) -> serde_json::Value {
        let state = self.inner.lock().unwrap();
        serde_json::json!({"room_id": self.room_id, "allocation_epoch": self.epoch,
            "peers": state.peers.values().map(|p| serde_json::json!({"player_id":p.player,
                "forwarded_packets":p.forwarded_packets,"forwarded_wire_bytes":p.forwarded_wire_bytes})).collect::<Vec<_>>()})
    }
}

pub fn revoke_room(state: &ServerState, room: Uuid) {
    state
        .relay_sessions
        .retain(|_, session| session.room_id != room);
    state
        .relay_renewals
        .lock()
        .unwrap()
        .retain(|_, renewal| renewal.room_id != room);
}
pub fn revoke_player(state: &ServerState, player: Uuid) {
    state
        .relay_sessions
        .retain(|_, session| !session.contains_player(player));
    state
        .relay_renewals
        .lock()
        .unwrap()
        .retain(|_, renewal| !renewal.owner.members.iter().any(|(_, p, _)| *p == player));
}
pub fn logical_sessions(state: &ServerState) -> usize {
    state
        .relay_sessions
        .iter()
        .filter(|entry| entry.primary.load(Ordering::Relaxed))
        .count()
}
pub fn cleanup_stale_sessions(state: &ServerState) -> usize {
    cleanup_sessions_older_than(state, Duration::from_secs(120))
}
pub fn cleanup_sessions_older_than(state: &ServerState, cutoff: Duration) -> usize {
    let before = state.relay_sessions.len();
    let now = Instant::now();
    state.relay_sessions.retain(|_, session| {
        let inner = session.inner.lock().unwrap();
        now < inner.expires_at && now.saturating_duration_since(inner.last_activity) < cutoff
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
// Separate untrusted source/global work budget, independent of claimed slot identity
struct CryptoAdmission {
    handshake_start: Instant,
    handshake_pairs: HashMap<u32, usize>,
    handshakes: crate::resource_budget::SourceBudget,
    started: Instant,
    packets: usize,
    pairs: HashMap<u32, usize>,
}
impl CryptoAdmission {
    fn new() -> Self {
        Self {
            handshake_start: Instant::now(),
            handshake_pairs: HashMap::new(),
            handshakes: crate::resource_budget::SourceBudget::new(2048, 512),
            started: Instant::now(),
            packets: 0,
            pairs: HashMap::new(),
        }
    }
    fn accept_handshake(&mut self, source: IpAddr, token: u32, now: Instant) -> bool {
        if now.duration_since(self.handshake_start) >= Duration::from_secs(1) {
            self.handshake_start = now;
            self.handshake_pairs.clear();
        }
        if self.handshake_pairs.get(&token).copied().unwrap_or(0) >= 32
            || !self.handshakes.accept(source, now)
        {
            return false;
        }
        *self.handshake_pairs.entry(token).or_default() += 1;
        true
    }
    fn accept(&mut self, token: u32, now: Instant) -> bool {
        if now.duration_since(self.started) >= Duration::from_secs(1) {
            self.started = now;
            self.packets = 0;
            self.pairs.clear();
        }
        // At most 100 logical pairs, each with one bounded replacement during renewal
        // The global ceiling equals their sum, so another pair cannot spend it
        if self.packets >= 51_200
            || self.pairs.get(&token).copied().unwrap_or(0) >= 256
            || (!self.pairs.contains_key(&token) && self.pairs.len() >= 200)
        {
            return false;
        }
        self.packets += 1;
        *self.pairs.entry(token).or_default() += 1;
        true
    }
}

fn dispatch_packet(
    sessions: &dashmap::DashMap<u32, RelaySession>,
    budget: &Mutex<crate::resource_budget::RelayBudget>,
    economy: Option<&Mutex<crate::relay_economy::RelayEconomy>>,
    admission: &mut CryptoAdmission,
    packet: &[u8],
    source: SocketAddr,
    now: Instant,
) -> Option<(SocketAddr, Vec<u8>)> {
    if !(HEADER_LEN + 16..=MAX_PACKET).contains(&packet.len()) || &packet[..4] != MAGIC {
        return None;
    }
    let token = u32::from_le_bytes(packet[4..8].try_into().ok()?);
    sessions.get(&token).and_then(|session| {
        if !session.plausible_source(packet, source, now) {
            return None;
        }
        let allowed = if packet[8] == DATA {
            admission.accept(token, now)
        } else {
            admission.accept_handshake(source.ip(), token, now)
        };
        if !allowed {
            return None;
        }
        session.process_bounded(packet, source, now, Some(budget), economy)
    })
}

pub async fn run_socket(socket: UdpSocket, state: Arc<ServerState>) -> std::io::Result<()> {
    let _ready = crate::ListenerReady::new(Arc::clone(&state), crate::ListenerReady::RELAY);
    let mut buf = vec![0u8; MAX_PACKET + 1];
    let mut admission = CryptoAdmission::new();
    loop {
        let (len, source) = socket.recv_from(&mut buf).await?;
        if !(HEADER_LEN + 16..=MAX_PACKET).contains(&len) || &buf[..4] != MAGIC {
            continue;
        }
        let forward = dispatch_packet(
            &state.relay_sessions,
            &state.relay_budget,
            Some(&state.relay_economy),
            &mut admission,
            &buf[..len],
            source,
            Instant::now(),
        );
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
    fn maximum_supported_hostile_transports_cannot_spend_a_survivor_reservation() {
        let sessions = dashmap::DashMap::new();
        let budget = Mutex::new(crate::resource_budget::RelayBudget::new());
        let mut admission = CryptoAdmission::new();
        let now = admission.started;
        let good_a = "127.0.0.1:21000".parse().unwrap();
        let good_b = "127.0.0.1:21001".parse().unwrap();
        let survivor = RelaySession::new(
            Uuid::new_v4(),
            [(0, Uuid::new_v4()), (1, Uuid::new_v4())],
            now,
        );
        let mut ga = 0;
        let mut gb = 0;
        bind(&survivor, 999, 0, 1, good_a, &mut ga, now);
        bind(&survivor, 999, 1, 0, good_b, &mut gb, now);
        let key_good = key(&survivor.credentials(0).unwrap().send_key);
        sessions.insert(999, survivor);
        for token in 0..199 {
            let a: SocketAddr = format!("127.0.0.{}:{}", 1 + token % 4, 22000 + token * 2)
                .parse()
                .unwrap();
            let b: SocketAddr = format!("127.0.0.{}:{}", 1 + token % 4, 22001 + token * 2)
                .parse()
                .unwrap();
            let bad = RelaySession::new(
                Uuid::new_v4(),
                [(0, Uuid::new_v4()), (1, Uuid::new_v4())],
                now,
            );
            let mut sa = 0;
            let mut sb = 0;
            bind(&bad, token, 0, 1, a, &mut sa, now);
            bind(&bad, token, 1, 0, b, &mut sb, now);
            sessions.insert(token, bad);
            let forged = seal(&[0; 32], token, DATA, 0, 1, sa + 1, b"forged").unwrap();
            for round in 0..2 {
                for _ in 0..256 {
                    assert!(dispatch_packet(
                        &sessions,
                        &budget,
                        None,
                        &mut admission,
                        &forged,
                        a,
                        now
                    )
                    .is_none());
                }
                if round == 1 {
                    continue;
                }
                ga += 1;
                let valid =
                    seal(&key_good, 999, DATA, 0, 1, ga, b"reserved survivor traffic").unwrap();
                assert!(dispatch_packet(
                    &sessions,
                    &budget,
                    None,
                    &mut admission,
                    &valid,
                    good_a,
                    now
                )
                .is_some());
                // Replays do not consume even the raw work reservation
                let before = admission.packets;
                assert!(dispatch_packet(
                    &sessions,
                    &budget,
                    None,
                    &mut admission,
                    &valid,
                    good_a,
                    now
                )
                .is_none());
                assert_eq!(admission.packets, before);
            }
        }
        assert_eq!(sessions.len(), 200);
        assert_eq!(admission.packets, 199 * 256 + 199);
    }
    #[test]
    fn two_full_rosters_at_forty_packets_per_path_share_one_nat() {
        let sessions = dashmap::DashMap::new();
        let budget = Mutex::new(crate::resource_budget::RelayBudget::new());
        let mut admission = CryptoAdmission::new();
        let now = admission.started;
        let month = crate::relay_economy::Month::current(now);
        let mut economy = crate::relay_economy::RelayEconomy::new(
            crate::relay_economy::RelayPolicy {
                enforce_identity_quotas: true,
                ..Default::default()
            },
            month,
            now,
        );
        let db = crate::db::Database::open(":memory:").unwrap();
        let (period, limit, wanted) = economy.request_credit(month).unwrap();
        economy.finish_credit(db.reserve_relay_egress(period, limit, wanted));
        let economy = Mutex::new(economy);
        let mut paths = Vec::new();
        for room_index in 0..2 {
            let room = Uuid::new_v4();
            let ids: Vec<_> = (0..8).map(|_| Uuid::new_v4()).collect();
            for a in 0..8 {
                for b in a + 1..8 {
                    let token = paths.len() as u32 + 1;
                    let aa: SocketAddr = format!("127.0.0.1:{}", 25000 + room_index * 100 + a)
                        .parse()
                        .unwrap();
                    let bb: SocketAddr = format!("127.0.0.1:{}", 25000 + room_index * 100 + b)
                        .parse()
                        .unwrap();
                    let session =
                        RelaySession::new(room, [(a as u8, ids[a]), (b as u8, ids[b])], now);
                    let mut sa = 0;
                    let mut sb = 0;
                    bind(&session, token, a as u8, b as u8, aa, &mut sa, now);
                    bind(&session, token, b as u8, a as u8, bb, &mut sb, now);
                    paths.push((
                        token,
                        a as u8,
                        b as u8,
                        aa,
                        bb,
                        key(&session.credentials(a as u8).unwrap().send_key),
                        key(&session.credentials(b as u8).unwrap().send_key),
                    ));
                    sessions.insert(token, session);
                }
            }
        }
        for sequence in 3..43 {
            for (token, a, b, aa, bb, ka, kb) in &paths {
                for (from, to, address, key) in [(*a, *b, *aa, ka), (*b, *a, *bb, kb)] {
                    let packet = seal(key, *token, DATA, from, to, sequence, &[0x42; 512]).unwrap();
                    assert!(dispatch_packet(
                        &sessions,
                        &budget,
                        Some(&economy),
                        &mut admission,
                        &packet,
                        address,
                        now
                    )
                    .is_some());
                }
            }
        }
        assert_eq!(admission.packets, 4480);
        let snapshot = economy.lock().unwrap().snapshot(0, now);
        assert_eq!(snapshot.counters.forwarded_packets, 4480);
        assert_eq!(snapshot.tracked_identities, 16);
        assert_eq!(snapshot.counters.over_identity_quota_packets, 0);
    }

    #[test]
    fn authenticated_arbitrary_payload_cost_is_shared_across_pairs() {
        let budget = Mutex::new(crate::resource_budget::RelayBudget::new());
        let now = Instant::now();
        let identity = Uuid::new_v4();
        let room = Uuid::new_v4();
        let a = "127.0.0.1:10001".parse().unwrap();
        let b = "127.0.0.1:10002".parse().unwrap();
        let payload = vec![0x5a; MAX_PAYLOAD];
        let mut forwarded = 0;
        for token in [42, 43] {
            let session = RelaySession::new(room, [(0, identity), (1, Uuid::new_v4())], now);
            let mut sa = 0;
            let mut sb = 0;
            bind(&session, token, 0, 1, a, &mut sa, now);
            bind(&session, token, 1, 0, b, &mut sb, now);
            let ca = session.credentials(0).unwrap();
            let cb = session.credentials(1).unwrap();
            let forged = seal(&[0; 32], token, DATA, 0, 1, sa + 1, &payload).unwrap();
            for _ in 0..32 {
                assert!(session
                    .process_bounded(&forged, a, now, Some(&budget), None)
                    .is_none());
            }
            for _ in 0..250 {
                sa += 1;
                let packet = seal(&key(&ca.send_key), token, DATA, 0, 1, sa, &payload).unwrap();
                if let Some((_, reply)) =
                    session.process_bounded(&packet, a, now, Some(&budget), None)
                {
                    assert_eq!(open(&key(&cb.receive_key), &reply).unwrap(), payload);
                    forwarded += reply.len();
                }
            }
        }
        assert!(forwarded <= 1024 * 1024);
        assert!(forwarded > 1024 * 1024 - MAX_PACKET);
    }

    #[test]
    fn mixed_unknown_and_invalid_pair_traffic_preserves_other_authenticated_pair() {
        let mut admission = CryptoAdmission::new();
        let now = admission.started;
        let budget = Mutex::new(crate::resource_budget::RelayBudget::new());
        let sessions = dashmap::DashMap::new();
        let a = "127.0.0.1:10001".parse().unwrap();
        let b = "127.0.0.1:10002".parse().unwrap();
        for token in [42, 43] {
            sessions.insert(
                token,
                RelaySession::new(
                    Uuid::new_v4(),
                    [(0, Uuid::new_v4()), (1, Uuid::new_v4())],
                    now,
                ),
            );
        }
        let session = sessions.get(&42).unwrap();
        let mut sa = 0;
        let mut sb = 0;
        bind(&session, 42, 0, 1, a, &mut sa, now);
        bind(&session, 42, 1, 0, b, &mut sb, now);
        let ca = session.credentials(0).unwrap();
        let cb = session.credentials(1).unwrap();
        drop(session);
        let unknown = seal(&[0; 32], 99, DATA, 0, 1, 1, b"invalid").unwrap();
        for _ in 0..20000 {
            assert!(
                dispatch_packet(&sessions, &budget, None, &mut admission, &unknown, a, now)
                    .is_none()
            );
        }
        assert_eq!(admission.packets, 0);
        let other = sessions.get(&43).unwrap();
        let mut oa = 0;
        let mut ob = 0;
        bind(&other, 43, 0, 1, a, &mut oa, now);
        bind(&other, 43, 1, 0, b, &mut ob, now);
        drop(other);
        let bind_flood = seal(&[0; 32], 42, BIND, 0, 1, 1, &[0; 16]).unwrap();
        for _ in 0..20000 {
            assert!(dispatch_packet(
                &sessions,
                &budget,
                None,
                &mut admission,
                &bind_flood,
                a,
                now
            )
            .is_none());
        }
        assert_eq!(admission.packets, 0);
        let forged = seal(&[0; 32], 43, DATA, 0, 1, 3, b"invalid").unwrap();
        for _ in 0..20000 {
            assert!(
                dispatch_packet(&sessions, &budget, None, &mut admission, &forged, a, now)
                    .is_none()
            );
        }
        assert_eq!(admission.packets, 256);
        let valid = seal(
            &key(&ca.send_key),
            42,
            DATA,
            0,
            1,
            sa + 1,
            b"survives mixed traffic",
        )
        .unwrap();
        let (address, reply) =
            dispatch_packet(&sessions, &budget, None, &mut admission, &valid, a, now).unwrap();
        assert_eq!(address, b);
        assert_eq!(
            open(&key(&cb.receive_key), &reply).unwrap(),
            b"survives mixed traffic"
        );
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

    #[test]
    fn forged_sender_does_not_spend_authenticated_peer_budget() {
        let now = Instant::now();
        let session = RelaySession::new(
            Uuid::new_v4(),
            [(0, Uuid::new_v4()), (2, Uuid::new_v4())],
            now,
        );
        let a = "127.0.0.1:10001".parse().unwrap();
        let b = "127.0.0.1:10002".parse().unwrap();
        let mut sa = 0;
        let mut sb = 0;
        bind(&session, 42, 0, 2, a, &mut sa, now);
        bind(&session, 42, 2, 0, b, &mut sb, now);
        let cb = session.credentials(2).unwrap();
        let valid = seal(
            &key(&cb.send_key),
            42,
            DATA,
            2,
            0,
            sb + 1,
            b"still admitted",
        )
        .unwrap();
        let mut forged = valid.clone();
        *forged.last_mut().unwrap() ^= 1;
        for _ in 0..300 {
            assert!(session.process(&forged, a, now).is_none());
        }
        assert!(session.process(&valid, b, now).is_some());
    }

    #[test]
    fn every_supported_pair_has_a_bounded_crypto_reservation() {
        let mut budget = CryptoAdmission::new();
        let now = budget.started;
        for token in 0..200 {
            for _ in 0..256 {
                assert!(budget.accept(token, now));
            }
        }
        assert_eq!(budget.packets, 51_200);
        assert_eq!(budget.pairs.len(), 200);
        assert!(!budget.accept(200, now));
        assert!(!budget.accept(0, now));
        assert!(budget.accept(100, now + Duration::from_secs(1)));
        assert_eq!(budget.pairs.len(), 1);
    }

    #[test]
    fn active_keepalives_cannot_refresh_absolute_expiry_and_retirement_never_extends_it() {
        let now = Instant::now();
        let session = RelaySession::new(
            Uuid::new_v4(),
            [(0, Uuid::new_v4()), (1, Uuid::new_v4())],
            now,
        );
        let a = "127.0.0.1:20001".parse().unwrap();
        let b = "127.0.0.1:20002".parse().unwrap();
        let mut sa = 0;
        let mut sb = 0;
        bind(&session, 42, 0, 1, a, &mut sa, now);
        bind(&session, 42, 1, 0, b, &mut sb, now);
        let credential = session.credentials(0).unwrap();
        let keepalive = seal(&key(&credential.send_key), 42, BIND, 0, 1, sa + 1, &[0; 16]).unwrap();
        assert!(session
            .process(&keepalive, a, now + Duration::from_secs(7199))
            .is_some());
        let data = seal(
            &key(&credential.send_key),
            42,
            DATA,
            0,
            1,
            sa + 2,
            b"expired",
        )
        .unwrap();
        assert!(session
            .process(&data, a, now + Duration::from_secs(7200))
            .is_none());
        session.retire_at(now + Duration::from_secs(5));
        session.retire_at(now + Duration::from_secs(500));
        assert_eq!(session.expires_at(), now + Duration::from_secs(5));
    }

    #[test]
    fn consent_requires_periodic_return_path_proof_and_tolerates_same_route_proof_reordering() {
        let now = Instant::now();
        let session = RelaySession::new(
            Uuid::new_v4(),
            [(0, Uuid::new_v4()), (1, Uuid::new_v4())],
            now,
        );
        let a = "127.0.0.1:20001".parse().unwrap();
        let b = "127.0.0.1:20002".parse().unwrap();
        let mut sa = 0;
        let mut sb = 0;
        bind(&session, 42, 0, 1, a, &mut sa, now);
        bind(&session, 42, 1, 0, b, &mut sb, now);
        let ca = session.credentials(0).unwrap();
        let cb = session.credentials(1).unwrap();
        let request = seal(&key(&ca.send_key), 42, BIND, 0, 1, 3, &[0; 16]).unwrap();
        let (_, challenge) = session.process(&request, a, now + CONSENT_REFRESH).unwrap();
        assert_eq!(challenge[8], CHALLENGE);
        let receipt = open(&key(&ca.receive_key), &challenge).unwrap();
        let proof = seal(&key(&ca.send_key), 42, PROOF, 0, 1, 4, &receipt).unwrap();
        let data = seal(
            &key(&ca.send_key),
            42,
            DATA,
            0,
            1,
            5,
            b"game while proof is in flight",
        )
        .unwrap();
        assert!(session.process(&data, a, now + CONSENT_REFRESH).is_some());
        assert!(session.process(&proof, a, now + CONSENT_REFRESH).is_some());
        assert!(session.process(&proof, a, now + CONSENT_REFRESH).is_none());
        let data = seal(
            &key(&ca.send_key),
            42,
            DATA,
            0,
            1,
            6,
            b"expired destination",
        )
        .unwrap();
        assert!(session.process(&data, a, now + CONSENT_LIFETIME).is_none());
        bind(
            &session,
            42,
            1,
            0,
            b,
            &mut sb,
            now + Duration::from_secs(31),
        );
        let data = seal(&key(&ca.send_key), 42, DATA, 0, 1, 7, b"fresh destination").unwrap();
        let (_, reply) = session
            .process(&data, a, now + Duration::from_secs(31))
            .unwrap();
        assert_eq!(
            open(&key(&cb.receive_key), &reply).unwrap(),
            b"fresh destination"
        );
        let data = seal(&key(&ca.send_key), 42, DATA, 0, 1, 8, b"expired sender").unwrap();
        assert!(session
            .process(&data, a, now + Duration::from_secs(45))
            .is_none());
        let request = seal(&key(&ca.send_key), 42, BIND, 0, 1, 9, &[0; 16]).unwrap();
        assert_eq!(
            session
                .process(&request, a, now + Duration::from_secs(46))
                .unwrap()
                .1[8],
            CHALLENGE
        );
        assert_eq!(
            session.inner.lock().unwrap().last_activity,
            now + Duration::from_secs(31)
        );
    }
}
