use serde::Serialize;
use std::collections::HashSet;
use std::time::Instant;
use uuid::Uuid;

/// Possible lobby states.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum LobbyState {
    Waiting,
    Holepunching,
    Starting,
    InGame,
}

/// Presence information for a connected player.
#[derive(Debug, Clone)]
pub enum Presence {
    Offline,
    Online,
    InLobby {
        lobby_id: Uuid,
    },
    InGame {
        lobby_id: Uuid,
        mission: String,
        player_count: u8,
    },
}

impl Presence {
    pub fn as_str(&self) -> &str {
        match self {
            Presence::Offline => "offline",
            Presence::Online => "online",
            Presence::InLobby { .. } => "in_lobby",
            Presence::InGame { .. } => "in_game",
        }
    }
}

/// A player within a lobby.
#[derive(Debug, Clone, Serialize)]
pub struct LobbyPlayer {
    pub player_id: Uuid,
    pub callsign: String,
    pub ready: bool,
    pub candidates: Vec<crate::protocol::ConnectionCandidate>,
    pub nat_type: Option<String>,
    pub connection_type: ConnectionType,
    pub ping_ms: Option<u32>,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub enum ConnectionType {
    Unknown,
    DirectLan,
    DirectUpnp,
    DirectHolepunch,
    PredictedHolepunch,
    Relay,
}

impl ConnectionType {
    pub fn as_str(&self) -> &str {
        match self {
            ConnectionType::Unknown => "unknown",
            ConnectionType::DirectLan => "direct_lan",
            ConnectionType::DirectUpnp => "direct_upnp",
            ConnectionType::DirectHolepunch => "direct_holepunch",
            ConnectionType::PredictedHolepunch => "predicted_holepunch",
            ConnectionType::Relay => "relay",
        }
    }

    pub fn is_server_relayed(&self) -> bool {
        matches!(self, ConnectionType::Relay)
    }
}

/// A lobby (game room) managed by the server.
#[derive(Debug, Clone)]
pub struct Lobby {
    pub id: Uuid,
    pub host_player_id: Uuid,
    pub host_callsign: String,
    pub game: String,
    pub mission: String,
    pub mode: String,
    pub max_players: u8,
    pub state: LobbyState,
    pub players: Vec<LobbyPlayer>,
    pub created_at: chrono::DateTime<chrono::Utc>,
    /// Monotonic creation time for staleness checks.
    pub created_at_instant: Instant,
    /// Optional lobby code for invite-only sessions.
    pub code: Option<String>,
    /// If true, only GPGS-verified players can join.
    pub verified_only: bool,
    /// Players kicked from this lobby (cannot rejoin).
    pub kicked_players: HashSet<Uuid>,
    /// When the lobby entered Holepunching state (for timeout).
    pub holepunch_started_at: Option<Instant>,
}

impl Lobby {
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        host_player_id: Uuid,
        host_callsign: String,
        game: String,
        mission: String,
        mode: String,
        max_players: u8,
        code: Option<String>,
        verified_only: bool,
    ) -> Self {
        let host = LobbyPlayer {
            player_id: host_player_id,
            callsign: host_callsign.clone(),
            ready: false,
            candidates: Vec::new(),
            nat_type: None,
            connection_type: ConnectionType::Unknown,
            ping_ms: None,
        };
        Self {
            id: Uuid::new_v4(),
            host_player_id,
            host_callsign,
            game,
            mission,
            mode,
            max_players,
            state: LobbyState::Waiting,
            players: vec![host],
            created_at: chrono::Utc::now(),
            created_at_instant: Instant::now(),
            code,
            verified_only,
            kicked_players: HashSet::new(),
            holepunch_started_at: None,
        }
    }

    pub fn is_full(&self) -> bool {
        self.players.len() >= self.max_players as usize
    }

    pub fn is_joinable(&self) -> bool {
        self.state == LobbyState::Waiting && !self.is_full()
    }

    /// Check if a player is allowed to join (not kicked).
    pub fn can_join(&self, player_id: &Uuid) -> bool {
        self.is_joinable() && !self.kicked_players.contains(player_id)
    }

    pub fn add_player(&mut self, player_id: Uuid, callsign: String) -> bool {
        if self.is_full() || self.state != LobbyState::Waiting {
            return false;
        }
        if self.players.iter().any(|p| p.player_id == player_id) {
            return false; // already in lobby
        }
        self.players.push(LobbyPlayer {
            player_id,
            callsign,
            ready: false,
            candidates: Vec::new(),
            nat_type: None,
            connection_type: ConnectionType::Unknown,
            ping_ms: None,
        });
        true
    }

    pub fn remove_player(&mut self, player_id: &Uuid) {
        self.players.retain(|p| &p.player_id != player_id);
    }

    pub fn player_count(&self) -> u8 {
        self.players.len() as u8
    }
}
