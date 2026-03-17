package com.dxxredux.app.multiplayer

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    AUTHENTICATING,
    CONNECTED,
    RECONNECTING,
}

enum class MultiplayerNav {
    BROWSER,
    LOBBY,
    FRIENDS,
    LAN,
}

data class CurrentLobbyState(
    val lobbyId: String,
    val players: List<LobbyPlayerInfo> = emptyList(),
    val isHost: Boolean = false,
    val hostPlayerId: String? = null,
)

data class ChatMessage(
    val fromCallsign: String,
    val text: String,
    val isMe: Boolean = false,
)

/** Peer NAT/connectivity information received from the server. */
data class PeerNatInfo(
    val peerId: String,
    val candidates: List<ConnectionCandidate>,
    val natType: String,
)

/** Relay session assigned by the server for NAT-blocked peers. */
data class RelayInfo(
    val relayAddr: String,
    val sessionToken: String,
)

/** Game launch info received from the server in GAME_STARTING. */
data class GameLaunchInfo(
    val game: String,
    val mission: String,
    val mode: String,
    val difficulty: Int,
    val levelNum: Int,
    val maxPlayers: Int,
    val yourSlot: Int,
    val isHost: Boolean,
    val peers: List<PeerAssignment>,
    // LAN direct join: host's real LAN IP address (null for online/proxy path)
    val lanHostAddr: String? = null,
    val isLan: Boolean = false,
)

data class MatchmakingState(
    val status: ConnectionStatus = ConnectionStatus.DISCONNECTED,
    val serverUrl: String = NetworkConstants.DEFAULT_SERVER_URL,
    val callsign: String = "Player",
    val playerId: String? = null,
    val sessionToken: String? = null,
    val lobbies: List<LobbyInfo> = emptyList(),
    val serverStatus: ServerStatusMsg? = null,
    val motd: String? = null,
    val maintenanceMessage: String? = null,
    val errorMessage: String? = null,
    val statusLog: List<String> = emptyList(),
    val nav: MultiplayerNav = MultiplayerNav.BROWSER,
    val currentLobby: CurrentLobbyState? = null,
    val chatMessages: List<ChatMessage> = emptyList(),
    val connectionInfo: List<PeerConnectionInfoMsg> = emptyList(),
    val peerCandidates: Map<String, PeerNatInfo> = emptyMap(),
    val connectivityPairs: List<CandidatePair> = emptyList(),
    val relayInfo: RelayInfo? = null,
    val stunAddrs: List<String> = emptyList(),
    val gameLaunchInfo: GameLaunchInfo? = null,
    val friends: List<FriendInfo> = emptyList(),
    val pendingFriendRequests: List<FriendRequestReceivedMsg> = emptyList(),
)

// Global observable state for Compose UI
object MatchmakingStateHolder {
    private val _state = MutableStateFlow(MatchmakingState())
    val state: StateFlow<MatchmakingState> = _state.asStateFlow()

    fun update(transform: (MatchmakingState) -> MatchmakingState) {
        _state.value = transform(_state.value)
    }

    fun appendLog(msg: String) {
        update { s ->
            val log = s.statusLog.takeLast(99) + msg
            s.copy(statusLog = log)
        }
    }
}
