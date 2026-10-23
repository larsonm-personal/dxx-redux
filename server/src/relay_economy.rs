//! Egress accounting and economic limits, independent of disposable relay allocations
use std::collections::HashMap;
use std::time::{Duration, Instant};

use chrono::{Datelike, TimeZone, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

const MIB: u64 = 1024 * 1024;
const CREDIT_BATCH: u64 = 64 * MIB;
const MAX_IDENTITIES: usize = 4096;
const DAY: Duration = Duration::from_secs(86400);
// Conservative UDP/IP allowance, including fragmentation at ordinary Internet MTUs
// Provider/OS limits remain necessary for unusual tunnels and link-layer billing
pub const WIRE_OVERHEAD: usize = 256;

#[derive(Clone, Deserialize, Serialize)]
#[serde(default, deny_unknown_fields)]
pub struct RelayPolicy {
    pub enforce_identity_quotas: bool,
    pub sustained_bytes_per_second: u64,
    pub burst_bytes: u64,
    pub identity_ten_minute_bytes: u64,
    pub identity_hour_bytes: u64,
    pub identity_day_bytes: u64,
    pub monthly_bytes: u64,
    pub allocation_lifetime_seconds: u64,
}
impl Default for RelayPolicy {
    fn default() -> Self {
        Self {
            enforce_identity_quotas: false,
            sustained_bytes_per_second: 256 * 1024,
            burst_bytes: 256 * MIB,
            identity_ten_minute_bytes: 512 * MIB,
            identity_hour_bytes: 2048 * MIB,
            identity_day_bytes: 8192 * MIB,
            monthly_bytes: 1024 * 1024 * MIB,
            allocation_lifetime_seconds: 7200,
        }
    }
}
impl RelayPolicy {
    pub fn validate(&self) -> bool {
        self.sustained_bytes_per_second > 0
            && (10..=86400).contains(&self.allocation_lifetime_seconds)
            && self.sustained_bytes_per_second <= MIB
            && [
                self.burst_bytes,
                self.identity_ten_minute_bytes,
                self.identity_hour_bytes,
                self.identity_day_bytes,
                self.monthly_bytes,
            ]
            .iter()
            .all(|v| *v > 0 && *v <= i64::MAX as u64)
    }
}

#[derive(Clone, Copy)]
pub struct Month {
    pub period: i32,
    pub expires: Instant,
}
impl Month {
    pub fn current(now: Instant) -> Self {
        let utc = Utc::now();
        let (year, month) = if utc.month() == 12 {
            (utc.year() + 1, 1)
        } else {
            (utc.year(), utc.month() + 1)
        };
        let end = Utc
            .with_ymd_and_hms(year, month, 1, 0, 0, 0)
            .single()
            .unwrap();
        Self {
            period: utc.year() * 12 + utc.month0() as i32,
            expires: now + (end - utc).to_std().unwrap(),
        }
    }
}

#[derive(Clone)]
struct Window {
    started: Instant,
    bytes: u64,
}
impl Window {
    fn advance(&mut self, now: Instant, seconds: u64) {
        if now.saturating_duration_since(self.started) >= Duration::from_secs(seconds) {
            self.started = now;
            self.bytes = 0;
        }
    }
}
struct Identity {
    tokens: u128,
    refill_at: Instant,
    last_used: Instant,
    windows: [Window; 3],
    packets: u64,
    bytes: u64,
    over_quota_packets: u64,
}
impl Identity {
    fn new(now: Instant, burst: u64) -> Self {
        Self {
            tokens: burst as u128 * 1_000_000_000,
            refill_at: now,
            last_used: now,
            windows: std::array::from_fn(|_| Window {
                started: now,
                bytes: 0,
            }),
            packets: 0,
            bytes: 0,
            over_quota_packets: 0,
        }
    }
}

#[derive(Clone, Copy, Default, Serialize)]
pub struct Counters {
    pub forwarded_packets: u64,
    pub forwarded_wire_bytes: u64,
    pub over_identity_quota_packets: u64,
    pub denied_identity_quota_packets: u64,
    pub denied_identity_capacity_packets: u64,
    pub denied_monthly_credit_packets: u64,
    pub denied_global_rate_packets: u64,
    pub persistence_failures: u64,
}
#[derive(Serialize)]
pub struct IdentityUsage {
    pub player_id: Uuid,
    pub forwarded_packets: u64,
    pub forwarded_wire_bytes: u64,
    pub ten_minute_bytes: u64,
    pub hour_bytes: u64,
    pub day_bytes: u64,
    pub over_quota_packets: u64,
}
#[derive(Serialize)]
pub struct Snapshot {
    pub policy: RelayPolicy,
    pub counters: Counters,
    pub month_period: i32,
    pub monthly_committed_wire_bytes: u64,
    pub prepaid_wire_bytes_remaining: u64,
    pub monthly_alert: bool,
    pub relay_credit_available: bool,
    pub tracked_identities: usize,
    pub identity_offset: usize,
    pub next_identity_offset: Option<usize>,
    pub identities: Vec<IdentityUsage>,
}

pub struct RelayEconomy {
    policy: RelayPolicy,
    month: Month,
    remaining: u64,
    committed: u64,
    alert_logged: bool,
    pending_credit: Option<i32>,
    next_prune: Instant,
    global_window: Window,
    identities: HashMap<Uuid, Identity>,
    counters: Counters,
}
impl RelayEconomy {
    pub fn new(policy: RelayPolicy, month: Month, now: Instant) -> Self {
        Self {
            policy,
            month,
            remaining: 0,
            committed: 0,
            alert_logged: false,
            pending_credit: None,
            next_prune: now + Duration::from_secs(60),
            global_window: Window {
                started: now,
                bytes: 0,
            },
            identities: HashMap::new(),
            counters: Counters::default(),
        }
    }

    // Only one bounded durable reservation may be outstanding, even with concurrent maintenance
    pub fn request_credit(&mut self, month: Month) -> Option<(i32, u64, u64)> {
        if self.pending_credit.is_some() || month.period < self.month.period {
            return None;
        }
        if month.period > self.month.period {
            self.month = month;
            self.remaining = 0;
            self.committed = 0;
            self.alert_logged = false;
        }
        if self.remaining >= CREDIT_BATCH / 2 || self.committed >= self.policy.monthly_bytes {
            return None;
        }
        self.pending_credit = Some(month.period);
        Some((month.period, self.policy.monthly_bytes, CREDIT_BATCH))
    }

    pub fn finish_credit(&mut self, result: Result<(i32, u64, u64), rusqlite::Error>) {
        let Some(period) = self.pending_credit.take() else {
            return;
        };
        match result {
            Ok((stored_period, committed, grant)) if stored_period == period => {
                self.committed = committed;
                self.remaining = self.remaining.saturating_add(grant);
            }
            _ => {
                self.counters.persistence_failures =
                    self.counters.persistence_failures.saturating_add(1);
            }
        }
    }

    pub fn can_allocate(&self, now: Instant) -> bool {
        let handshake_bytes = 4 * (crate::relay::HEADER_LEN + 16 + WIRE_OVERHEAD) + 32;
        now < self.month.expires && self.remaining >= handshake_bytes as u64
    }

    pub fn totals(&self) -> Counters {
        self.counters
    }

    fn take_monthly_alert(&mut self) -> Option<(i32, u64, u64)> {
        if self.alert_logged
            || self.committed < self.policy.monthly_bytes - self.policy.monthly_bytes / 4
        {
            return None;
        }
        self.alert_logged = true;
        Some((self.month.period, self.committed, self.policy.monthly_bytes))
    }

    // Charge only an authenticated, valid response that would otherwise be emitted
    pub fn reserve(&mut self, player: Uuid, wire_bytes: usize, now: Instant) -> bool {
        let bytes = wire_bytes as u64;
        if now >= self.month.expires || bytes > self.remaining {
            self.counters.denied_monthly_credit_packets = self
                .counters
                .denied_monthly_credit_packets
                .saturating_add(1);
            return false;
        }
        self.global_window.advance(now, 1);
        if bytes > (8 * MIB).saturating_sub(self.global_window.bytes) {
            self.counters.denied_global_rate_packets =
                self.counters.denied_global_rate_packets.saturating_add(1);
            return false;
        }
        if now >= self.next_prune {
            self.identities
                .retain(|_, v| now.saturating_duration_since(v.last_used) < DAY);
            self.next_prune = now + Duration::from_secs(60);
        }
        if !self.identities.contains_key(&player) && self.identities.len() >= MAX_IDENTITIES {
            self.counters.denied_identity_capacity_packets = self
                .counters
                .denied_identity_capacity_packets
                .saturating_add(1);
            return false;
        }
        let identity = self
            .identities
            .entry(player)
            .or_insert_with(|| Identity::new(now, self.policy.burst_bytes));
        let refill = now
            .saturating_duration_since(identity.refill_at)
            .as_nanos()
            .saturating_mul(self.policy.sustained_bytes_per_second as u128);
        identity.tokens = identity
            .tokens
            .saturating_add(refill)
            .min(self.policy.burst_bytes as u128 * 1_000_000_000);
        identity.refill_at = now;
        for (window, seconds) in identity.windows.iter_mut().zip([600, 3600, 86400]) {
            window.advance(now, seconds);
        }
        let token_cost = bytes as u128 * 1_000_000_000;
        let limits = [
            self.policy.identity_ten_minute_bytes,
            self.policy.identity_hour_bytes,
            self.policy.identity_day_bytes,
        ];
        let over_quota = token_cost > identity.tokens
            || identity
                .windows
                .iter()
                .zip(limits)
                .any(|(w, limit)| bytes > limit.saturating_sub(w.bytes));
        if over_quota {
            identity.over_quota_packets = identity.over_quota_packets.saturating_add(1);
            self.counters.over_identity_quota_packets =
                self.counters.over_identity_quota_packets.saturating_add(1);
            if self.policy.enforce_identity_quotas {
                self.counters.denied_identity_quota_packets = self
                    .counters
                    .denied_identity_quota_packets
                    .saturating_add(1);
                return false;
            }
        }
        identity.tokens = identity.tokens.saturating_sub(token_cost);
        for window in &mut identity.windows {
            window.bytes = window.bytes.saturating_add(bytes);
        }
        identity.last_used = now;
        identity.packets = identity.packets.saturating_add(1);
        identity.bytes = identity.bytes.saturating_add(bytes);
        self.remaining -= bytes;
        self.global_window.bytes += bytes;
        self.counters.forwarded_packets = self.counters.forwarded_packets.saturating_add(1);
        self.counters.forwarded_wire_bytes =
            self.counters.forwarded_wire_bytes.saturating_add(bytes);
        true
    }

    pub fn snapshot(&self, offset: usize, now: Instant) -> Snapshot {
        let mut keys: Vec<_> = self.identities.keys().copied().collect();
        keys.sort_unstable();
        let identities = keys
            .iter()
            .skip(offset)
            .take(100)
            .map(|key| {
                let v = &self.identities[key];
                let mut windows = v.windows.clone();
                for (w, seconds) in windows.iter_mut().zip([600, 3600, 86400]) {
                    w.advance(now, seconds);
                }
                IdentityUsage {
                    player_id: *key,
                    forwarded_packets: v.packets,
                    forwarded_wire_bytes: v.bytes,
                    ten_minute_bytes: windows[0].bytes,
                    hour_bytes: windows[1].bytes,
                    day_bytes: windows[2].bytes,
                    over_quota_packets: v.over_quota_packets,
                }
            })
            .collect();
        Snapshot {
            policy: self.policy.clone(),
            counters: Counters { ..self.counters },
            month_period: self.month.period,
            monthly_committed_wire_bytes: self.committed,
            prepaid_wire_bytes_remaining: self.remaining,
            monthly_alert: self.committed
                >= self.policy.monthly_bytes - self.policy.monthly_bytes / 4,
            relay_credit_available: self.can_allocate(now),
            tracked_identities: self.identities.len(),
            identity_offset: offset,
            next_identity_offset: (offset.saturating_add(100) < keys.len())
                .then_some(offset.saturating_add(100)),
            identities,
        }
    }
}

/// No SQLite work, control lock or network await on the UDP packet path
pub async fn replenish(state: std::sync::Arc<crate::ServerState>) {
    let plan = state
        .relay_economy
        .lock()
        .unwrap()
        .request_credit(Month::current(Instant::now()));
    if let Some((period, limit, wanted)) = plan {
        let db_state = std::sync::Arc::clone(&state);
        let result = tokio::task::spawn_blocking(move || {
            db_state.db.reserve_relay_egress(period, limit, wanted)
        })
        .await
        .unwrap_or(Err(rusqlite::Error::InvalidQuery));
        state.relay_economy.lock().unwrap().finish_credit(result);
    }
    let warning = state.relay_economy.lock().unwrap().take_monthly_alert();
    if let Some((period, committed, limit)) = warning {
        tracing::warn!(
            period,
            committed_wire_bytes = committed,
            limit_wire_bytes = limit,
            "Relay monthly egress reservations reached 75 percent of the configured cap"
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fixture(policy: RelayPolicy) -> (RelayEconomy, Instant) {
        let now = Instant::now();
        let mut economy = RelayEconomy::new(
            policy,
            Month {
                period: 100,
                expires: now + Duration::from_secs(40 * 86400),
            },
            now,
        );
        let db = crate::db::Database::open(":memory:").unwrap();
        let (period, limit, wanted) = economy.request_credit(economy.month).unwrap();
        economy.finish_credit(db.reserve_relay_egress(period, limit, wanted));
        (economy, now)
    }

    #[test]
    fn sustained_bucket_keeps_fractional_refill_and_does_not_refund_rejections() {
        let (mut e, now) = fixture(RelayPolicy {
            enforce_identity_quotas: true,
            burst_bytes: 1000,
            sustained_bytes_per_second: 100,
            ..Default::default()
        });
        let id = Uuid::new_v4();
        assert!(e.reserve(id, 1000, now));
        assert!(!e.reserve(id, 1, now));
        assert!(e.reserve(id, 500, now + Duration::from_secs(5)));
        assert!(!e.reserve(id, 1, now + Duration::from_secs(5)));
        assert!(e.reserve(id, 1, now + Duration::from_millis(5010)));
        assert_eq!(e.snapshot(0, now).counters.forwarded_wire_bytes, 1501);
    }

    #[test]
    fn all_three_identity_windows_reject_and_recover_independently() {
        for (index, seconds) in [(0, 600), (1, 3600), (2, 86400)] {
            let mut policy = RelayPolicy {
                enforce_identity_quotas: true,
                ..Default::default()
            };
            match index {
                0 => policy.identity_ten_minute_bytes = 100,
                1 => policy.identity_hour_bytes = 100,
                _ => policy.identity_day_bytes = 100,
            }
            let (mut e, now) = fixture(policy);
            let id = Uuid::new_v4();
            assert!(e.reserve(id, 100, now));
            assert!(!e.reserve(id, 1, now));
            assert!(!e.reserve(id, 1, now + Duration::from_secs(seconds - 1)));
            assert!(e.reserve(id, 100, now + Duration::from_secs(seconds)));
            assert_eq!(e.counters.denied_identity_quota_packets, 2);
        }
    }

    #[test]
    fn telemetry_mode_still_enforces_monthly_credit_and_accounts_every_byte() {
        let (mut e, now) = fixture(RelayPolicy {
            burst_bytes: 1,
            monthly_bytes: 250,
            ..Default::default()
        });
        let id = Uuid::new_v4();
        for bytes in [100, 100, 50] {
            assert!(e.reserve(id, bytes, now));
        }
        assert!(!e.reserve(id, 1, now));
        let snap = e.snapshot(0, now);
        assert_eq!(snap.counters.forwarded_wire_bytes, 250);
        assert_eq!(snap.counters.over_identity_quota_packets, 3);
        assert_eq!(snap.counters.denied_identity_quota_packets, 0);
        assert_eq!(snap.identities[0].day_bytes, 250);
        assert!(snap.monthly_alert);
        assert!(e.request_credit(e.month).is_none());
    }

    #[test]
    fn global_wire_rate_is_enforced_even_when_identity_limits_only_observe() {
        let (mut e, now) = fixture(RelayPolicy::default());
        assert!(e.reserve(Uuid::new_v4(), (8 * MIB) as usize, now));
        assert!(!e.reserve(Uuid::new_v4(), 1, now));
        assert!(e.reserve(Uuid::new_v4(), 1, now + Duration::from_secs(1)));
        assert_eq!(e.counters.denied_global_rate_packets, 1);
    }

    #[test]
    fn largest_supported_mission_fits_a_single_peer_transfer_with_enforcement() {
        let now = Instant::now();
        let month = Month {
            period: 100,
            expires: now + DAY,
        };
        let mut e = RelayEconomy::new(
            RelayPolicy {
                enforce_identity_quotas: true,
                ..Default::default()
            },
            month,
            now,
        );
        let db = crate::db::Database::open(":memory:").unwrap();
        let id = Uuid::new_v4();
        // 256 MiB in 4096-byte payloads, below the existing per-peer one-second byte limit
        for packet in 0..65536 {
            if let Some((period, limit, wanted)) = e.request_credit(month) {
                e.finish_credit(db.reserve_relay_egress(period, limit, wanted));
            }
            assert!(e.reserve(
                id,
                crate::relay::MAX_PACKET + WIRE_OVERHEAD,
                now + Duration::from_secs(packet / 253)
            ));
        }
        assert_eq!(e.counters.over_identity_quota_packets, 0);
        assert_eq!(e.counters.forwarded_packets, 65536);
        assert_eq!(
            e.counters.forwarded_wire_bytes,
            65536 * (crate::relay::MAX_PACKET + WIRE_OVERHEAD) as u64
        );
    }

    #[test]
    fn bounded_identity_records_are_not_evicted_by_churn_and_pages_are_bounded() {
        let (mut e, now) = fixture(RelayPolicy::default());
        for _ in 0..MAX_IDENTITIES {
            assert!(e.reserve(Uuid::new_v4(), 1, now));
        }
        assert!(!e.reserve(Uuid::new_v4(), 1, now));
        let first = e.snapshot(0, now);
        let second = e.snapshot(100, now);
        assert_eq!(first.identities.len(), 100);
        assert_eq!(first.next_identity_offset, Some(100));
        assert!(first
            .identities
            .iter()
            .all(|a| second.identities.iter().all(|b| a.player_id != b.player_id)));
        assert!(e.snapshot(usize::MAX, now).identities.is_empty());
        assert!(e.reserve(Uuid::new_v4(), 1, now + DAY));
        assert_eq!(e.identities.len(), 1);
    }

    #[test]
    fn only_one_credit_request_can_run_and_failure_or_month_expiry_fails_closed() {
        let now = Instant::now();
        let month = Month {
            period: 100,
            expires: now + Duration::from_secs(10),
        };
        let mut e = RelayEconomy::new(RelayPolicy::default(), month, now);
        assert!(e.request_credit(month).is_some());
        assert!(e.request_credit(month).is_none());
        e.finish_credit(Err(rusqlite::Error::InvalidQuery));
        assert!(!e.reserve(Uuid::new_v4(), 1, now));
        assert_eq!(e.counters.persistence_failures, 1);
        assert!(e.request_credit(month).is_some());
        e.finish_credit(Ok((100, 1000, 1000)));
        assert!(e.reserve(Uuid::new_v4(), 1, now));
        assert!(!e.reserve(Uuid::new_v4(), 1, now + Duration::from_secs(10)));
        assert!(e
            .request_credit(Month {
                period: 99,
                expires: month.expires
            })
            .is_none());
    }

    #[test]
    fn persistent_reservations_survive_restart_month_rollover_and_clock_rollback() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("budget.sqlite");
        let path = path.to_str().unwrap();
        {
            let db = crate::db::Database::open(path).unwrap();
            assert_eq!(
                db.reserve_relay_egress(100, 100, 60).unwrap(),
                (100, 60, 60)
            );
        }
        let db = crate::db::Database::open(path).unwrap();
        assert_eq!(
            db.reserve_relay_egress(100, 100, 60).unwrap(),
            (100, 100, 40)
        );
        assert_eq!(
            db.reserve_relay_egress(100, 100, 60).unwrap(),
            (100, 100, 0)
        );
        assert_eq!(
            db.reserve_relay_egress(101, 100, 60).unwrap(),
            (101, 60, 60)
        );
        assert_eq!(db.reserve_relay_egress(100, 100, 60).unwrap(), (101, 60, 0));
        assert_eq!(
            db.reserve_relay_egress(101, 100, 60).unwrap(),
            (101, 100, 40)
        );
        for (limit, amount) in [(0, 1), (u64::MAX, 1), (100, 0)] {
            assert!(db.reserve_relay_egress(102, limit, amount).is_err());
        }
    }

    #[test]
    fn concurrent_database_handles_cannot_double_spend_monthly_cap() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("budget.sqlite");
        let handles: Vec<_> = (0..4)
            .map(|_| crate::db::Database::open(path.to_str().unwrap()).unwrap())
            .collect();
        let threads: Vec<_> = handles
            .into_iter()
            .map(|db| std::thread::spawn(move || db.reserve_relay_egress(100, 100, 60).unwrap().2))
            .collect();
        assert_eq!(
            threads.into_iter().map(|t| t.join().unwrap()).sum::<u64>(),
            100
        );
    }

    #[test]
    fn invalid_policies_fail_and_unknown_fields_do_not_silently_disable_limits() {
        assert!(serde_json::from_str::<RelayPolicy>(r#"{"monthly_byte": 123}"#).is_err());
        for policy in [
            RelayPolicy {
                monthly_bytes: 0,
                ..Default::default()
            },
            RelayPolicy {
                burst_bytes: u64::MAX,
                ..Default::default()
            },
            RelayPolicy {
                sustained_bytes_per_second: MIB + 1,
                ..Default::default()
            },
        ] {
            assert!(!policy.validate());
        }
    }

    #[test]
    fn monthly_warning_is_emitted_once_per_process_and_month_not_per_packet() {
        let (mut e, now) = fixture(RelayPolicy {
            monthly_bytes: 100,
            ..Default::default()
        });
        assert_eq!(e.take_monthly_alert(), Some((100, 100, 100)));
        assert!(e.take_monthly_alert().is_none());
        assert!(e.reserve(Uuid::new_v4(), 1, now));
        assert!(e.take_monthly_alert().is_none());
        let next = Month {
            period: 101,
            expires: now + DAY,
        };
        assert!(e.request_credit(next).is_some());
        e.finish_credit(Ok((101, 100, 100)));
        assert_eq!(e.take_monthly_alert(), Some((101, 100, 100)));
    }
}
