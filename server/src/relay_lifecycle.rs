//! Server-owned, bounded make-before-break relay credential renewal
use std::sync::atomic::Ordering;
use std::time::{Duration, Instant};
use uuid::Uuid;

use crate::lobby::LobbyState;
use crate::protocol::{RelayRekeyScope, ServerMessage};
use crate::relay::{RelayOwnership, RelaySession};
use crate::ServerState;

#[derive(Clone, Copy)]
enum Phase {
    Staging { deadline: Instant },
    Committed { retire_at: Instant },
}
pub struct Renewal {
    pub room_id: Uuid,
    pub owner: RelayOwnership,
    old_epoch: Uuid,
    new_epoch: Uuid,
    pub(crate) new_token: u32,
    ready: [bool; 2],
    phase: Phase,
}
impl Renewal {
    fn scope(&self, old_token: u32, index: usize) -> RelayRekeyScope {
        let (peer_slot, peer_id, _) = self.owner.members[1 - index];
        RelayRekeyScope {
            lobby_id: self.room_id,
            game_epoch: self.owner.game_epoch,
            old_token,
            old_epoch: self.old_epoch,
            new_token: self.new_token,
            new_epoch: self.new_epoch,
            peer_id,
            peer_slot,
        }
    }
    fn send(&self, state: &ServerState, old_token: u32, kind: u8, milliseconds: u64) -> bool {
        let mut success = true;
        for (index, (slot, player, connection)) in self.owner.members.iter().enumerate() {
            let message = match kind {
                0 => {
                    let Some(credentials) = state
                        .relay_sessions
                        .get(&self.new_token)
                        .and_then(|s| s.credentials(*slot))
                    else {
                        return false;
                    };
                    ServerMessage::RelayRekeyOffer {
                        scope: self.scope(old_token, index),
                        credentials,
                        ready_timeout_ms: milliseconds,
                    }
                }
                1 => ServerMessage::RelayRekeyCommit {
                    scope: self.scope(old_token, index),
                    retire_after_ms: milliseconds,
                },
                _ => ServerMessage::RelayRekeyAbort {
                    scope: self.scope(old_token, index),
                },
            };
            let sent = state
                .sessions
                .get(player)
                .filter(|s| s.connection_id == *connection)
                .is_some_and(|s| s.tx.send(message).is_ok());
            success &= sent;
        }
        success
    }
}

fn live(state: &ServerState, room: Uuid, owner: &RelayOwnership) -> bool {
    let Some(lobby) = state.lobbies.get(&room) else {
        return false;
    };
    if !matches!(lobby.state, LobbyState::Starting | LobbyState::InGame)
        || lobby.game_epoch != owner.game_epoch
    {
        return false;
    }
    owner.members.iter().all(|(slot, player, connection)| {
        lobby
            .players
            .iter()
            .any(|p| p.slot == *slot && p.player_id == *player)
            && state.sessions.get(player).is_some_and(|s| {
                s.connection_id == *connection
                    && s.lobby_id == Some(room)
                    && (state.config.skip_gpgs_verify || s.account_verified)
            })
    })
}

// Caller holds the short control-transition lock, just like all authenticated handlers
pub fn ready(state: &ServerState, player: Uuid, ack: crate::protocol::RelayRekeyAck, now: Instant) {
    let crate::protocol::RelayRekeyAck {
        lobby_id: room,
        game_epoch,
        old_token,
        new_token,
        new_epoch,
    } = ack;
    let mut renewals = state.relay_renewals.lock().unwrap();
    let Some(renewal) = renewals.get_mut(&old_token) else {
        return;
    };
    let Phase::Staging { deadline } = renewal.phase else {
        return;
    };
    if now >= deadline
        || renewal.room_id != room
        || renewal.owner.game_epoch != game_epoch
        || renewal.new_token != new_token
        || renewal.new_epoch != new_epoch
        || !live(state, room, &renewal.owner)
    {
        return;
    }
    let Some(index) = renewal
        .owner
        .members
        .iter()
        .position(|(_, id, _)| *id == player)
    else {
        return;
    };
    let slot = renewal.owner.members[index].0;
    if !state
        .relay_sessions
        .get(&new_token)
        .is_some_and(|s| s.is_bound(slot, now) && now < s.expires_at())
    {
        return;
    }
    renewal.ready[index] = true;
    if !renewal.ready.iter().all(|v| *v) {
        return;
    }
    if !state
        .relay_sessions
        .get(&new_token)
        .is_some_and(|s| s.both_bound(now))
    {
        return;
    }
    let overlap = Duration::from_secs(
        (state.config.relay_policy.allocation_lifetime_seconds / 8).clamp(1, 3),
    );
    let Some(old) = state.relay_sessions.get(&old_token) else {
        return;
    };
    if old.epoch != renewal.old_epoch || now >= old.expires_at() {
        return;
    }
    let retire_at = old.expires_at().min(now + overlap);
    old.retire_at(retire_at);
    old.primary.store(false, Ordering::Relaxed);
    if let Some(new) = state.relay_sessions.get(&new_token) {
        new.primary.store(true, Ordering::Relaxed);
    }
    renewal.phase = Phase::Committed { retire_at };
    renewal.send(
        state,
        old_token,
        1,
        retire_at.saturating_duration_since(now).as_millis() as u64,
    );
}

