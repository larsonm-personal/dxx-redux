package com.dxxredux.app.multiplayer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.net.InetSocketAddress
import java.util.UUID
import java.util.concurrent.TimeUnit
import kotlin.math.min

private const val TAG = "MatchmakingService"

object MatchmakingService {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val state get() = MatchmakingStateHolder

    private val client =
        OkHttpClient
            .Builder()
            .pingInterval(30, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS) // no read timeout for websocket
            .build()

    private var webSocket: WebSocket? = null
    private var reconnectJob: Job? = null
    private var reconnectAttempt = 0
    private var manualDisconnect = false
    private var stunJob: Job? = null
    private var stunCompleted = false
    private var localhostProxy: LocalhostProxy? = null

    // Unique per app process -- ensures two emulators/devices get different player IDs
    // when using dev mode (SKIP_GPGS_VERIFY=true on the server)
    private val devToken: String = "dev-${UUID.randomUUID()}"

    fun connect(
        serverUrl: String,
        callsign: String,
    ) {
        manualDisconnect = false
        reconnectJob?.cancel()
        webSocket?.close(NetworkConstants.CLOSE_NORMAL, null)

        state.update {
            it.copy(
                status = ConnectionStatus.CONNECTING,
                serverUrl = serverUrl,
                callsign = callsign,
                errorMessage = null,
            )
        }
        state.appendLog("Connecting to $serverUrl ...")

        val request = Request.Builder().url(serverUrl).build()
        webSocket = client.newWebSocket(request, Listener(callsign))
    }

    fun disconnect() {
        manualDisconnect = true
        reconnectJob?.cancel()
        stunJob?.cancel()
        stunJob = null
        stunCompleted = false
        localhostProxy?.shutdown()
        localhostProxy = null
        webSocket?.close(NetworkConstants.CLOSE_NORMAL, "user disconnect")
        webSocket = null
        state.update {
            it.copy(
                status = ConnectionStatus.DISCONNECTED,
                playerId = null,
                sessionToken = null,
                currentLobby = null,
                chatMessages = emptyList(),
                connectionInfo = emptyList(),
                peerCandidates = emptyMap(),
                connectivityPairs = emptyList(),
                relayInfo = null,
                nav = MultiplayerNav.BROWSER,
                gameLaunchInfo = null,
            )
        }
        state.appendLog("Disconnected.")
    }

    fun send(json: String) {
        val ws = webSocket
        if (ws == null) {
            Log.w(TAG, "send() called with no active WebSocket")
            return
        }
        Log.d(TAG, "TX: $json")
        ws.send(json)
    }

    fun sendAuthenticate(callsign: String) {
        val msg =
            AuthenticateMsg(
                callsign = callsign,
                playGamesToken = devToken,
            )
        send(protocolJson.encodeToString(AuthenticateMsg.serializer(), msg))
    }

    fun requestLobbyList() {
        send(protocolJson.encodeToString(ListLobbiesMsg.serializer(), ListLobbiesMsg()))
    }

    fun createLobby(
        game: String,
        mission: String,
        mode: String,
        maxPlayers: Int,
    ) {
        val msg =
            CreateLobbyMsg(
                game = game,
                mission = mission,
                mode = mode,
                maxPlayers = maxPlayers,
            )
        send(protocolJson.encodeToString(CreateLobbyMsg.serializer(), msg))
    }

    fun joinLobby(
        lobbyId: String,
        lobbyCode: String? = null,
    ) {
        val msg = JoinLobbyMsg(lobbyId = lobbyId, lobbyCode = lobbyCode)
        send(protocolJson.encodeToString(JoinLobbyMsg.serializer(), msg))
        state.appendLog("Joining lobby $lobbyId...")
    }

    fun leaveLobby() {
        send(protocolJson.encodeToString(LeaveLobbyMsg.serializer(), LeaveLobbyMsg()))
        stunJob?.cancel()
        stunJob = null
        stunCompleted = false
        state.update {
            it.copy(
                currentLobby = null,
                chatMessages = emptyList(),
                connectionInfo = emptyList(),
                peerCandidates = emptyMap(),
                connectivityPairs = emptyList(),
                relayInfo = null,
                nav = MultiplayerNav.BROWSER,
            )
        }
        state.appendLog("Left lobby.")
        requestLobbyList()
    }

    fun setReady(ready: Boolean) {
        send(protocolJson.encodeToString(ReadyMsg.serializer(), ReadyMsg(ready = ready)))
    }

