//! Finite local service budgets; deployment capacity still needs measurement
use std::collections::HashMap;
use std::time::{Duration, Instant};
use uuid::Uuid;

#[derive(Clone, Copy, Default)]
struct Cost {
    packets: usize,
    bytes: usize,
}
impl Cost {
    fn fits(self, bytes: usize, packets: usize, byte_limit: usize) -> bool {
        self.packets < packets && bytes <= byte_limit.saturating_sub(self.bytes)
    }
    fn add(&mut self, bytes: usize) {
        self.packets += 1;
        self.bytes += bytes;
    }
}

pub struct RelayBudget {
    start: Instant,
    global: Cost,
    identities: HashMap<Uuid, Cost>,
    rooms: HashMap<Uuid, Cost>,
}
impl RelayBudget {
    pub fn new() -> Self {
        Self {
            start: Instant::now(),
            global: Cost::default(),
            identities: HashMap::new(),
            rooms: HashMap::new(),
        }
    }
    /// Charge authenticated input, including framing, before encrypting any reply
    pub fn accept(&mut self, identity: Uuid, room: Uuid, bytes: usize, now: Instant) -> bool {
        if now.duration_since(self.start) >= Duration::from_secs(1) {
            self.start = now;
            self.global = Cost::default();
            self.identities.clear();
            self.rooms.clear();
        }
        let player = self.identities.get(&identity).copied().unwrap_or_default();
        let group = self.rooms.get(&room).copied().unwrap_or_default();
        if (!self.identities.contains_key(&identity) && self.identities.len() >= 1024)
            || (!self.rooms.contains_key(&room) && self.rooms.len() >= 1024)
            || !player.fits(bytes, 1024, 1024 * 1024)
            || !group.fits(bytes, 4096, 4 * 1024 * 1024)
            || !self.global.fits(bytes, 16384, 8 * 1024 * 1024)
        {
            return false;
        }
        self.global.add(bytes);
        self.identities.entry(identity).or_default().add(bytes);
        self.rooms.entry(room).or_default().add(bytes);
        true
    }
}
impl Default for RelayBudget {
    fn default() -> Self {
        Self::new()
    }
}

/// Fixed window with bounded key storage; exhaustion rejects work instead of queuing
pub struct WorkBudget {
    start: Instant,
    interval: Duration,
    count: usize,
    limit: usize,
}
impl WorkBudget {
    pub fn new(limit: usize, seconds: u64) -> Self {
        Self {
            start: Instant::now(),
            interval: Duration::from_secs(seconds),
            count: 0,
            limit,
        }
    }
    pub fn accept(&mut self, now: Instant) -> bool {
        if now.duration_since(self.start) >= self.interval {
            self.start = now;
            self.count = 0;
        }
        if self.count >= self.limit {
            return false;
        }
        self.count += 1;
        true
    }
}

pub struct AllocationBudget {
    start: Instant,
    count: usize,
    identities: HashMap<Uuid, usize>,
    rooms: HashMap<Uuid, usize>,
}
impl AllocationBudget {
    pub fn new() -> Self {
        Self {
            start: Instant::now(),
            count: 0,
            identities: HashMap::new(),
            rooms: HashMap::new(),
        }
    }
    pub fn accept(&mut self, a: Uuid, b: Uuid, room: Uuid, now: Instant) -> bool {
        if now.duration_since(self.start) >= Duration::from_secs(60) {
            self.start = now;
            self.count = 0;
            self.identities.clear();
            self.rooms.clear();
        }
        if self.count >= 256
            || self.identities.get(&a).copied().unwrap_or(0) >= 28
            || self.identities.get(&b).copied().unwrap_or(0) >= 28
            || self.rooms.get(&room).copied().unwrap_or(0) >= 112
        {
            return false;
        }
        self.count += 1;
        *self.identities.entry(a).or_default() += 1;
        *self.identities.entry(b).or_default() += 1;
        *self.rooms.entry(room).or_default() += 1;
        true
    }
}
impl Default for AllocationBudget {
    fn default() -> Self {
        Self::new()
    }
}

/// Bounded per-listener work; source churn cannot grow the map beyond global quota
pub struct SourceBudget {
    start: Instant,
    global: usize,
    sources: HashMap<std::net::IpAddr, usize>,
    limit: usize,
    source_limit: usize,
}
impl SourceBudget {
    pub fn new(limit: usize, source_limit: usize) -> Self {
        Self {
            start: Instant::now(),
            global: 0,
            sources: HashMap::new(),
            limit,
            source_limit,
        }
    }
    pub fn accept(&mut self, source: std::net::IpAddr, now: Instant) -> bool {
        if now.duration_since(self.start) >= Duration::from_secs(1) {
            self.start = now;
            self.global = 0;
            self.sources.clear();
        }
        if self.global >= self.limit {
            return false;
        }
        let count = self.sources.entry(source).or_default();
        if *count >= self.source_limit {
            return false;
        }
        *count += 1;
        self.global += 1;
        true
    }
}

