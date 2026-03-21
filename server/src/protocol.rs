use serde::{Deserialize, Serialize};
use serde_json::Value as JsonValue;
use serde_json::Value as JsonValue;
use uuid::Uuid;

// -- Protocol version constants --
// Bump CURRENT_PROTOCOL when making breaking changes to the WebSocket
// message schema. Keep MIN_CLIENT_PROTOCOL at the oldest version the
// server still accepts. Both values must stay in sync with the client
// constant in NetworkConstants.kt.
pub const CURRENT_PROTOCOL: u32 = 1;
pub const MIN_CLIENT_PROTOCOL: u32 = 1;

/// Maximum serialized size of a game_info JSON object (5 KB).
pub const GAME_INFO_MAX_BYTES: usize = 5 * 1024;

/// Maximum serialized size of a game_info JSON object (5 KB).
pub const GAME_INFO_MAX_BYTES: usize = 5 * 1024;

// -- Client -> Server messages --

#[derive(Debug, Deserialize)]
#[serde(tag = "type")]
#[allow(dead_code)] // fields read via serde deserialization
pub enum ClientMessage {
    #[serde(rename = "AUTHENTICATE")]
    Authenticate {
        protocol_version: u32,
        client_version: String,
        #[serde(default)]
        play_games_token: Option<String>,
        callsign: String,
        platform: String,
        /// "gpgs" (default) or "keypair"
        #[serde(default = "default_auth_method")]
        auth_method: String,
        /// Ed25519 public key (64 hex chars), required for keypair auth
        #[serde(default)]
        public_key: Option<String>,
        /// Unix timestamp (seconds) used in signature, required for keypair auth
        #[serde(default)]
        auth_timestamp: Option<u64>,
        /// Ed25519 signature of "callsign:timestamp" (128 hex chars)
        #[serde(default)]
        auth_signature: Option<String>,
    },

    #[serde(rename = "CREATE_LOBBY")]
    CreateLobby {
        game: String,
        max_players: u8,
        #[serde(default)]
        lobby_code: Option<String>,
        #[serde(default)]
        verified_only: bool,
        /// Extensible game config (mission, mode, difficulty, level_num, etc.)
        /// Max 5KB serialized. Unknown fields are preserved on passthrough.
        #[serde(default = "default_game_info")]
        game_info: JsonValue,
    },

    #[serde(rename = "UPDATE_GAME_INFO")]
    UpdateGameInfo {
        /// Updated game config -- replaces game_info in the lobby.
        game_info: JsonValue,
    },

    /// Host sends periodic in-game state updates so the server can track
    /// in-progress games and allow mid-game joins.
    #[serde(rename = "UPDATE_GAME_STATE")]
    UpdateGameState {
        player_count: u8,
        max_players: u8,
        current_level: i32,
        game_status: u8,
    },

    /// Host signals that the game engine has exited and the lobby should
    /// return to the Waiting state so players can re-ready and start again.
    #[serde(rename = "END_GAME")]
    EndGame {},

    #[serde(rename = "LIST_LOBBIES")]
    ListLobbies {},

    #[serde(rename = "JOIN_LOBBY")]
    JoinLobby {
        lobby_id: Uuid,
        #[serde(default)]
        lobby_code: Option<String>,
    },

    #[serde(rename = "LEAVE_LOBBY")]
    LeaveLobby {},

    #[serde(rename = "READY")]
    Ready { ready: bool },

    #[serde(rename = "START_GAME")]
    StartGame {},

    #[serde(rename = "STUN_RESULT")]
    StunResult {
        candidates: Vec<ConnectionCandidate>,
        nat_type: String, // "full_cone", "address_restricted", "port_restricted_cone",
                          // "symmetric", "symmetric_sequential", "unknown"
    },

    #[serde(rename = "CONNECTIVITY_OK")]
    ConnectivityOk {
        peer_id: Uuid,
        winning_candidate_type: String,
        rtt_ms: u32,
    },

    #[serde(rename = "CONNECTIVITY_UPDATE")]
    ConnectivityUpdate {
        peer_id: Uuid,
        new_method: String,
        detail: Option<String>,
    },

    #[serde(rename = "FILE_HASHES")]
    FileHashes { hashes: Vec<FileHashEntry> },

    #[serde(rename = "KICK_PLAYER")]
    KickPlayer { player_id: Uuid },

    #[serde(rename = "MATCH_RESULT")]
    MatchResult {
        lobby_id: Uuid,
        duration_secs: u32,
        result: String,
        players: Vec<MatchPlayerResult>,
    },

    // -- Friends --
    #[serde(rename = "FRIEND_REQUEST")]
    FriendRequest { target_callsign: String },

    #[serde(rename = "FRIEND_ACCEPT")]
    FriendAccept { player_id: Uuid },

    #[serde(rename = "FRIEND_REMOVE")]
    FriendRemove { player_id: Uuid },