    fun startGame() {
        send(protocolJson.encodeToString(StartGameMsg.serializer(), StartGameMsg()))
        state.appendLog("Requesting game start...")
    }

    fun kickPlayer(playerId: String) {
        send(protocolJson.encodeToString(KickPlayerMsg.serializer(), KickPlayerMsg(playerId = playerId)))
        state.appendLog("Kicking player $playerId...")
    }

    fun sendMessage(
        targetPlayerId: String,
        text: String,
    ) {
        val msg = SendMessageMsg(targetPlayerId = targetPlayerId, text = text)
        send(protocolJson.encodeToString(SendMessageMsg.serializer(), msg))
    }

    fun sendStunResult(
        candidates: List<ConnectionCandidate>,
        natType: String,
    ) {
        val msg = StunResultMsg(candidates = candidates, natType = natType)
        send(protocolJson.encodeToString(StunResultMsg.serializer(), msg))
    }

    fun sendConnectivityOk(
        peerId: String,
        winningCandidateType: String,
        rttMs: Int,
    ) {
        val msg = ConnectivityOkMsg(peerId = peerId, winningCandidateType = winningCandidateType, rttMs = rttMs)
        send(protocolJson.encodeToString(ConnectivityOkMsg.serializer(), msg))
    }

    /** Broadcast a message to all players in the current lobby. */
    fun sendLobbyChat(text: String) {
        val lobby = state.state.value.currentLobby ?: return
        val myId = state.state.value.playerId ?: return
        // Send to each player in the lobby except ourselves
        for (player in lobby.players) {
            if (player.playerId != myId) {
                sendMessage(player.playerId, text)
            }
        }
        // Add our own message to the chat locally
        state.update { s ->
            val msgs =
                s.chatMessages.takeLast(49) +
                    ChatMessage(
                        fromCallsign = s.callsign,
                        text = text,
                        isMe = true,
                    )
            s.copy(chatMessages = msgs)
        }
    }

    private fun scheduleReconnect() {
        if (manualDisconnect) return
        reconnectJob?.cancel()
        reconnectJob =
            scope.launch {
                val delayMs =
                    min(
                        NetworkConstants.RECONNECT_BASE_DELAY_MS * (1L shl reconnectAttempt),
                        NetworkConstants.RECONNECT_MAX_DELAY_MS,
                    )
                state.update { it.copy(status = ConnectionStatus.RECONNECTING) }
                state.appendLog("Reconnecting in ${delayMs}ms (attempt ${reconnectAttempt + 1})...")
                delay(delayMs)
                reconnectAttempt++
                val s = state.state.value
                connect(s.serverUrl, s.callsign)
            }
    }

    private fun launchStunDiscovery() {
        val addrs = state.state.value.stunAddrs
        if (addrs.size < 2) {
            state.appendLog("STUN: no server addresses available, skipping")
            stunCompleted = true
            sendStunResult(emptyList(), "unknown")
            return
        }
        stunJob =
            scope.launch {
                state.appendLog("Starting STUN discovery...")
                try {
                    val report = StunClient.discover(addrs)
                    stunCompleted = true
                    state.appendLog("STUN: ${report.natType}, ${report.candidates.size} candidates")
                    sendStunResult(report.candidates, report.natType)
                } catch (e: Exception) {
                    Log.e(TAG, "STUN discovery failed", e)
                    state.appendLog("STUN discovery failed: ${e.message}")
                    // Send minimal result so the server can proceed (will use relay)
                    stunCompleted = true
                    sendStunResult(emptyList(), "unknown")
                } finally {
                    stunJob = null
                }
            }
    }

