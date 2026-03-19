use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;
use std::time::Duration;

use tracing::info;

use crate::ServerState;

/// Atomic counters for real-time server stats.
pub struct ServerStats {
    pub current_online: AtomicU32,
    pub current_in_game: AtomicU32,
    pub current_lobbies: AtomicU32,
    pub peak_online_today: AtomicU32,
    pub peak_online_alltime: AtomicU32,
}

impl Default for ServerStats {
    fn default() -> Self {
        Self::new()
    }
}

impl ServerStats {
    pub fn new() -> Self {
        Self {
            current_online: AtomicU32::new(0),
            current_in_game: AtomicU32::new(0),
            current_lobbies: AtomicU32::new(0),
            peak_online_today: AtomicU32::new(0),
            peak_online_alltime: AtomicU32::new(0),
        }
    }

    pub fn player_connected(&self) {
        let online = self.current_online.fetch_add(1, Ordering::Relaxed) + 1;
        self.peak_online_today.fetch_max(online, Ordering::Relaxed);
        self.peak_online_alltime
            .fetch_max(online, Ordering::Relaxed);
    }

    pub fn player_disconnected(&self) {
        self.current_online.fetch_sub(1, Ordering::Relaxed);
    }

    pub fn game_started(&self) {
        self.current_in_game.fetch_add(1, Ordering::Relaxed);
    }

    pub fn game_ended(&self) {
        self.current_in_game.fetch_sub(1, Ordering::Relaxed);
    }

    pub fn lobby_created(&self) {
        self.current_lobbies.fetch_add(1, Ordering::Relaxed);
    }

    pub fn lobby_closed(&self) {
        self.current_lobbies.fetch_sub(1, Ordering::Relaxed);
    }

    pub fn snapshot(&self) -> StatsSnapshot {
        StatsSnapshot {
            online: self.current_online.load(Ordering::Relaxed),
            in_game: self.current_in_game.load(Ordering::Relaxed),
            lobbies: self.current_lobbies.load(Ordering::Relaxed),
            peak_today: self.peak_online_today.load(Ordering::Relaxed),
            peak_alltime: self.peak_online_alltime.load(Ordering::Relaxed),
        }
    }

    /// Reset daily peak (call at midnight UTC).
    pub fn reset_daily_peak(&self) {
        self.peak_online_today.store(
            self.current_online.load(Ordering::Relaxed),
            Ordering::Relaxed,
        );
    }
}

#[derive(Clone)]
pub struct StatsSnapshot {
    pub online: u32,
    pub in_game: u32,
    pub lobbies: u32,
    pub peak_today: u32,
    pub peak_alltime: u32,
}

/// Periodic background tasks: stats snapshots, rate limiter cleanup, relay session reaping.
pub async fn periodic_tasks(state: Arc<ServerState>) {
    let mut snapshot_interval = tokio::time::interval(Duration::from_secs(300));
    let mut cleanup_interval = tokio::time::interval(Duration::from_secs(600));
    let mut relay_cleanup_interval = tokio::time::interval(Duration::from_secs(300));
    let mut lobby_cleanup_interval = tokio::time::interval(Duration::from_secs(60));

    loop {
        tokio::select! {
            _ = snapshot_interval.tick() => {
                let snap = state.stats.snapshot();
                info!(
                    online = snap.online,
                    in_game = snap.in_game,
                    lobbies = snap.lobbies,
                    "periodic stats snapshot"
                );
                let _ = state.db.insert_player_count_snapshot(
                    snap.online, snap.in_game, snap.lobbies,
                );
            }
            _ = cleanup_interval.tick() => {
                state.rate_limiter.cleanup();
            }
            _ = relay_cleanup_interval.tick() => {
                crate::relay::cleanup_stale_sessions(&state);
            }
            _ = lobby_cleanup_interval.tick() => {
                cleanup_stale_lobbies(&state);
            }
        }
    }
}

/// Reap lobbies stuck in non-Waiting states (D5) and revert Holepunching timeouts (D6).
fn cleanup_stale_lobbies(state: &Arc<ServerState>) {
    use crate::lobby::LobbyState;
    let now = std::time::Instant::now();
    let max_lobby_age = Duration::from_secs(4 * 3600); // 4 hours
    let holepunch_timeout = Duration::from_secs(30);

    let mut to_remove = Vec::new();
    let mut to_revert = Vec::new();

    for entry in state.lobbies.iter() {
        let lobby = entry.value();
        let age = now.duration_since(lobby.created_at_instant);
        // D6: revert Holepunching to Waiting after 30s
        if lobby.state == LobbyState::Holepunching {
            if let Some(started) = lobby.holepunch_started_at {
                if now.duration_since(started) > holepunch_timeout {
                    to_revert.push(lobby.id);
                }
            }
        }
        // D5: reap lobbies stuck in Starting/InGame for 4+ hours
        if (lobby.state == LobbyState::Starting || lobby.state == LobbyState::InGame)
            && age > max_lobby_age
        {
            to_remove.push(lobby.id);
        }
    }

    for lid in to_revert {
        if let Some(mut lobby) = state.lobbies.get_mut(&lid) {
            if lobby.state == LobbyState::Holepunching {
                lobby.state = LobbyState::Waiting;
                lobby.holepunch_started_at = None;
                info!(lobby_id = %lid, "holepunching timed out, reverted to Waiting");
            }
        }
    }

    for lid in to_remove {
        // Notify players before removing
        if let Some(lobby) = state.lobbies.get(&lid) {
            for p in lobby.players.iter() {
                if let Some(sess) = state.sessions.get(&p.player_id) {
                    let _ = sess.tx.send(crate::protocol::ServerMessage::Error {
                        code: "LOBBY_EXPIRED".into(),
                        message: "Lobby expired due to inactivity".into(),
                    });
                }
                if let Some(mut s) = state.sessions.get_mut(&p.player_id) {
                    s.lobby_id = None;
                    s.presence = crate::lobby::Presence::Online;
                }
            }
        }
        state.lobbies.remove(&lid);
        state.stats.lobby_closed();
        info!(lobby_id = %lid, "reaped stale lobby");
    }
}
