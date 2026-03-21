package com.dxxredux.app.multiplayer

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive

// JSON parser configured to ignore unknown fields (forward-compat with server additions)
val protocolJson =
    Json {
        ignoreUnknownKeys = true
        encodeDefaults = true
    }

// -- Client -> Server messages --
// Each is serialized independently with a "type" field via @SerialName.

@Serializable
data class AuthenticateMsg(
    val type: String = "AUTHENTICATE",
    @SerialName("protocol_version") val protocolVersion: Int = NetworkConstants.PROTOCOL_VERSION,
    @SerialName("client_version") val clientVersion: String = NetworkConstants.CLIENT_VERSION,
    @SerialName("play_games_token") val playGamesToken: String? = null,
    val callsign: String,
    val platform: String = NetworkConstants.PLATFORM,
    @SerialName("auth_method") val authMethod: String = "gpgs",
)

@Serializable
data class ListLobbiesMsg(
    val type: String = "LIST_LOBBIES",
)

@Serializable
data class CreateLobbyMsg(
    val type: String = "CREATE_LOBBY",
    val game: String,
    @SerialName("max_players") val maxPlayers: Int,
    @SerialName("lobby_code") val lobbyCode: String? = null,
    @SerialName("verified_only") val verifiedOnly: Boolean = false,
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
)

@Serializable
data class JoinLobbyMsg(
    val type: String = "JOIN_LOBBY",
    @SerialName("lobby_id") val lobbyId: String,
    @SerialName("lobby_code") val lobbyCode: String? = null,
)

@Serializable
data class LeaveLobbyMsg(
    val type: String = "LEAVE_LOBBY",
)

@Serializable
data class ReadyMsg(
    val type: String = "READY",
    val ready: Boolean,
)

@Serializable
data class StartGameMsg(
    val type: String = "START_GAME",
)

@Serializable
data class EndGameMsg(
    val type: String = "END_GAME",
)

@Serializable
data class UpdateGameInfoMsg(
    val type: String = "UPDATE_GAME_INFO",
    @SerialName("game_info") val gameInfo: JsonObject,
)

