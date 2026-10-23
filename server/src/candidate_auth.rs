//! Return-path ownership proofs for candidate discovery
//!
//! The control plane issues one secret per live lobby admission, never a peer-selected address
//! A UDP listener returns a fresh receipt only to its observed source; the control plane must
//! redeem that receipt in the same scope before distributing the endpoint to other players

use std::collections::HashMap;
use std::net::{IpAddr, SocketAddr};
use std::sync::Mutex;
use std::time::{Duration, Instant};
use uuid::Uuid;

pub const ATTEMPT_LIFETIME: Duration = Duration::from_secs(15);
pub const MAX_ATTEMPTS: usize = 500;
pub const MAX_OBSERVATIONS: usize = 2;
pub const MAX_RESPONSES_PER_OBSERVATION: u8 = 3;

/// Values come from server-owned connection/lobby state, not a client request
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct AttemptScope {
    pub player_id: Uuid,
    pub connection_id: Uuid,
    pub lobby_id: Uuid,
    pub admission_id: Uuid,
    pub public_ip: IpAddr,
    /// Only explicit local development may observe loopback endpoints, never private LAN addresses
    pub local_development: bool,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct CandidateChallenge {
    pub attempt_id: Uuid,
    /// Secret sent only on this player's authenticated control connection
    pub credential: Uuid,
}

impl std::fmt::Debug for CandidateChallenge {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("CandidateChallenge")
            .field("attempt_id", &self.attempt_id)
            .finish_non_exhaustive()
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct VerifiedCandidates {
    pub endpoints: Vec<SocketAddr>,
    pub nat_type: &'static str,
    pub expires_at: Option<Instant>,
}

#[derive(Clone, Copy)]
struct Observation {
    endpoint: SocketAddr,
    receipt: Uuid,
    responses: u8,
}

struct Attempt {
    scope: AttemptScope,
    challenge: CandidateChallenge,
    issued_at: Instant,
    consumed: bool,
    observations: [Option<Observation>; MAX_OBSERVATIONS],
}

impl Attempt {
    fn live(&self, now: Instant) -> bool {
        !self.consumed
            && now
                .checked_duration_since(self.issued_at)
                .is_some_and(|age| age < ATTEMPT_LIFETIME)
    }
}

#[derive(Default)]
struct Entries {
    by_player: HashMap<Uuid, Attempt>,
    by_credential: HashMap<Uuid, Uuid>,
}

impl Entries {
    fn remove(&mut self, player_id: Uuid) {
        if let Some(attempt) = self.by_player.remove(&player_id) {
            self.by_credential.remove(&attempt.challenge.credential);
        }
    }
}

pub struct CandidateAttempts {
    entries: Mutex<Entries>,
    capacity: usize,
}

impl Default for CandidateAttempts {
    fn default() -> Self {
        Self::new(MAX_ATTEMPTS)
    }
}

impl CandidateAttempts {
    pub fn new(capacity: usize) -> Self {
        Self {
            entries: Mutex::new(Entries::default()),
            capacity: capacity.min(MAX_ATTEMPTS),
        }
    }

    /// Retries never rotate credentials, refresh expiry or reopen a consumed attempt
    /// A new scope requires a server-authorized admission/round/connection transition
    pub fn issue(&self, scope: AttemptScope, now: Instant) -> Option<CandidateChallenge> {
        if [
            scope.player_id,
            scope.connection_id,
            scope.lobby_id,
            scope.admission_id,
        ]
        .iter()
        .any(Uuid::is_nil)
        {
            return None;
        }
        let mut entries = self.entries.lock().unwrap();
        if let Some(attempt) = entries.by_player.get(&scope.player_id) {
            if attempt.scope == scope {
                return Some(attempt.challenge);
            }
        } else if entries.by_player.len() >= self.capacity {
            return None;
        }
        entries.remove(scope.player_id);
        let challenge = CandidateChallenge {
            attempt_id: Uuid::new_v4(),
            credential: Uuid::new_v4(),
        };
        entries
            .by_credential
            .insert(challenge.credential, scope.player_id);
        entries.by_player.insert(
            scope.player_id,
            Attempt {
                scope,
                challenge,
                issued_at: now,
                consumed: false,
                observations: [None; MAX_OBSERVATIONS],
            },
        );
        Some(challenge)
    }

    /// A credential proves request authorization, not source-port ownership
    /// Only redeeming the returned receipt over the control connection proves receipt at source
    pub fn observe(
        &self,
        credential: Uuid,
        listener: usize,
        source: SocketAddr,
        now: Instant,
    ) -> Option<Uuid> {
        if listener >= MAX_OBSERVATIONS || source.port() == 0 {
            return None;
        }
        let mut entries = self.entries.lock().unwrap();
        let player = *entries.by_credential.get(&credential)?;
        let attempt = entries.by_player.get_mut(&player)?;
        if !attempt.live(now)
            || source.ip() != attempt.scope.public_ip
            || !eligible_ip(source.ip(), attempt.scope.local_development)
        {
            return None;
        }
        let observation = attempt.observations[listener].get_or_insert_with(|| Observation {
            endpoint: source,
            receipt: Uuid::new_v4(),
            responses: 0,
        });
        if observation.endpoint != source || observation.responses >= MAX_RESPONSES_PER_OBSERVATION
        {
            return None;
        }
        observation.responses += 1;
        Some(observation.receipt)
    }

    /// Accept at most one report, including an empty relay-only report
    /// Invalid receipts leave the attempt unchanged, without promoting any claimed address
    pub fn consume(
        &self,
        scope: AttemptScope,
        attempt_id: Uuid,
        receipts: &[Uuid],
        now: Instant,
    ) -> Option<VerifiedCandidates> {
        if receipts.len() > MAX_OBSERVATIONS || (receipts.len() == 2 && receipts[0] == receipts[1])
        {
            return None;
        }
        let mut entries = self.entries.lock().unwrap();
        let attempt = entries.by_player.get_mut(&scope.player_id)?;
        if attempt.scope != scope
            || attempt.challenge.attempt_id != attempt_id
            || attempt.consumed
            || (!receipts.is_empty() && !attempt.live(now))
        {
            return None;
        }
        let mut endpoints = Vec::with_capacity(MAX_OBSERVATIONS);
        for receipt in receipts {
            let observation = attempt
                .observations
                .iter()
                .flatten()
                .find(|observation| observation.receipt == *receipt)?;
            endpoints.push(observation.endpoint);
        }
        let nat_type = match endpoints.as_slice() {
            [a, b] if a == b => "full_cone",
            [_, _] => "symmetric",
            _ => "unknown",
        };
        endpoints.dedup();
        attempt.consumed = true;
        Some(VerifiedCandidates {
            endpoints,
            nat_type,
            expires_at: (!receipts.is_empty()).then_some(attempt.issued_at + ATTEMPT_LIFETIME),
        })
    }

    /// Stale cleanup from a superseded connection/admission must not revoke its replacement
    pub fn revoke(&self, scope: AttemptScope) {
        let mut entries = self.entries.lock().unwrap();
        if entries
            .by_player
            .get(&scope.player_id)
            .is_some_and(|attempt| attempt.scope == scope)
        {
            entries.remove(scope.player_id);
        }
    }

    pub fn revoke_lobby(&self, lobby_id: Uuid) {
        let mut entries = self.entries.lock().unwrap();
        let players: Vec<_> = entries
            .by_player
            .iter()
            .filter_map(|(player, attempt)| (attempt.scope.lobby_id == lobby_id).then_some(*player))
            .collect();
        for player in players {
            entries.remove(player);
        }
    }

    pub fn revoke_connection(&self, player_id: Uuid, connection_id: Uuid) {
        let mut entries = self.entries.lock().unwrap();
        if entries
            .by_player
            .get(&player_id)
            .is_some_and(|a| a.scope.connection_id == connection_id)
        {
            entries.remove(player_id);
        }
    }

    pub fn is_empty(&self) -> bool {
        let entries = self.entries.lock().unwrap();
        entries.by_player.is_empty() && entries.by_credential.is_empty()
    }
}

/// Conservative global-unicast IPv4 policy; the existing candidate transport is IPv4-only
fn eligible_ip(ip: IpAddr, local_development: bool) -> bool {
    let IpAddr::V4(ip) = ip else {
        return false;
    };
    if ip.is_loopback() {
        return local_development;
    }
    let [a, b, c, _] = ip.octets();
    !(a == 0
        || a == 10
        || a >= 224
        || (a == 100 && (64..=127).contains(&b))
        || (a == 169 && b == 254)
        || (a == 172 && (16..=31).contains(&b))
        || (a == 192 && b == 168)
        || (a == 192 && b == 0 && (c == 0 || c == 2))
        || (a == 192 && b == 88 && c == 99)
        || (a == 198 && (b == 18 || b == 19))
        || (a == 198 && b == 51 && c == 100)
        || (a == 203 && b == 0 && c == 113))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scope(ip: &str) -> AttemptScope {
        AttemptScope {
            player_id: Uuid::new_v4(),
            connection_id: Uuid::new_v4(),
            lobby_id: Uuid::new_v4(),
            admission_id: Uuid::new_v4(),
            public_ip: ip.parse().unwrap(),
            local_development: false,
        }
    }

    #[test]
    fn ownership_requires_a_returned_receipt_not_the_request_credential() {
        let registry = CandidateAttempts::default();
        let scope = scope("8.8.4.4");
        let now = Instant::now();
        let challenge = registry.issue(scope, now).unwrap();
        assert!(!format!("{challenge:?}").contains(&challenge.credential.to_string()));
        let source = "8.8.4.4:4242".parse().unwrap();
        assert!(registry.observe(Uuid::new_v4(), 0, source, now).is_none());
        assert!(registry
            .observe(
                challenge.credential,
                0,
                "8.8.8.8:4242".parse().unwrap(),
                now
            )
            .is_none());
        assert!(registry
            .consume(scope, challenge.attempt_id, &[challenge.credential], now)
            .is_none());
        let receipt = registry
            .observe(challenge.credential, 0, source, now)
            .unwrap();
        assert!(registry
            .consume(scope, challenge.attempt_id, &[Uuid::new_v4()], now)
            .is_none());
        let verified = registry
            .consume(
                scope,
                challenge.attempt_id,
                &[receipt],
                now + Duration::from_secs(5),
            )
            .unwrap();
        assert_eq!(verified.endpoints, vec![source]);
        assert_eq!(verified.expires_at, Some(now + ATTEMPT_LIFETIME));
        assert_eq!(verified.nat_type, "unknown");
        assert!(registry
            .consume(scope, challenge.attempt_id, &[receipt], now)
            .is_none());
        assert!(registry
            .observe(challenge.credential, 1, source, now)
            .is_none());
        assert_eq!(registry.issue(scope, now).unwrap(), challenge);
    }

    #[test]
    fn two_slots_have_fixed_endpoints_receipts_and_response_limits() {
        let registry = CandidateAttempts::default();
        let scope = scope("8.8.4.4");
        let now = Instant::now();
        let challenge = registry.issue(scope, now).unwrap();
        let source = "8.8.4.4:4242".parse().unwrap();
        let mut receipts = Vec::new();
        for listener in 0..MAX_OBSERVATIONS {
            let receipt = registry
                .observe(challenge.credential, listener, source, now)
                .unwrap();
            receipts.push(receipt);
            assert!(registry
                .observe(
                    challenge.credential,
                    listener,
                    "8.8.4.4:4243".parse().unwrap(),
                    now
                )
                .is_none());
            for _ in 1..MAX_RESPONSES_PER_OBSERVATION {
                assert_eq!(
                    registry.observe(challenge.credential, listener, source, now),
                    Some(receipt)
                );
            }
            assert!(registry
                .observe(challenge.credential, listener, source, now)
                .is_none());
            assert!(registry
                .observe(
                    challenge.credential,
                    listener,
                    "8.8.4.4:4243".parse().unwrap(),
                    now
                )
                .is_none());
        }
        assert_ne!(receipts[0], receipts[1]);
        assert!(registry
            .observe(challenge.credential, 2, source, now)
            .is_none());
        assert!(registry
            .consume(
                scope,
                challenge.attempt_id,
                &[receipts[0], receipts[0]],
                now
            )
            .is_none());
        let verified = registry
            .consume(scope, challenge.attempt_id, &receipts, now)
            .unwrap();
        assert_eq!(verified.endpoints, vec![source]);
        assert_eq!(verified.nat_type, "full_cone");
    }

    #[test]
    fn retries_cannot_extend_expiry_and_empty_reports_are_one_shot() {
        let registry = CandidateAttempts::default();
        let scope = scope("8.8.8.8");
        let now = Instant::now();
        let challenge = registry.issue(scope, now).unwrap();
        let source = "8.8.8.8:4242".parse().unwrap();
        let receipt = registry
            .observe(challenge.credential, 0, source, now)
            .unwrap();
        assert_eq!(
            registry.issue(scope, now + ATTEMPT_LIFETIME).unwrap(),
            challenge
        );
        assert!(registry
            .observe(challenge.credential, 0, source, now + ATTEMPT_LIFETIME)
            .is_none());
        assert!(registry
            .consume(
                scope,
                challenge.attempt_id,
                &[receipt],
                now + ATTEMPT_LIFETIME
            )
            .is_none());
        // Expired endpoints cannot be promoted, but an empty report can still select relay
        assert!(registry
            .consume(scope, challenge.attempt_id, &[], now + ATTEMPT_LIFETIME)
            .is_some());
        let next = AttemptScope {
            admission_id: Uuid::new_v4(),
            ..scope
        };
        let fresh = registry.issue(next, now + ATTEMPT_LIFETIME).unwrap();
        assert_ne!(fresh, challenge);
        assert!(registry
            .consume(next, fresh.attempt_id, &[], now + ATTEMPT_LIFETIME)
            .unwrap()
            .endpoints
            .is_empty());
        assert!(registry
            .consume(next, fresh.attempt_id, &[], now + ATTEMPT_LIFETIME)
            .is_none());
    }

    #[test]
    fn every_scope_dimension_fences_old_proofs_and_stale_revocation() {
        let registry = CandidateAttempts::default();
        let initial = scope("8.8.8.8");
        let now = Instant::now();
        for next in [
            AttemptScope {
                player_id: Uuid::new_v4(),
                ..initial
            },
            AttemptScope {
                connection_id: Uuid::new_v4(),
                ..initial
            },
            AttemptScope {
                lobby_id: Uuid::new_v4(),
                ..initial
            },
            AttemptScope {
                admission_id: Uuid::new_v4(),
                ..initial
            },
            AttemptScope {
                public_ip: "8.8.4.4".parse().unwrap(),
                ..initial
            },
        ] {
            let old = registry.issue(initial, now).unwrap();
            let receipt = registry
                .observe(old.credential, 0, "8.8.8.8:4242".parse().unwrap(), now)
                .unwrap();
            assert!(registry
                .consume(next, old.attempt_id, &[receipt], now)
                .is_none());
            let fresh = registry.issue(next, now).unwrap();
            assert!(registry
                .consume(next, old.attempt_id, &[receipt], now)
                .is_none());
            registry.revoke(initial);
            assert!(registry.consume(next, fresh.attempt_id, &[], now).is_some());
            registry.revoke(next);
        }
    }

    #[test]
    fn capacity_and_revocation_bound_all_indexes() {
        let registry = CandidateAttempts::new(2);
        let now = Instant::now();
        let a = scope("8.8.8.8");
        let b = scope("8.8.4.4");
        let c = scope("1.1.1.1");
        let old = registry.issue(a, now).unwrap();
        registry.issue(b, now).unwrap();
        assert!(registry.issue(c, now).is_none());
        let replacement = AttemptScope {
            admission_id: Uuid::new_v4(),
            ..a
        };
        registry.issue(replacement, now).unwrap();
        assert!(registry
            .observe(old.credential, 0, "8.8.8.8:4242".parse().unwrap(), now)
            .is_none());
        registry.revoke_lobby(a.lobby_id);
        registry.issue(c, now).unwrap();
        registry.revoke(b);
        registry.revoke(c);
        let entries = registry.entries.lock().unwrap();
        assert!(entries.by_player.is_empty());
        assert!(entries.by_credential.is_empty());
    }

    #[test]
    fn missing_server_owned_scope_ids_cannot_create_an_unfenced_attempt() {
        let registry = CandidateAttempts::default();
        let scope = scope("8.8.8.8");
        for invalid in [
            AttemptScope {
                player_id: Uuid::nil(),
                ..scope
            },
            AttemptScope {
                connection_id: Uuid::nil(),
                ..scope
            },
            AttemptScope {
                lobby_id: Uuid::nil(),
                ..scope
            },
            AttemptScope {
                admission_id: Uuid::nil(),
                ..scope
            },
        ] {
            assert!(registry.issue(invalid, Instant::now()).is_none());
        }
        let entries = registry.entries.lock().unwrap();
        assert!(entries.by_player.is_empty());
        assert!(entries.by_credential.is_empty());
    }

    #[test]
    fn internet_attempts_never_authorize_private_or_special_destinations() {
        let registry = CandidateAttempts::default();
        for ip in [
            "0.1.2.3",
            "10.0.0.1",
            "100.64.0.1",
            "127.0.0.1",
            "169.254.1.2",
            "172.16.0.1",
            "192.168.1.1",
            "192.0.0.1",
            "192.0.2.1",
            "192.88.99.1",
            "198.18.1.2",
            "198.51.100.1",
            "203.0.113.1",
            "224.0.0.1",
            "240.0.0.1",
            "255.255.255.255",
            "::1",
            "::ffff:8.8.8.8",
            "2001:4860:4860::8888",
        ] {
            let scope = scope(ip);
            let now = Instant::now();
            let challenge = registry.issue(scope, now).unwrap();
            assert!(
                registry
                    .observe(
                        challenge.credential,
                        0,
                        SocketAddr::new(scope.public_ip, 4242),
                        now
                    )
                    .is_none(),
                "{ip}"
            );
            assert!(registry
                .consume(scope, challenge.attempt_id, &[], now)
                .unwrap()
                .endpoints
                .is_empty());
            let dev = AttemptScope {
                local_development: true,
                ..scope
            };
            let challenge = registry.issue(dev, now).unwrap();
            assert_eq!(
                registry
                    .observe(
                        challenge.credential,
                        0,
                        SocketAddr::new(dev.public_ip, 4242),
                        now
                    )
                    .is_some(),
                ip == "127.0.0.1",
                "{ip}"
            );
        }
    }
}