pub fn tick(state: &ServerState, now: Instant) {
    let _control = state.control.lock().unwrap();
    let lifetime = Duration::from_secs(state.config.relay_policy.allocation_lifetime_seconds);
    let lead = Duration::from_secs((lifetime.as_secs() / 2).min(60));
    let timeout = Duration::from_secs((lead.as_secs() / 2).clamp(1, 15));
    let mut renewals = state.relay_renewals.lock().unwrap();
    renewals.retain(|old_token, renewal| {
        if !live(state, renewal.room_id, &renewal.owner) {
            state.relay_sessions.remove(old_token);
            state.relay_sessions.remove(&renewal.new_token);
            return false;
        }
        match renewal.phase {
            Phase::Staging { deadline } => {
                if now >= deadline
                    || !state.relay_sessions.contains_key(old_token)
                    || !state.relay_sessions.contains_key(&renewal.new_token)
                {
                    renewal.send(state, *old_token, 2, 0);
                    state.relay_sessions.remove(&renewal.new_token);
                    return false;
                }
            }
            Phase::Committed { retire_at } => {
                if now >= retire_at || !state.relay_sessions.contains_key(&renewal.new_token) {
                    state.relay_sessions.remove(old_token);
                    return false;
                }
            }
        }
        true
    });
    let allocations: Vec<_> = state
        .relay_sessions
        .iter()
        .map(|s| {
            (
                *s.key(),
                s.room_id,
                s.owner.clone(),
                s.expires_at(),
                s.primary.load(Ordering::Relaxed),
                s.created_at,
            )
        })
        .collect();
    for (token, room, owner, expires, primary, created) in allocations {
        if now >= expires {
            state.relay_sessions.remove(&token);
            continue;
        }
        let Some(owner) = owner else {
            continue;
        };
        if !live(state, room, &owner) {
            state.relay_sessions.remove(&token);
            continue;
        }
        if !primary
            || now < created + lifetime - lead
            || renewals.contains_key(&token)
            || renewals.len() >= 100
            || state.relay_sessions.len() >= 200
        {
            continue;
        }
        let old_epoch = {
            let Some(old) = state.relay_sessions.get(&token) else {
                continue;
            };
            if old.renewal_attempted.load(Ordering::Relaxed) {
                continue;
            }
            old.epoch
        };
        if !state.relay_economy.lock().unwrap().can_allocate(now) {
            continue;
        }
        let Some(new_token) = (0..16)
            .map(|_| Uuid::new_v4().as_u128() as u32)
            .find(|candidate| {
                !state.relay_sessions.contains_key(candidate)
                    && !renewals.contains_key(candidate)
                    && !renewals
                        .values()
                        .any(|renewal| renewal.new_token == *candidate)
            })
        else {
            continue;
        };
        if state
            .relay_sessions
            .get(&token)
            .is_none_or(|old| old.renewal_attempted.swap(true, Ordering::Relaxed))
        {
            continue;
        }
        let replacement = RelaySession::owned(room, owner.clone(), now, lifetime, false);
        let new_epoch = replacement.epoch;
        state.relay_sessions.insert(new_token, replacement);
        let renewal = Renewal {
            room_id: room,
            owner,
            old_epoch,
            new_epoch,
            new_token,
            ready: [false; 2],
            phase: Phase::Staging {
                deadline: expires.min(now + timeout),
            },
        };
        if renewal.send(
            state,
            token,
            0,
            expires
                .min(now + timeout)
                .saturating_duration_since(now)
                .as_millis() as u64,
        ) {
            renewals.insert(token, renewal);
        } else {
            renewal.send(state, token, 2, 0);
            state.relay_sessions.remove(&new_token);
        }
    }
}