/// FIFO byte grants: each connection has at most one waiting writer
pub struct FairEgress {
    window: tokio::sync::Mutex<(Instant, usize)>,
    bytes: usize,
    interval: Duration,
}
impl FairEgress {
    pub fn new(bytes: usize, interval: Duration) -> Self {
        Self {
            window: tokio::sync::Mutex::new((Instant::now(), 0)),
            bytes,
            interval,
        }
    }
    pub async fn wait(&self, bytes: usize) {
        assert!(bytes <= self.bytes);
        let mut window = self.window.lock().await;
        let now = Instant::now();
        if now.duration_since(window.0) >= self.interval {
            *window = (now, 0);
        }
        if bytes > self.bytes - window.1 {
            tokio::time::sleep(self.interval.saturating_sub(window.0.elapsed())).await;
            *window = (Instant::now(), 0);
        }
        window.1 += bytes;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[tokio::test]
    async fn noisy_writer_cannot_starve_an_unrelated_control_response() {
        let budget = std::sync::Arc::new(FairEgress::new(1024, Duration::from_millis(10)));
        let other = budget.clone();
        let (started, ready) = tokio::sync::oneshot::channel();
        let noisy = tokio::spawn(async move {
            other.wait(1024).await;
            started.send(()).unwrap();
            for _ in 0..100 {
                other.wait(1024).await;
            }
        });
        ready.await.unwrap();
        tokio::time::timeout(Duration::from_millis(100), budget.wait(100))
            .await
            .unwrap();
        noisy.abort();
        let _ = noisy.await;
    }
    #[test]
    fn pair_allocation_limits_allow_initial_roster_and_three_recoveries() {
        let mut budget = AllocationBudget::new();
        let now = budget.start;
        let room = Uuid::new_v4();
        let ids: Vec<_> = (0..8).map(|_| Uuid::new_v4()).collect();
        for _ in 0..4 {
            for a in 0..8 {
                for b in a + 1..8 {
                    assert!(budget.accept(ids[a], ids[b], room, now));
                }
            }
        }
        assert!(!budget.accept(ids[0], Uuid::new_v4(), Uuid::new_v4(), now));
        assert!(!budget.accept(Uuid::new_v4(), Uuid::new_v4(), room, now));
        for _ in 112..256 {
            assert!(budget.accept(Uuid::new_v4(), Uuid::new_v4(), Uuid::new_v4(), now));
        }
        assert!(!budget.accept(Uuid::new_v4(), Uuid::new_v4(), Uuid::new_v4(), now));
        assert!(budget.accept(ids[0], ids[1], room, now + Duration::from_secs(60)));
    }
    #[test]
    fn source_work_and_global_work_recover_without_unbounded_keys() {
        let mut source = SourceBudget::new(512, 32);
        let now = source.start;
        let ip = "127.0.0.1".parse().unwrap();
        for _ in 0..32 {
            assert!(source.accept(ip, now));
        }
        assert!(!source.accept(ip, now));
        for i in 1..481 {
            assert!(source.accept(std::net::IpAddr::V4(std::net::Ipv4Addr::from(i)), now));
        }
        assert!(!source.accept("127.0.0.2".parse().unwrap(), now));
        assert!(source.sources.len() <= 512);
        assert!(source.accept(ip, now + Duration::from_secs(1)));
        let mut work = WorkBudget::new(64, 1);
        let now = work.start;
        for _ in 0..64 {
            assert!(work.accept(now));
        }
        assert!(!work.accept(now));
        assert!(work.accept(now + Duration::from_secs(1)));
    }
    #[test]
    fn normal_eight_player_room_and_identity_churn_have_finite_costs() {
        let mut b = RelayBudget::new();
        let now = b.start;
        let room = Uuid::new_v4();
        let ids: Vec<_> = (0..8).map(|_| Uuid::new_v4()).collect();
        // All 56 directed paths, 30 packets per second, 512 bytes including framing
        for _ in 0..30 {
            for id in &ids {
                for _ in 0..7 {
                    assert!(b.accept(*id, room, 512, now));
                }
            }
        }
        assert_eq!(b.global.bytes, 56 * 30 * 512);
        let next = now + Duration::from_secs(1);
        for _ in 0..1024 {
            assert!(b.accept(Uuid::new_v4(), room, 1, next));
        }
        assert!(!b.accept(Uuid::new_v4(), room, 1, next));
        assert_eq!(b.identities.len(), 1024);
    }
    #[test]
    fn aggregate_limits_survive_pair_and_room_churn_and_reset() {
        let mut b = RelayBudget::new();
        let now = b.start;
        let id = Uuid::new_v4();
        for _ in 0..256 {
            assert!(b.accept(id, Uuid::new_v4(), 4096, now));
        }
        assert!(!b.accept(id, Uuid::new_v4(), 1, now));
        let room = Uuid::new_v4();
        for _ in 0..4 {
            let p = Uuid::new_v4();
            for _ in 0..256 {
                assert!(b.accept(p, room, 4096, now));
            }
        }
        assert!(!b.accept(Uuid::new_v4(), room, 1, now));
        for _ in 0..3 {
            let p = Uuid::new_v4();
            let r = Uuid::new_v4();
            for _ in 0..256 {
                assert!(b.accept(p, r, 4096, now));
            }
        }
        assert!(!b.accept(Uuid::new_v4(), Uuid::new_v4(), 1, now));
        assert!(b.accept(id, room, 4096, now + Duration::from_secs(1)));
    }
}
