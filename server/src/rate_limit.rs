use std::collections::VecDeque;
use std::net::IpAddr;
use std::time::Instant;

use dashmap::DashMap;
use uuid::Uuid;

/// Sliding-window rate limiter.
///
/// Limits are enforced per key (IP address or player ID). Each key gets
/// a deque of timestamps. On check, expired entries are pruned and the
/// current count is compared against the limit.
pub struct RateLimiter {
    /// Per-IP connection attempts: max 3 per 60s
    ip_connections: DashMap<IpAddr, VecDeque<Instant>>,
    /// Per-IP auth failures: max 5 per 3600s
    ip_auth_fails: DashMap<IpAddr, VecDeque<Instant>>,
    /// Per-player lobby creation: max 1 per 10s
    lobby_creates: DashMap<Uuid, VecDeque<Instant>>,
    /// Per-player lobby joins: max 5 per 60s
    lobby_joins: DashMap<Uuid, VecDeque<Instant>>,
    /// Per-player callsign searches: max 5 per 60s
    callsign_searches: DashMap<Uuid, VecDeque<Instant>>,
    /// Per-connection message rate: max 30 per 1s
    ws_messages: DashMap<Uuid, VecDeque<Instant>>,
    /// Per-player direct messages: max 5 per 60s
    player_messages: DashMap<Uuid, VecDeque<Instant>>,
}

impl Default for RateLimiter {
    fn default() -> Self {
        Self::new()
    }
}

impl RateLimiter {
    pub fn new() -> Self {
        Self {
            ip_connections: DashMap::new(),
            ip_auth_fails: DashMap::new(),
            lobby_creates: DashMap::new(),
            lobby_joins: DashMap::new(),
            callsign_searches: DashMap::new(),
            ws_messages: DashMap::new(),
            player_messages: DashMap::new(),
        }
    }

    pub fn check_ip_connection(&self, ip: IpAddr) -> bool {
        Self::check(&self.ip_connections, ip, 3, 60)
    }

    pub fn record_auth_fail(&self, ip: IpAddr) -> bool {
        Self::check(&self.ip_auth_fails, ip, 5, 3600)
    }

    pub fn check_lobby_create(&self, player_id: Uuid) -> bool {
        Self::check(&self.lobby_creates, player_id, 1, 10)
    }

    pub fn check_lobby_join(&self, player_id: Uuid) -> bool {
        Self::check(&self.lobby_joins, player_id, 5, 60)
    }

    pub fn check_callsign_search(&self, player_id: Uuid) -> bool {
        Self::check(&self.callsign_searches, player_id, 5, 60)
    }

    pub fn check_ws_message(&self, session_id: Uuid) -> bool {
        Self::check(&self.ws_messages, session_id, 30, 1)
    }

    pub fn check_player_message(&self, player_id: Uuid) -> bool {
        Self::check(&self.player_messages, player_id, 5, 60)
    }

    /// Returns true if the action is allowed, false if rate-limited.
    fn check<K: Eq + std::hash::Hash>(
        map: &DashMap<K, VecDeque<Instant>>,
        key: K,
        max_count: usize,
        window_secs: u64,
    ) -> bool {
        let now = Instant::now();
        let window = std::time::Duration::from_secs(window_secs);
        let mut entry = map.entry(key).or_default();
        let deque = entry.value_mut();

        // Prune expired entries
        while deque
            .front()
            .is_some_and(|t| now.duration_since(*t) > window)
        {
            deque.pop_front();
        }

        if deque.len() >= max_count {
            false
        } else {
            deque.push_back(now);
            true
        }
    }

    /// Periodically clean up stale entries to prevent unbounded memory growth.
    pub fn cleanup(&self) {
        let now = Instant::now();
        let max_age = std::time::Duration::from_secs(3600);
        Self::cleanup_map(&self.ip_connections, now, max_age);
        Self::cleanup_map(&self.ip_auth_fails, now, max_age);
        Self::cleanup_map(&self.lobby_creates, now, max_age);
        Self::cleanup_map(&self.lobby_joins, now, max_age);
        Self::cleanup_map(&self.callsign_searches, now, max_age);
        Self::cleanup_map(&self.ws_messages, now, max_age);
        Self::cleanup_map(&self.player_messages, now, max_age);
    }

    fn cleanup_map<K: Eq + std::hash::Hash + Clone>(
        map: &DashMap<K, VecDeque<Instant>>,
        now: Instant,
        max_age: std::time::Duration,
    ) {
        let stale_keys: Vec<K> = map
            .iter()
            .filter(|entry| {
                entry.value().back().is_none()
                    || now.duration_since(*entry.value().back().unwrap()) > max_age
            })
            .map(|entry| entry.key().clone())
            .collect();
        for key in stale_keys {
            map.remove(&key);
        }
    }
}