@Serializable
data class KickPlayerMsg(
    val type: String = "KICK_PLAYER",
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class SendMessageMsg(
    val type: String = "SEND_MESSAGE",
    @SerialName("target_player_id") val targetPlayerId: String,
    val text: String,
)

// -- Client -> Server: friend messages --

@Serializable
data class FriendRequestMsg(
    val type: String = "FRIEND_REQUEST",
    @SerialName("target_callsign") val targetCallsign: String,
)

@Serializable
data class FriendAcceptMsg(
    val type: String = "FRIEND_ACCEPT",
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class FriendRemoveMsg(
    val type: String = "FRIEND_REMOVE",
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class FriendBlockMsg(
    val type: String = "FRIEND_BLOCK",
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class FriendListRequestMsg(
    val type: String = "FRIEND_LIST",
)

@Serializable
data class JoinFriendGameMsg(
    val type: String = "JOIN_FRIEND_GAME",
    @SerialName("friend_player_id") val friendPlayerId: String,
)

// -- Client -> Server: NAT traversal messages --

@Serializable
data class StunResultMsg(
    val type: String = "STUN_RESULT",
    val candidates: List<ConnectionCandidate>,
    @SerialName("nat_type") val natType: String,
)

@Serializable
data class ConnectionCandidate(
    @SerialName("candidate_type") val candidateType: String, // "host", "srflx", "upnp", "predicted"
    val addr: String, // "ip:port"
)

@Serializable
data class ConnectivityOkMsg(
    val type: String = "CONNECTIVITY_OK",
    @SerialName("peer_id") val peerId: String,
    @SerialName("winning_candidate_type") val winningCandidateType: String,
    @SerialName("rtt_ms") val rttMs: Int,
)

@Serializable
data class ConnectivityUpdateMsg(
    val type: String = "CONNECTIVITY_UPDATE",
    @SerialName("peer_id") val peerId: String,
    @SerialName("new_method") val newMethod: String,
    val detail: String? = null,
)

// -- Server -> Client parsed types --

@Serializable
data class AuthOk(
    @SerialName("player_id") val playerId: String,
    @SerialName("session_token") val sessionToken: String,
    @SerialName("stun_addrs") val stunAddrs: List<String> = emptyList(),
)

@Serializable
data class AuthFail(
    val reason: String,
)

@Serializable
data class PowChallenge(
    val challenge: String,
    val difficulty: Int,
)

@Serializable
data class ServerError(
    val code: String,
    val message: String,
)

@Serializable
data class Motd(
    val message: String,
    val url: String? = null,
    val severity: String = "info",
)

@Serializable
data class LobbyInfo(
    @SerialName("lobby_id") val lobbyId: String,
    @SerialName("host_callsign") val hostCallsign: String,
    val game: String,
    @SerialName("player_count") val playerCount: Int,
    @SerialName("max_players") val maxPlayers: Int,
    val joinable: Boolean,
    @SerialName("host_ping_ms") val hostPingMs: Int? = null,
    @SerialName("has_code") val hasCode: Boolean = false,
    @SerialName("verified_only") val verifiedOnly: Boolean = false,
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
    // "waiting" or "in_progress"
    @SerialName("lobby_state") val lobbyState: String = "waiting",
    @SerialName("current_level") val currentLevel: Int? = null,
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
    // "waiting" or "in_progress"
    @SerialName("lobby_state") val lobbyState: String = "waiting",
    @SerialName("current_level") val currentLevel: Int? = null,
)

@Serializable
data class LobbyListMsg(
    val lobbies: List<LobbyInfo>,
)

@Serializable
data class LobbyPlayerInfo(
    @SerialName("player_id") val playerId: String,
    val callsign: String,
    val ready: Boolean,
    @SerialName("ping_ms") val pingMs: Int? = null,
    @SerialName("connection_type") val connectionType: String = "unknown",
)

@Serializable
data class LobbyUpdateMsg(
    @SerialName("lobby_id") val lobbyId: String,
    val players: List<LobbyPlayerInfo>,
)

@Serializable
data class ServerStatusMsg(
    @SerialName("online_players") val onlinePlayers: Int,
    @SerialName("active_games_count") val activeGamesCount: Int,
    @SerialName("active_game_list") val activeGameList: List<ActiveGameInfo> = emptyList(),
    @SerialName("total_games_played") val totalGamesPlayed: Long = 0,
)

@Serializable
data class ActiveGameInfo(
    @SerialName("lobby_id") val lobbyId: String = "",
    @SerialName("host_callsign") val hostCallsign: String,
    val mission: String,
    val mode: String,
    @SerialName("player_count") val playerCount: Int,
    @SerialName("duration_secs") val durationSecs: Long,
)

@Serializable
data class GameStartingMsg(
    @SerialName("host_addr") val hostAddr: String,
    val game: String,
    @SerialName("your_slot") val yourSlot: Int = 0,
    @SerialName("max_players") val maxPlayers: Int = 4,
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
    @SerialName("game_info") val gameInfo: JsonObject = JsonObject(emptyMap()),
    val peers: List<PeerAssignment> = emptyList(),
)

@Serializable
data class PeerAssignment(
    val slot: Int,
    val addr: String,
    @SerialName("is_relay") val isRelay: Boolean = false,
    @SerialName("relay_token") val relayToken: Long? = null,
    @SerialName("relay_dest_slot") val relayDestSlot: Int? = null,
)

@Serializable
data class RateLimitedMsg(
    @SerialName("retry_after_ms") val retryAfterMs: Long,
)

@Serializable
data class VersionRejectedMsg(
    val reason: String,
    @SerialName("required_version") val requiredVersion: Int,
    @SerialName("required_version_name") val requiredVersionName: String,
    @SerialName("current_server_version") val currentServerVersion: Int,
    @SerialName("update_url") val updateUrl: String,
)

@Serializable
data class MessageReceivedMsg(
    @SerialName("from_player_id") val fromPlayerId: String,
    @SerialName("from_callsign") val fromCallsign: String,
    val text: String,
)

@Serializable
data class MessageSentMsg(
    @SerialName("target_player_id") val targetPlayerId: String,
)

@Serializable
data class PeerConnectionInfoMsg(
    @SerialName("peer_id") val peerId: String,
    @SerialName("peer_callsign") val peerCallsign: String,
    val method: String,
    val detail: String? = null,
    @SerialName("server_relay") val serverRelay: Boolean = false,
    @SerialName("estimated_latency_ms") val estimatedLatencyMs: Int? = null,
)

@Serializable
data class ConnectionInfoMsg(
    val connections: List<PeerConnectionInfoMsg>,
)

// -- Server -> Client: friend messages --

@Serializable
data class FriendInfo(
    @SerialName("player_id") val playerId: String,
    val callsign: String,
    val status: String, // "pending", "accepted", "blocked"
    val presence: String, // "online", "offline", "in_game"
    @SerialName("in_game_details") val inGameDetails: InGameDetails? = null,
    @SerialName("last_seen") val lastSeen: String? = null,
)

@Serializable
data class InGameDetails(
    @SerialName("lobby_id") val lobbyId: String,
    val mission: String,
    @SerialName("player_count") val playerCount: Int,
    @SerialName("max_players") val maxPlayers: Int,
    val joinable: Boolean,
)

@Serializable
data class FriendListRespMsg(
    val friends: List<FriendInfo>,
)

@Serializable
data class FriendRequestReceivedMsg(
    @SerialName("from_player_id") val fromPlayerId: String,
    @SerialName("from_callsign") val fromCallsign: String,
)

@Serializable
data class FriendAcceptedMsg(
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class FriendRemovedMsg(
    @SerialName("player_id") val playerId: String,
)

@Serializable
data class FriendPresenceUpdateMsg(
    @SerialName("player_id") val playerId: String,
    val presence: String,
    val details: InGameDetails? = null,
)

@Serializable
data class JoinFriendGameRespMsg(
    val success: Boolean,
    val reason: String? = null,
    @SerialName("lobby_id") val lobbyId: String? = null,
)

// -- Server -> Client: NAT traversal messages --

@Serializable
data class PeerCandidatesMsg(
    @SerialName("peer_id") val peerId: String,
    val candidates: List<ConnectionCandidate>,
    @SerialName("nat_type") val natType: String,
)

@Serializable
data class CandidatePair(
    @SerialName("peer_id") val peerId: String,
    @SerialName("local_type") val localType: String,
    @SerialName("remote_type") val remoteType: String,
    @SerialName("remote_addr") val remoteAddr: String,
    val priority: Int,
)

@Serializable
data class ConnectivityCheckGoMsg(
    @SerialName("peer_addrs") val peerAddrs: List<CandidatePair>,
)

@Serializable
data class RelayAssignedMsg(
    @SerialName("relay_addr") val relayAddr: String,
    @SerialName("session_token") val sessionToken: String,
)

@Serializable
data class MaintenanceMsg(
    val message: String,
    @SerialName("estimated_return") val estimatedReturn: String? = null,
)

@Serializable
data class MaintenanceWarningMsg(
    val message: String,
    @SerialName("shutdown_at") val shutdownAt: String? = null,
)

@Serializable
data class LateJoinProbeMsg(
    @SerialName("joiner_id") val joinerId: String,
    @SerialName("joiner_callsign") val joinerCallsign: String,
    @SerialName("probe_addrs") val probeAddrs: List<String>,
)

@Serializable
data class LateJoinApprovedMsg(
    val peer: PeerAssignment,
)

@Serializable
data class LateJoinProbeMsg(
    @SerialName("joiner_id") val joinerId: String,
    @SerialName("joiner_callsign") val joinerCallsign: String,
    @SerialName("probe_addrs") val probeAddrs: List<String>,
)

@Serializable
data class LateJoinApprovedMsg(
    val peer: PeerAssignment,
)

// Sealed class representing any server message, dispatched by "type" field
sealed class ServerMessage {
    data class AuthOkMsg(
        val data: AuthOk,
    ) : ServerMessage()

    data class AuthFailMsg(
        val data: AuthFail,
    ) : ServerMessage()

    data class PowChallengeMsg(
        val data: PowChallenge,
    ) : ServerMessage()

    data class ErrorMsg(
        val data: ServerError,
    ) : ServerMessage()

    data class MotdMsg(
        val data: Motd,
    ) : ServerMessage()

    data class LobbyListReceived(
        val data: LobbyListMsg,
    ) : ServerMessage()

    data class LobbyUpdated(
        val data: LobbyUpdateMsg,
    ) : ServerMessage()

    data class ServerStatusReceived(
        val data: ServerStatusMsg,
    ) : ServerMessage()

    data class GameStarting(
        val data: GameStartingMsg,
    ) : ServerMessage()

    data class RateLimited(
        val data: RateLimitedMsg,
    ) : ServerMessage()

    data class VersionRejected(
        val data: VersionRejectedMsg,
    ) : ServerMessage()

    data class MessageReceived(
        val data: MessageReceivedMsg,
    ) : ServerMessage()

    data class MessageSent(
        val data: MessageSentMsg,
    ) : ServerMessage()

    data class ConnectionInfoReceived(
        val data: ConnectionInfoMsg,
    ) : ServerMessage()

    data class PeerCandidatesReceived(
        val data: PeerCandidatesMsg,
    ) : ServerMessage()

    data class ConnectivityCheckGoReceived(
        val data: ConnectivityCheckGoMsg,
    ) : ServerMessage()

    data class RelayAssignedReceived(
        val data: RelayAssignedMsg,
    ) : ServerMessage()

    data class MaintenanceReceived(
        val data: MaintenanceMsg,
    ) : ServerMessage()

    data class MaintenanceWarningReceived(
        val data: MaintenanceWarningMsg,
    ) : ServerMessage()

    data class FriendListReceived(
        val data: FriendListRespMsg,
    ) : ServerMessage()

    data class FriendRequestReceived(
        val data: FriendRequestReceivedMsg,
    ) : ServerMessage()

    data class FriendAccepted(
        val data: FriendAcceptedMsg,
    ) : ServerMessage()

    data class FriendRemoved(
        val data: FriendRemovedMsg,
    ) : ServerMessage()

    data class FriendPresenceUpdated(
        val data: FriendPresenceUpdateMsg,
    ) : ServerMessage()

    data class JoinFriendGameResponse(
        val data: JoinFriendGameRespMsg,
    ) : ServerMessage()

    data class LateJoinProbeReceived(
        val data: LateJoinProbeMsg,
    ) : ServerMessage()

    data class LateJoinApprovedReceived(
        val data: LateJoinApprovedMsg,
    ) : ServerMessage()

    data class LateJoinProbeReceived(
        val data: LateJoinProbeMsg,
    ) : ServerMessage()

    data class LateJoinApprovedReceived(
        val data: LateJoinApprovedMsg,
    ) : ServerMessage()

    data class Unknown(
        val type: String,
        val raw: String,
    ) : ServerMessage()

    companion object {
        fun parse(text: String): ServerMessage {
            val obj = protocolJson.decodeFromString<JsonObject>(text)
            val type = obj["type"]?.jsonPrimitive?.content ?: return Unknown("missing", text)
            return when (type) {
                "AUTH_OK" -> AuthOkMsg(protocolJson.decodeFromString<AuthOk>(text))
                "AUTH_FAIL" -> AuthFailMsg(protocolJson.decodeFromString<AuthFail>(text))
                "POW_CHALLENGE" -> PowChallengeMsg(protocolJson.decodeFromString<PowChallenge>(text))
                "ERROR" -> ErrorMsg(protocolJson.decodeFromString<ServerError>(text))
                "MOTD" -> MotdMsg(protocolJson.decodeFromString<Motd>(text))
                "LOBBY_LIST" -> LobbyListReceived(protocolJson.decodeFromString<LobbyListMsg>(text))
                "LOBBY_UPDATE" -> LobbyUpdated(protocolJson.decodeFromString<LobbyUpdateMsg>(text))
                "SERVER_STATUS" -> ServerStatusReceived(protocolJson.decodeFromString<ServerStatusMsg>(text))
                "GAME_STARTING" -> GameStarting(protocolJson.decodeFromString<GameStartingMsg>(text))
                "RATE_LIMITED" -> RateLimited(protocolJson.decodeFromString<RateLimitedMsg>(text))
                "VERSION_REJECTED" -> VersionRejected(protocolJson.decodeFromString<VersionRejectedMsg>(text))
                "MESSAGE_RECEIVED" -> MessageReceived(protocolJson.decodeFromString<MessageReceivedMsg>(text))
                "MESSAGE_SENT" -> MessageSent(protocolJson.decodeFromString<MessageSentMsg>(text))
                "CONNECTION_INFO" -> ConnectionInfoReceived(protocolJson.decodeFromString<ConnectionInfoMsg>(text))
                "PEER_CANDIDATES" -> PeerCandidatesReceived(protocolJson.decodeFromString<PeerCandidatesMsg>(text))
                "CONNECTIVITY_CHECK_GO" ->
                    ConnectivityCheckGoReceived(
                        protocolJson.decodeFromString<ConnectivityCheckGoMsg>(text),
                    )
                "RELAY_ASSIGNED" -> RelayAssignedReceived(protocolJson.decodeFromString<RelayAssignedMsg>(text))
                "MAINTENANCE" -> MaintenanceReceived(protocolJson.decodeFromString<MaintenanceMsg>(text))
                "MAINTENANCE_WARNING" ->
                    MaintenanceWarningReceived(
                        protocolJson.decodeFromString<MaintenanceWarningMsg>(text),
                    )
                "FRIEND_LIST_RESP" ->
                    FriendListReceived(protocolJson.decodeFromString<FriendListRespMsg>(text))
                "FRIEND_REQUEST_RECEIVED" ->
                    FriendRequestReceived(
                        protocolJson.decodeFromString<FriendRequestReceivedMsg>(text),
                    )
                "FRIEND_ACCEPTED" ->
                    FriendAccepted(protocolJson.decodeFromString<FriendAcceptedMsg>(text))
                "FRIEND_REMOVED" ->
                    FriendRemoved(protocolJson.decodeFromString<FriendRemovedMsg>(text))
                "FRIEND_PRESENCE_UPDATE" ->
                    FriendPresenceUpdated(
                        protocolJson.decodeFromString<FriendPresenceUpdateMsg>(text),
                    )
                "JOIN_FRIEND_GAME_RESP" ->
                    JoinFriendGameResponse(
                        protocolJson.decodeFromString<JoinFriendGameRespMsg>(text),
                    )
                "LATE_JOIN_PROBE" ->
                    LateJoinProbeReceived(
                        protocolJson.decodeFromString<LateJoinProbeMsg>(text),
                    )
                "LATE_JOIN_APPROVED" ->
                    LateJoinApprovedReceived(
                        protocolJson.decodeFromString<LateJoinApprovedMsg>(text),
                    )
                "LATE_JOIN_PROBE" ->
                    LateJoinProbeReceived(
                        protocolJson.decodeFromString<LateJoinProbeMsg>(text),
                    )
                "LATE_JOIN_APPROVED" ->
                    LateJoinApprovedReceived(
                        protocolJson.decodeFromString<LateJoinApprovedMsg>(text),
                    )
                else -> Unknown(type, text)
            }
        }
    }
}
