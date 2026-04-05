use std::net::{IpAddr, SocketAddr};
use std::sync::Arc;

use dashmap::{DashMap, DashSet};
use tokio::net::UdpSocket;
use tracing::{debug, warn};

use crate::ServerState;

/// Maximum age of a relay session before it is reaped.
const MAX_RELAY_SESSION_SECS: u64 = 7200; // 2 hours

/// Active relay session mapping player slots to their UDP addresses.
pub struct RelaySession {
    pub session_token: u32,
    pub player_addrs: DashMap<u8, SocketAddr>,
    /// Number of expected players in this session (for address learning).
    pub expected_players: u8,
    /// D12: IPs allowed to participate (pre-registered from WS peer addresses).
    pub allowed_ips: DashSet<IpAddr>,
    pub created_at: std::time::Instant,
    /// Packet counter for diagnostic logging (first N packets logged at info).
    pub packets_forwarded: std::sync::atomic::AtomicU64,
}

/// Remove relay sessions older than `MAX_RELAY_SESSION_SECS`.
/// Returns the number of sessions removed.
pub fn cleanup_stale_sessions(state: &ServerState) -> usize {
    cleanup_sessions_older_than(
        state,
        std::time::Duration::from_secs(MAX_RELAY_SESSION_SECS),
    )
}

/// Remove relay sessions older than `cutoff`.
/// Returns the number of sessions removed.
pub fn cleanup_sessions_older_than(state: &ServerState, cutoff: std::time::Duration) -> usize {
    let now = std::time::Instant::now();
    let mut removed = 0usize;
    state.relay_sessions.retain(|_token, session| {
        let keep = now.duration_since(session.created_at) < cutoff;
        if !keep {
            removed += 1;
        }
        keep
    });
    if removed > 0 {
        tracing::info!(
            removed,
            remaining = state.relay_sessions.len(),
            "reaped stale relay sessions"
        );
    }
    removed
}

/// Relay packet format:
///   [session_token: 4 bytes LE][dest_player: 1 byte][payload...]
/// Forwarded as:
///   [session_token: 4 bytes LE][from_player: 1 byte][payload...]
const RELAY_HEADER_LEN: usize = 5;

/// Run the UDP relay listener.
pub async fn run(
    addr: SocketAddr,
    state: Arc<ServerState>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let socket = UdpSocket::bind(addr).await?;
    tracing::info!(%addr, "UDP relay listening");

    let mut buf = [0u8; 2048];
    loop {
        let (len, src) = match socket.recv_from(&mut buf).await {
            Ok(v) => v,
            Err(e) => {
                tracing::warn!(%e, "relay recv_from error, continuing");
                continue;
            }
        };
        if len < RELAY_HEADER_LEN {
            debug!(%src, len, "relay packet too short, dropping");
            continue;
        }

        let token = u32::from_le_bytes([buf[0], buf[1], buf[2], buf[3]]);
        let dest_player = buf[4];

        let Some(session) = state.relay_sessions.get(&token) else {
            // Log at info for first unknown token to aid debugging
            debug!(%src, token, "unknown relay session token");
            continue;
        };

        // Identify the sender by their source address
        let from_player = session
            .player_addrs
            .iter()
            .find(|entry| *entry.value() == src)
            .map(|entry| *entry.key());

        let from_player = match from_player {
            Some(p) => p,
            None => {
                // Check if this is a NAT rebind: a known slot sending from a
                // new source address. In a 2-player session, the sender must
                // be the slot that ISN'T dest_player.
                let candidate_slot = (0..session.expected_players)
                    .find(|s| *s != dest_player && session.player_addrs.contains_key(s));
                if let Some(slot) = candidate_slot {
                    if !session.allowed_ips.is_empty() && !session.allowed_ips.contains(&src.ip()) {
                        warn!(%src, token, "relay address update rejected: IP not allowed");
                        continue;
                    }
                    let old_addr = session.player_addrs.insert(slot, src);
                    tracing::info!(%src, ?old_addr, slot, token, "relay: updated address for slot (NAT rebind)");
                    slot
                } else {
                    // Address learning: if there are unregistered slots, try to
                    // infer the sender.
                    let registered: std::collections::HashSet<u8> =
                        session.player_addrs.iter().map(|e| *e.key()).collect();
                    let free_slot = (0..session.expected_players)
                        .find(|s| !registered.contains(s) && *s != dest_player);
                    if let Some(slot) = free_slot {
                        // D12: only learn addresses from allowed IPs
                        if !session.allowed_ips.is_empty() && !session.allowed_ips.contains(&src.ip()) {
                            warn!(%src, token, "relay address learning rejected: IP not allowed");
                            continue;
                        }
                        session.player_addrs.insert(slot, src);
                        tracing::info!(%src, slot, token, "relay: learned address for slot");
                        slot
                    } else {
                        warn!(%src, token, "relay packet from unregistered address");
                        continue;
                    }
                }
            }
        };

        // Look up destination address
        let Some(dest_addr) = session.player_addrs.get(&dest_player).map(|r| *r.value()) else {
            debug!(token, dest_player, from_player, "relay dest slot not yet known, dropping");
            continue;
        };

        // Build forwarded packet: same header but replace dest with from
        let mut fwd = Vec::with_capacity(len);
        fwd.extend_from_slice(&buf[0..4]); // session_token
        fwd.push(from_player); // from_player (replaces dest)
        fwd.extend_from_slice(&buf[RELAY_HEADER_LEN..len]); // payload

        let count = session
            .packets_forwarded
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        if count < 5 || (count < 100 && count % 20 == 0) || count % 500 == 0 {
            tracing::info!(
                token, from_player, dest_player, %src, %dest_addr,
                len, count, "relay: forwarded packet"
            );
        }

        if let Err(e) = socket.send_to(&fwd, dest_addr).await {
            tracing::info!(%e, %dest_addr, from_player, dest_player, "relay forward failed");
        }
    }
}
