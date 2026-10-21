use std::collections::{HashMap, VecDeque};
use std::net::IpAddr;
use std::sync::Mutex;
use std::time::Instant;

use uuid::Uuid;

struct WindowEntries<K> {
    keys: HashMap<K, VecDeque<Instant>>,
    next_cleanup: Instant,
    #[cfg(test)]
    cleanup_scans: usize,
}
impl<K: Eq + std::hash::Hash> WindowEntries<K> {
    fn new() -> Self {
        Self {
            keys: HashMap::new(),
            next_cleanup: Instant::now(),
            #[cfg(test)]
            cleanup_scans: 0,
        }
    }
    fn prune(&mut self, now: Instant, window: std::time::Duration) {
        self.keys.retain(|_, events| {
            events
                .back()
                .is_some_and(|last| now.saturating_duration_since(*last) < window)
        });
        self.next_cleanup = self
            .keys
            .values()
            .filter_map(|events| events.back().map(|last| *last + window))
            .min()
            .unwrap_or(now + window);
        #[cfg(test)]
        {
            self.cleanup_scans += 1;
        }
    }
}

/// Sliding-window rate limiter.
///
/// Limits are enforced per key (IP address or player ID). Each key gets
/// a deque of timestamps. On check, expired entries are pruned and the
/// current count is compared against the limit.
pub struct RateLimiter {
    /// Per-IP connection attempts: max 64 per 60s (two full rooms plus two reconnect waves)
    ip_connections: Mutex<WindowEntries<IpAddr>>,
    /// Per-IP auth failures: max 5 per 3600s
    ip_auth_fails: Mutex<WindowEntries<IpAddr>>,
    /// Per-player lobby creation: max 1 per 10s
    lobby_creates: Mutex<WindowEntries<Uuid>>,
    /// Per-player lobby joins: max 5 per 60s
    lobby_joins: Mutex<WindowEntries<Uuid>>,
    /// Per-player callsign searches: max 5 per 60s
    callsign_searches: Mutex<WindowEntries<Uuid>>,
    /// Per-connection message rate: max 30 per 1s
    ws_messages: Mutex<WindowEntries<Uuid>>,
    /// Per-player direct messages: max 5 per 60s
    player_messages: Mutex<WindowEntries<Uuid>>,
    /// Per-session lobby list requests: max 5 per 10s
    lobby_lists: Mutex<WindowEntries<Uuid>>,
}

impl Default for RateLimiter {
    fn default() -> Self {
        Self::new()
    }
}

impl RateLimiter {
    pub fn new() -> Self {
        Self {
            ip_connections: Mutex::new(WindowEntries::new()),
            ip_auth_fails: Mutex::new(WindowEntries::new()),
            lobby_creates: Mutex::new(WindowEntries::new()),
            lobby_joins: Mutex::new(WindowEntries::new()),
            callsign_searches: Mutex::new(WindowEntries::new()),
            ws_messages: Mutex::new(WindowEntries::new()),
            player_messages: Mutex::new(WindowEntries::new()),
            lobby_lists: Mutex::new(WindowEntries::new()),
        }
    }

    pub fn check_ip_connection(&self, ip: IpAddr) -> bool {
        Self::check(&self.ip_connections, ip, 64, 60)
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

    pub fn check_lobby_list(&self, session_id: Uuid) -> bool {
        Self::check(&self.lobby_lists, session_id, 5, 10)
    }

    pub fn release_connection(&self, connection: Uuid) {
        self.ws_messages.lock().unwrap().keys.remove(&connection);
    }

    fn check<K: Eq + std::hash::Hash>(
        map: &Mutex<WindowEntries<K>>,
        key: K,
        max_count: usize,
        window_secs: u64,
    ) -> bool {
        Self::check_at(map, key, max_count, window_secs, Instant::now())
    }
    fn check_at<K: Eq + std::hash::Hash>(
        map: &Mutex<WindowEntries<K>>,
        key: K,
        max_count: usize,
        window_secs: u64,
        now: Instant,
    ) -> bool {
        let window = std::time::Duration::from_secs(window_secs);
        let mut table = map.lock().unwrap();
        if table.keys.len() >= 4096 && !table.keys.contains_key(&key) {
            if now < table.next_cleanup {
                return false;
            }
            table.prune(now, window);
            if table.keys.len() >= 4096 {
                return false;
            }
        }
        let events = table.keys.entry(key).or_default();
        while events
            .front()
            .is_some_and(|first| now.saturating_duration_since(*first) >= window)
        {
            events.pop_front();
        }
        if events.len() >= max_count {
            return false;
        }
        events.push_back(now);
        true
    }

    pub fn cleanup(&self) {
        let now = Instant::now();
        Self::cleanup_map(&self.ip_connections, now, 60);
        Self::cleanup_map(&self.ip_auth_fails, now, 3600);
        Self::cleanup_map(&self.lobby_creates, now, 10);
        Self::cleanup_map(&self.lobby_joins, now, 60);
        Self::cleanup_map(&self.callsign_searches, now, 60);
        Self::cleanup_map(&self.ws_messages, now, 1);
        Self::cleanup_map(&self.player_messages, now, 60);
        Self::cleanup_map(&self.lobby_lists, now, 10);
    }
    fn cleanup_map<K: Eq + std::hash::Hash>(
        map: &Mutex<WindowEntries<K>>,
        now: Instant,
        seconds: u64,
    ) {
        let window = std::time::Duration::from_secs(seconds);
        map.lock().unwrap().prune(now, window);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn closed_connection_churn_releases_only_connection_owned_limits() {
        let limiter = RateLimiter::new();
        let now = Instant::now();
        let identity = Uuid::new_v4();
        for _ in 0..5000 {
            let connection = Uuid::new_v4();
            assert!(RateLimiter::check_at(
                &limiter.ws_messages,
                connection,
                30,
                1,
                now
            ));
            limiter.release_connection(connection);
        }
        assert!(limiter.ws_messages.lock().unwrap().keys.is_empty());
        for _ in 0..5 {
            assert!(RateLimiter::check_at(
                &limiter.lobby_joins,
                identity,
                5,
                60,
                now
            ));
        }
        limiter.release_connection(identity);
        assert!(!RateLimiter::check_at(
            &limiter.lobby_joins,
            identity,
            5,
            60,
            now
        ));
    }
    #[test]
    fn adversarial_keys_are_bounded_and_recover_at_their_own_window() {
        for seconds in [1, 10, 60, 3600] {
            let map = Mutex::new(WindowEntries::new());
            let now = Instant::now();
            for _ in 0..4096 {
                assert!(RateLimiter::check_at(&map, Uuid::new_v4(), 1, seconds, now));
            }
            assert!(!RateLimiter::check_at(
                &map,
                Uuid::new_v4(),
                1,
                seconds,
                now
            ));
            assert_eq!(map.lock().unwrap().keys.len(), 4096);
            for _ in 0..1000 {
                assert!(!RateLimiter::check_at(
                    &map,
                    Uuid::new_v4(),
                    1,
                    seconds,
                    now
                ));
            }
            assert_eq!(map.lock().unwrap().cleanup_scans, 1);
            assert!(RateLimiter::check_at(
                &map,
                Uuid::new_v4(),
                1,
                seconds,
                now + std::time::Duration::from_secs(seconds)
            ));
            assert_eq!(map.lock().unwrap().keys.len(), 1);
        }
    }
}