    #[serde(rename = "FRIEND_BLOCK")]
    FriendBlock { player_id: Uuid },

    #[serde(rename = "FRIEND_LIST")]
    FriendList {},

    #[serde(rename = "JOIN_FRIEND_GAME")]
    JoinFriendGame { friend_player_id: Uuid },

    #[serde(rename = "SEND_MESSAGE")]
    SendMessage {
        target_player_id: Uuid,
        text: String,
    },

    #[serde(rename = "POW_SOLUTION")]
    PowSolution { challenge: String, solution: String },
}

fn default_auth_method() -> String {
    "gpgs".into()
}

fn default_game_info() -> JsonValue {
    serde_json::json!({})
}

fn default_game_info() -> JsonValue {
    serde_json::json!({})
}

// -- Server -> Client messages --

#[derive(Debug, Serialize, Clone)]
#[serde(tag = "type")]
pub enum ServerMessage {
    #[serde(rename = "AUTH_OK")]
    AuthOk {
        player_id: Uuid,
        session_token: String,
        /// Self-hosted STUN server addresses for NAT discovery (e.g. ["1.2.3.4:3478", "1.2.3.4:3479"])
        #[serde(skip_serializing_if = "Vec::is_empty")]
        stun_addrs: Vec<String>,
    },

    #[serde(rename = "AUTH_FAIL")]
    AuthFail { reason: String },

    #[serde(rename = "POW_CHALLENGE")]
    PowChallenge { challenge: String, difficulty: u8 },

    #[serde(rename = "VERSION_REJECTED")]
    VersionRejected {
        reason: String,
        required_version: u32,
        required_version_name: String,
        current_server_version: u32,
        update_url: String,
    },

    #[serde(rename = "VERSION_WARNING")]
    VersionWarning {
        message: String,
        update_url: String,
        deadline: Option<String>,
    },

    #[serde(rename = "MOTD")]
    Motd {
        message: String,
        url: Option<String>,
        severity: String,
    },

    #[serde(rename = "LOBBY_LIST")]
    LobbyList { lobbies: Vec<LobbyInfo> },

    #[serde(rename = "LOBBY_UPDATE")]
    LobbyUpdate {
        lobby_id: Uuid,
        players: Vec<LobbyPlayerInfo>,
    },

    #[serde(rename = "PEER_CANDIDATES")]
    PeerCandidates {
        peer_id: Uuid,
        candidates: Vec<ConnectionCandidate>,
        nat_type: String,
    },

    #[serde(rename = "CONNECTIVITY_CHECK_GO")]
    ConnectivityCheckGo { peer_addrs: Vec<CandidatePair> },

    #[serde(rename = "GAME_STARTING")]
    GameStarting {
        host_addr: String,
        game: String,
        your_slot: u8,
        max_players: u8,
        /// Extensible game config from the lobby (mission, mode, difficulty, etc.)
        game_info: JsonValue,
        /// Extensible game config from the lobby (mission, mode, difficulty, etc.)
        game_info: JsonValue,
        peers: Vec<PeerAssignment>,
    },

    #[serde(rename = "RELAY_ASSIGNED")]
    RelayAssigned {
        relay_addr: String,
        session_token: String,
    },

    #[serde(rename = "ERROR")]
    Error { code: String, message: String },

    #[serde(rename = "RATE_LIMITED")]
    RateLimited { retry_after_ms: u64 },

    #[serde(rename = "MATCH_RESULT_ACK")]
    MatchResultAck { match_id: Uuid },

    // -- Friends --
    #[serde(rename = "FRIEND_LIST_RESP")]
    FriendListResp { friends: Vec<FriendInfo> },

    #[serde(rename = "FRIEND_REQUEST_RECEIVED")]
    FriendRequestReceived {
        from_player_id: Uuid,
        from_callsign: String,
    },

    #[serde(rename = "FRIEND_ACCEPTED")]
    FriendAccepted { player_id: Uuid },

    #[serde(rename = "FRIEND_REMOVED")]
    FriendRemoved { player_id: Uuid },

    #[serde(rename = "FRIEND_PRESENCE_UPDATE")]
    FriendPresenceUpdate {
        player_id: Uuid,
        presence: String,
        details: Option<InGameDetails>,
    },

    #[serde(rename = "JOIN_FRIEND_GAME_RESP")]
    JoinFriendGameResp {
        success: bool,
        reason: Option<String>,
        lobby_id: Option<Uuid>,
    },

    #[serde(rename = "CONNECTION_INFO")]
    ConnectionInfo {
        connections: Vec<PeerConnectionInfo>,
    },

    #[serde(rename = "MAINTENANCE")]
    Maintenance {
        message: String,
        estimated_return: Option<String>,
    },

    #[serde(rename = "SERVER_STATUS")]
    ServerStatus {
        online_players: u32,
        active_games_count: u32,
        active_game_list: Vec<ActiveGameInfo>,
        total_games_played: u64,
    },