    private fun launchConnectivityCheck(pairs: List<CandidatePair>) {
        scope.launch {
            try {
                val result = ConnectivityChecker.probe(pairs)
                if (result != null) {
                    state.appendLog("Direct connection: ${result.winningCandidateType} (${result.rttMs}ms)")
                    sendConnectivityOk(result.peerId, result.winningCandidateType, result.rttMs)
                } else {
                    state.appendLog("No direct connection, will use relay")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Connectivity check failed", e)
                state.appendLog("Connectivity check failed: ${e.message}")
            }
        }
    }

    private class Listener(
        private val callsign: String,
    ) : WebSocketListener() {
        override fun onOpen(
            webSocket: WebSocket,
            response: Response,
        ) {
            Log.i(TAG, "WebSocket open")
            reconnectAttempt = 0
            state.update { it.copy(status = ConnectionStatus.AUTHENTICATING) }
            state.appendLog("Connected. Authenticating as '$callsign'...")
            sendAuthenticate(callsign)
        }

        override fun onMessage(
            webSocket: WebSocket,
            text: String,
        ) {
            Log.d(TAG, "RX: $text")
            handleMessage(text)
        }

        override fun onClosing(
            webSocket: WebSocket,
            code: Int,
            reason: String,
        ) {
            Log.i(TAG, "WebSocket closing: $code $reason")
            webSocket.close(NetworkConstants.CLOSE_NORMAL, null)
        }

        override fun onClosed(
            webSocket: WebSocket,
            code: Int,
            reason: String,
        ) {
            Log.i(TAG, "WebSocket closed: $code $reason")
            this@MatchmakingService.webSocket = null
            state.update {
                it.copy(
                    status = ConnectionStatus.DISCONNECTED,
                    playerId = null,
                    sessionToken = null,
                    currentLobby = null,
                    chatMessages = emptyList(),
                    connectionInfo = emptyList(),
                    nav = MultiplayerNav.BROWSER,
                )
            }
            state.appendLog("Connection closed ($code).")
            scheduleReconnect()
        }

        override fun onFailure(
            webSocket: WebSocket,
            t: Throwable,
            response: Response?,
        ) {
            Log.e(TAG, "WebSocket failure: ${t.message}", t)
            this@MatchmakingService.webSocket = null
            state.update {
                it.copy(
                    status = ConnectionStatus.DISCONNECTED,
                    errorMessage = t.message,
                    playerId = null,
                    sessionToken = null,
                    currentLobby = null,
                    chatMessages = emptyList(),
                    connectionInfo = emptyList(),
                    nav = MultiplayerNav.BROWSER,
                )
            }
            state.appendLog("Error: ${t.message}")
            scheduleReconnect()
        }
    }

    private fun handleMessage(text: String) {
        when (val msg = ServerMessage.parse(text)) {
            is ServerMessage.AuthOkMsg -> {
                state.update {
                    it.copy(
                        status = ConnectionStatus.CONNECTED,
                        playerId = msg.data.playerId,
                        sessionToken = msg.data.sessionToken,
                        stunAddrs = msg.data.stunAddrs,
                        errorMessage = null,
                    )
                }
                state.appendLog("Authenticated! Player ID: ${msg.data.playerId}")
                // Auto-request lobby list after auth
                requestLobbyList()
            }

            is ServerMessage.AuthFailMsg -> {
                state.update {
                    it.copy(
                        status = ConnectionStatus.DISCONNECTED,
                        errorMessage = "Auth failed: ${msg.data.reason}",
                    )
                }
                state.appendLog("Auth failed: ${msg.data.reason}")
                manualDisconnect = true // don't auto-reconnect on auth failure
            }

            is ServerMessage.PowChallengeMsg -> {
                state.appendLog("PoW challenge received (not yet implemented)")
            }

            is ServerMessage.ErrorMsg -> {
                state.update { it.copy(errorMessage = "${msg.data.code}: ${msg.data.message}") }
                state.appendLog("Server error: ${msg.data.code} - ${msg.data.message}")
                // Kicked from lobby -- return to browser
                if (msg.data.code.contains("KICKED", ignoreCase = true) ||
                    msg.data.code.contains("LOBBY", ignoreCase = true)
                ) {
                    state.update {
                        it.copy(
                            currentLobby = null,
                            chatMessages = emptyList(),
                            connectionInfo = emptyList(),
                            nav = MultiplayerNav.BROWSER,
                        )
                    }
                }
            }

            is ServerMessage.MotdMsg -> {
                state.update { it.copy(motd = msg.data.message) }
                state.appendLog("MOTD: ${msg.data.message}")
            }

            is ServerMessage.LobbyListReceived -> {
                state.update { it.copy(lobbies = msg.data.lobbies) }
                state.appendLog("Lobby list: ${msg.data.lobbies.size} lobbies")
            }

            is ServerMessage.LobbyUpdated -> {
                val update = msg.data
                val myId = state.state.value.playerId
                // First player in the list is the host
                val hostId = update.players.firstOrNull()?.playerId
                val isHost = myId != null && myId == hostId
                state.update {
                    it.copy(
                        currentLobby =
                            CurrentLobbyState(
                                lobbyId = update.lobbyId,
                                players = update.players,
                                isHost = isHost,
                                hostPlayerId = hostId,
                            ),
                        nav = MultiplayerNav.LOBBY,
                    )
                }
                state.appendLog(
                    "Lobby updated: ${update.lobbyId} (${update.players.size} players)",
                )
                // Auto-start STUN when lobby has 2+ players and we haven't done it yet
                if (update.players.size >= 2 && !stunCompleted && stunJob == null) {
                    launchStunDiscovery()
                }
            }

            is ServerMessage.ServerStatusReceived -> {
                state.update { it.copy(serverStatus = msg.data) }
                state.appendLog(
                    "Server: ${msg.data.onlinePlayers} online, " +
                        "${msg.data.activeGamesCount} games",
                )
            }

            is ServerMessage.GameStarting -> {
                val gs = msg.data
                state.appendLog("Game starting: ${gs.mission} (slot ${gs.yourSlot})")

                // Set up localhost proxy for each peer
                localhostProxy?.shutdown()
                val proxy = LocalhostProxy(scope)
                for (peer in gs.peers) {
                    val addrParts = peer.addr.split(":")
                    if (addrParts.size != 2) {
                        state.appendLog("Bad peer addr: ${peer.addr}")
                        continue
                    }
                    val addr = InetSocketAddress(addrParts[0], addrParts[1].toIntOrNull() ?: continue)
                    proxy.addPeer(
                        PeerProxyConfig(
                            peerSlot = peer.slot,
                            localPort = NetworkConstants.PROXY_PORT_BASE + peer.slot,
                            realAddr = addr,
                            isRelay = peer.isRelay,
                            relayToken = peer.relayToken?.toUInt() ?: 0u,
                            relayDestSlot = peer.relayDestSlot ?: peer.slot,
                        ),
                    )
                }
                localhostProxy = proxy

                // Determine if we're the host (slot 0 = host)
                val isHost = gs.yourSlot == 0
                val launchInfo =
                    GameLaunchInfo(
                        game = gs.game,
                        mission = gs.mission,
                        mode = gs.mode,
                        difficulty = gs.difficulty,
                        levelNum = gs.levelNum,
                        maxPlayers = gs.maxPlayers,
                        yourSlot = gs.yourSlot,
                        isHost = isHost,
                        peers = gs.peers,
                    )
                state.update { it.copy(gameLaunchInfo = launchInfo) }
            }

            is ServerMessage.RateLimited -> {
                state.appendLog("Rate limited, retry in ${msg.data.retryAfterMs}ms")
            }

            is ServerMessage.VersionRejected -> {
                state.update {
                    it.copy(errorMessage = "Version rejected: ${msg.data.reason}")
                }
                state.appendLog("Version rejected: ${msg.data.reason}")
                manualDisconnect = true
            }

            is ServerMessage.MessageReceived -> {
                val m = msg.data
                state.update { s ->
                    val msgs =
                        s.chatMessages.takeLast(49) +
                            ChatMessage(
                                fromCallsign = m.fromCallsign,
                                text = m.text,
                                isMe = false,
                            )
                    s.copy(chatMessages = msgs)
                }
            }

            is ServerMessage.MessageSent -> {
                // Delivery confirmation -- no UI action needed
            }

            is ServerMessage.ConnectionInfoReceived -> {
                state.update { it.copy(connectionInfo = msg.data.connections) }
            }

            is ServerMessage.PeerCandidatesReceived -> {
                val pc = msg.data
                state.update { s ->
                    val info = PeerNatInfo(pc.peerId, pc.candidates, pc.natType)
                    s.copy(peerCandidates = s.peerCandidates + (pc.peerId to info))
                }
                state.appendLog("Peer candidates: ${pc.peerId} (${pc.natType}, ${pc.candidates.size} candidates)")
            }

            is ServerMessage.ConnectivityCheckGoReceived -> {
                val pairs = msg.data.peerAddrs
                state.update { it.copy(connectivityPairs = pairs) }
                state.appendLog("Connectivity check GO: ${pairs.size} pairs to probe")
                launchConnectivityCheck(pairs)
            }

            is ServerMessage.RelayAssignedReceived -> {
                val relay = msg.data
                state.update { it.copy(relayInfo = RelayInfo(relay.relayAddr, relay.sessionToken)) }
                state.appendLog("Relay assigned: ${relay.relayAddr}")
            }

            is ServerMessage.Unknown -> {
                state.appendLog("Unknown message type: ${msg.type}")
            }
        }
    }
}
