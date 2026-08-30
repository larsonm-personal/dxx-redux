use serde::Serialize;
use serde_json::Value as JsonValue;
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
    pub mission_status: Option<crate::protocol::MissionStatusReport>,
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
    /// Extensible game config (mission, mode, difficulty, level_num, etc.)
    /// Opaque JSON -- unknown fields preserved on passthrough.
    pub game_info: JsonValue,
    // -- runtime state updated by host during InGame via UPDATE_GAME_STATE --
    /// Current connected player count (from host C engine).
    pub runtime_player_count: Option<u8>,
    /// Current level number (from host C engine).
    pub runtime_level: Option<i32>,
    /// Engine game_status (NETSTAT_PLAYING=1, etc.).
    pub runtime_game_status: Option<u8>,
    /// Last time the host sent an UPDATE_GAME_STATE.
    pub last_state_update: Option<Instant>,
    /// Players currently going through late-join ICE (joiner player_ids).
    pub pending_late_joiners: HashSet<Uuid>,
}

impl Lobby {
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        host_player_id: Uuid,
        host_callsign: String,
        game: String,
        max_players: u8,
        code: Option<String>,
        verified_only: bool,
        game_info: JsonValue,
    ) -> Self {
        let host = LobbyPlayer {
            player_id: host_player_id,
            callsign: host_callsign.clone(),
            ready: false,
            candidates: Vec::new(),
            nat_type: None,
            connection_type: ConnectionType::Unknown,
            ping_ms: None,
            mission_status: None,
        };
        Self {
            id: Uuid::new_v4(),
            host_player_id,
            host_callsign,
            game,
            max_players,
            state: LobbyState::Waiting,
            players: vec![host],
            created_at: chrono::Utc::now(),
            created_at_instant: Instant::now(),
            code,
            verified_only,
            kicked_players: HashSet::new(),
            holepunch_started_at: None,
            game_info,
            runtime_player_count: None,
            runtime_level: None,
            runtime_game_status: None,
            last_state_update: None,
            pending_late_joiners: HashSet::new(),
        }
    }

    pub fn is_full(&self) -> bool {
        self.players.len() >= self.max_players as usize
    }

    pub fn is_joinable(&self) -> bool {
        if self.is_full() {
            return false;
        }
        match self.state {
            LobbyState::Waiting => true,
            LobbyState::InGame => {
                // Joinable mid-game if host is sending fresh state updates
                // (not stale >60s) and engine reports NETSTAT_PLAYING.
                const STALE_SECS: u64 = 60;
                const NETSTAT_PLAYING: u8 = 1;
                self.runtime_game_status == Some(NETSTAT_PLAYING)
                    && self
                        .last_state_update
                        .map(|t| t.elapsed().as_secs() < STALE_SECS)
                        .unwrap_or(false)
            }
            _ => false,
        }
    }

    /// Check if a player is allowed to join (not kicked).
    pub fn can_join(&self, player_id: &Uuid) -> bool {
        self.is_joinable() && !self.kicked_players.contains(player_id)
    }

    pub fn add_player(&mut self, player_id: Uuid, callsign: String) -> bool {
        if self.is_full() {
            return false;
        }
        match self.state {
            LobbyState::Waiting | LobbyState::InGame => {}
            _ => return false,
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
            mission_status: None,
        });
        true
    }

    pub fn remove_player(&mut self, player_id: &Uuid) {
        self.players.retain(|p| &p.player_id != player_id);
        self.pending_late_joiners.remove(player_id);
    }

    pub fn player_count(&self) -> u8 {
        self.players.len() as u8
    }
}

/// Extract a string field from a game_info JSON value.
pub fn game_info_str(game_info: &serde_json::Value, key: &str) -> String {
    game_info
        .get(key)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string()
}