    #[serde(rename = "MESSAGE_RECEIVED")]
    MessageReceived {
        from_player_id: Uuid,
        from_callsign: String,
        text: String,
    },

    #[serde(rename = "MESSAGE_SENT")]
    MessageSent { target_player_id: Uuid },

    #[serde(rename = "MAINTENANCE_WARNING")]
    MaintenanceWarning {
        message: String,
        shutdown_at: String,
    },

    // -- Late-join (mid-game) ICE messages --

    /// Sent to host when a joiner needs NAT holepunching.
    /// Host should send blind probes from shared socket and enable probe echo.
    #[serde(rename = "LATE_JOIN_PROBE")]
    LateJoinProbe {
        joiner_id: Uuid,
        joiner_callsign: String,
        /// Addresses to send blind probes to (opens NAT pinholes from shared socket).
        probe_addrs: Vec<String>,
    },

    /// Sent to host after late-join ICE completes. Host adds joiner to proxy.
    #[serde(rename = "LATE_JOIN_APPROVED")]
    LateJoinApproved { peer: PeerAssignment },

    // -- Late-join (mid-game) ICE messages --

    /// Sent to host when a joiner needs NAT holepunching.
    /// Host should send blind probes from shared socket and enable probe echo.
    #[serde(rename = "LATE_JOIN_PROBE")]
    LateJoinProbe {
        joiner_id: Uuid,
        joiner_callsign: String,
        /// Addresses to send blind probes to (opens NAT pinholes from shared socket).
        probe_addrs: Vec<String>,
    },

    /// Sent to host after late-join ICE completes. Host adds joiner to proxy.
    #[serde(rename = "LATE_JOIN_APPROVED")]
    LateJoinApproved { peer: PeerAssignment },
}

// -- Supporting types --

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct FileHashEntry {
    pub name: String,
    pub sha256: String,
    pub size: u64,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct MatchPlayerResult {
    pub player_id: Uuid,
    pub score: i32,
    pub kills: u32,
    pub deaths: u32,
    pub result: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct LobbyInfo {
    pub lobby_id: Uuid,
    pub host_callsign: String,
    pub game: String,
    pub player_count: u8,
    pub max_players: u8,
    pub joinable: bool,
    pub host_ping_ms: Option<u32>,
    pub has_code: bool,
    pub verified_only: bool,
    /// Extensible game config (mission, mode, difficulty, level_num, etc.)
    pub game_info: JsonValue,
    /// "waiting" or "in_progress"
    pub lobby_state: String,
    /// Current level number reported by host (only for in_progress)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub current_level: Option<i32>,
    /// Extensible game config (mission, mode, difficulty, level_num, etc.)
    pub game_info: JsonValue,
    /// "waiting" or "in_progress"
    pub lobby_state: String,
    /// Current level number reported by host (only for in_progress)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub current_level: Option<i32>,
}

#[derive(Debug, Serialize, Clone)]
pub struct LobbyPlayerInfo {
    pub player_id: Uuid,
    pub callsign: String,
    pub ready: bool,
    pub ping_ms: Option<u32>,
    pub connection_type: String,
}

#[derive(Debug, Serialize, Clone)]
pub struct FriendInfo {
    pub player_id: Uuid,
    pub callsign: String,
    pub status: String,
    pub presence: String,
    pub in_game_details: Option<InGameDetails>,
    pub last_seen: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ConnectionCandidate {
    pub candidate_type: String, // "host", "srflx", "upnp", "predicted", "relay"
    pub addr: String,           // "ip:port"
}

#[derive(Debug, Serialize, Clone)]
pub struct CandidatePair {
    pub peer_id: Uuid,
    pub local_type: String,
    pub remote_type: String,
    pub remote_addr: String,
    pub priority: u32,
}

#[derive(Debug, Serialize, Clone)]
pub struct PeerConnectionInfo {
    pub peer_id: Uuid,
    pub peer_callsign: String,
    pub method: String,
    pub detail: Option<String>,
    pub server_relay: bool,
    pub estimated_latency_ms: Option<u32>,
}

/// Per-peer connection assignment sent in GAME_STARTING.
#[derive(Debug, Serialize, Clone)]
pub struct PeerAssignment {
    pub slot: u8,
    pub addr: String,
    pub is_relay: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub relay_token: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub relay_dest_slot: Option<u8>,
}

#[derive(Debug, Serialize, Clone)]
pub struct InGameDetails {
    pub lobby_id: Uuid,
    pub mission: String,
    pub player_count: u8,
    pub max_players: u8,
    pub joinable: bool,
}

#[derive(Debug, Serialize, Clone)]
pub struct ActiveGameInfo {
    pub lobby_id: Uuid,
    pub host_callsign: String,
    pub mission: String,
    pub mode: String,
    pub player_count: u8,
    pub duration_secs: u64,
}
