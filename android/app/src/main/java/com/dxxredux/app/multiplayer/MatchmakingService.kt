package com.dxxredux.app.multiplayer

import android.app.Activity
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.lang.ref.WeakReference
import java.net.InetAddress
import java.net.InetSocketAddress
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.UUID
import java.util.concurrent.TimeUnit
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager
import kotlin.math.min

private const val TAG = "MatchmakingService"
private const val MAX_RECONNECT_ATTEMPTS = 15

/** Normalize user input like "myserver.com" into a connectable WebSocket URL.
 *  Always defaults to wss:// (TLS). LAN self-signed certs are handled by lanClient.
 *  Honors explicit ws:// if the user typed it. */
private fun normalizeServerUrl(raw: String): String {
    var url = raw.trim()
    if (url.isEmpty()) return NetworkConstants.DEFAULT_SERVER_URL

    // Add scheme if missing -- always default to wss://
    // (lanClient handles self-signed certs for private IPs)
    if (!url.startsWith("ws://") && !url.startsWith("wss://")) {
        url = "wss://$url"
    }

    // Strip scheme to inspect host:port/path
    val schemeEnd = url.indexOf("://") + 3
    val scheme = url.substring(0, schemeEnd)
    val rest = url.substring(schemeEnd)
    val hostOnly = rest.substringBefore(':').substringBefore('/')

    // Split into host(:port) and path
    val slashIdx = rest.indexOf('/')
    val hostPort = if (slashIdx >= 0) rest.substring(0, slashIdx) else rest
    val path = if (slashIdx >= 0) rest.substring(slashIdx) else ""

    // Add default port if none specified: LAN -> 9000, public -> 443
    val withPort =
        if (hostPort.isNotEmpty() && !hostPort.contains(':')) {
            val defaultPort =
                if (isPrivateAddress(hostOnly)) {
                    NetworkConstants.DEFAULT_LAN_WSS_PORT
                } else {
                    NetworkConstants.DEFAULT_WSS_PORT
                }
            "$hostPort:$defaultPort"
        } else {
            hostPort
        }

    // Add default path if none specified
    val withPath = if (path.isEmpty()) NetworkConstants.DEFAULT_WS_PATH else path

    return "$scheme$withPort$withPath"
}

/** Check if a hostname is a private/LAN IP address (RFC 1918 + link-local). */
private fun isPrivateAddress(host: String): Boolean =
    try {
        val addr = InetAddress.getByName(host)
        addr.isSiteLocalAddress || addr.isLoopbackAddress || addr.isLinkLocalAddress
    } catch (_: Exception) {
        false
    }

object MatchmakingService {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val state get() = MatchmakingStateHolder

    private val client =
        OkHttpClient
            .Builder()
            .pingInterval(30, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS) // no read timeout for websocket
            .build()

    // OkHttp client that accepts self-signed certificates for LAN testing.
    // Only used when connecting to private/RFC1918 IP addresses.
    private val lanClient: OkHttpClient by lazy {
        val trustAll =
            object : X509TrustManager {
                override fun checkClientTrusted(
                    chain: Array<X509Certificate>,
                    authType: String,
                ) {}

                override fun checkServerTrusted(
                    chain: Array<X509Certificate>,
                    authType: String,
                ) {}

                override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
            }
        val sslContext = SSLContext.getInstance("TLS")
        sslContext.init(null, arrayOf<TrustManager>(trustAll), SecureRandom())
        OkHttpClient
            .Builder()
            .sslSocketFactory(sslContext.socketFactory, trustAll)
            .hostnameVerifier { _, _ -> true }
            .pingInterval(30, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS)
            .build()
    }

    @Volatile
    private var webSocket: WebSocket? = null

    @Volatile
    private var reconnectJob: Job? = null

    @Volatile
    private var reconnectAttempt = 0

    @Volatile
    private var manualDisconnect = false

    @Volatile
    private var lastLobbyId: String? = null // saved for re-join after reconnect

    @Volatile
    private var stunJob: Job? = null

    @Volatile
    private var stunCompleted = false

    /** gameInfo sent with the last CREATE_LOBBY, so the first LOBBY_UPDATE can carry it. */
    @Volatile
    private var pendingGameInfo: JsonObject = JsonObject(emptyMap())

    @Volatile
    private var localhostProxy: LocalhostProxy? = null

    @Volatile
    private var connectivityCheckJob: Job? = null

    // Periodic game state update coroutine (host-only, active during InGame)
    @Volatile
    private var gameStateUpdateJob: Job? = null

    @Volatile
    private var upnpMapping: UpnpMapping? = null

    // Shared UDP socket used for STUN, connectivity checks, and then handed
    // to the first PeerProxy so the UPnP port mapping stays valid.
    @Volatile
    private var candidateSocket: java.net.DatagramSocket? = null

    fun getProxyStats(): List<PeerProxyStats> = localhostProxy?.getStats() ?: emptyList()

    /** Create a simple proxy for LAN joiner (one peer = the host). */
    fun createLanProxy(
        hostAddr: String,
        hostPort: Int,
    ) {
        localhostProxy?.shutdown()
        val proxy = LocalhostProxy(scope)
        proxy.addPeer(
            PeerProxyConfig(
                peerSlot = 0,
                localPort = NetworkConstants.PROXY_PORT_BASE,
                realAddr = InetSocketAddress(hostAddr, hostPort),
                isRelay = false,
            ),
        )
        localhostProxy = proxy
    }

    // Unique per app process -- ensures two emulators/devices get different player IDs
    // when using dev mode (SKIP_GPGS_VERIFY=true on the server)
    private val devToken: String = "dev-${UUID.randomUUID()}"

    // Weak reference to current activity for GPGS auth code requests.
    // Set by SetupActivity.onCreate(), cleared automatically by GC.
    private var activityRef: WeakReference<Activity>? = null

    fun setActivity(activity: Activity) {
        activityRef = WeakReference(activity)
    }

    fun connect(
        serverUrl: String,
        callsign: String,
    ) {
        manualDisconnect = false
        reconnectJob?.cancel()
        webSocket?.close(NetworkConstants.CLOSE_NORMAL, null)

        val normalizedUrl = normalizeServerUrl(serverUrl)
        // Store the raw URL so the text field and reconnect use what the user typed
        state.update {
            it.copy(
                status = ConnectionStatus.CONNECTING,
                serverUrl = serverUrl.trim(),
                callsign = callsign,
                errorMessage = null,
            )
        }
        state.appendLog("Connecting to $normalizedUrl ...")
        NetLog.log("CONNECT", "Connecting to $normalizedUrl as '$callsign'")

        val request = Request.Builder().url(normalizedUrl).build()
        // Use LAN-tolerant client (accepts self-signed certs) for private IPs
        val host = request.url.host
        val httpClient = if (isPrivateAddress(host)) lanClient else client
        webSocket = httpClient.newWebSocket(request, Listener(callsign))
    }

    fun disconnect() {
        manualDisconnect = true
        lastLobbyId = null
        reconnectJob?.cancel()
        stunJob?.cancel()
        stunJob = null
        stunCompleted = false
        connectivityCheckJob?.cancel()
        connectivityCheckJob = null
        stopGameStateUpdates()
        cleanupUpnp()
        closeCandidateSocket()
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
                friends = emptyList(),
                pendingFriendRequests = emptyList(),
            )
        }
        state.appendLog("Disconnected.")
        NetLog.log("CONNECT", "Disconnected (manual)")
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
        val activity = activityRef?.get()
        if (activity != null && PlayGamesAuth.isConfigured) {
            // Try GPGS auth -- get a fresh server auth code
            scope.launch {
                // C12: timeout so we don't hang if Play Services is stuck
                val authCode =
                    kotlinx.coroutines.withTimeoutOrNull(5000L) {
                        PlayGamesAuth.getServerAuthCode(activity)
                    }
                if (authCode != null) {
                    Log.i(TAG, "Using GPGS auth code")
                    val msg =
                        AuthenticateMsg(
                            callsign = callsign,
                            playGamesToken = authCode,
                            authMethod = "gpgs",
                        )
                    send(protocolJson.encodeToString(AuthenticateMsg.serializer(), msg))
                } else {
                    Log.i(TAG, "GPGS auth code unavailable, falling back to dev token")
                    sendDevAuthenticate(callsign)
                }
            }
        } else {
            sendDevAuthenticate(callsign)
        }
    }

    private fun sendDevAuthenticate(callsign: String) {
        val msg =
            AuthenticateMsg(
                callsign = callsign,
                playGamesToken = devToken,
                authMethod = "dev",
            )
        send(protocolJson.encodeToString(AuthenticateMsg.serializer(), msg))
    }

    fun requestLobbyList() {
        send(protocolJson.encodeToString(ListLobbiesMsg.serializer(), ListLobbiesMsg()))
    }

    fun createLobby(
        game: String,
        maxPlayers: Int,
        gameInfo: JsonObject,
    ) {
        pendingGameInfo = gameInfo
        val msg =
            CreateLobbyMsg(
                game = game,
                maxPlayers = maxPlayers,
                gameInfo = gameInfo,
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
        NetLog.log("LOBBY", "Joining lobby $lobbyId")
    }

    fun leaveLobby() {
        send(protocolJson.encodeToString(LeaveLobbyMsg.serializer(), LeaveLobbyMsg()))
        stunJob?.cancel()
        stunJob = null
        stunCompleted = false
        cleanupUpnp()
        closeCandidateSocket()
        state.update {
            it.copy(
                currentLobby = null,
                chatMessages = emptyList(),
                connectionInfo = emptyList(),
                peerCandidates = emptyMap(),
                connectivityPairs = emptyList(),
                relayInfo = null,
                iceStatus = IceStatus(),
                nav = MultiplayerNav.BROWSER,
                gameLaunchInfo = null,
            )
        }
        state.appendLog("Left lobby.")
        NetLog.log("LOBBY", "Left lobby")
        requestLobbyList()
    }

    fun setReady(ready: Boolean) {
        send(protocolJson.encodeToString(ReadyMsg.serializer(), ReadyMsg(ready = ready)))
    }

    fun startGame() {
        send(protocolJson.encodeToString(StartGameMsg.serializer(), StartGameMsg()))
        state.appendLog("Requesting game start...")
        NetLog.log("LOBBY", "Requesting game start")
    }

    /** Host signals the server that the game engine has exited.
     *  The server transitions the lobby back to Waiting so players can re-ready. */
    fun endGame() {
        stopGameStateUpdates()
        send(protocolJson.encodeToString(EndGameMsg.serializer(), EndGameMsg()))
        state.appendLog("Game ended, returning to lobby.")
        NetLog.log("LOBBY", "Sent END_GAME")
        requestLobbyList()
    }

    fun kickPlayer(playerId: String) {
        send(protocolJson.encodeToString(KickPlayerMsg.serializer(), KickPlayerMsg(playerId = playerId)))
        state.appendLog("Kicking player $playerId...")
        NetLog.log("LOBBY", "Kicking player $playerId")
    }

    // -- In-game state updates (host only) --
    // Shared constants with C engine (multi.h):
    // NETSTAT_MENU=0, NETSTAT_PLAYING=1, GM_NETWORK=4
    private const val NETSTAT_PLAYING = 1
    private const val GM_NETWORK = 4
    private const val GAME_STATE_UPDATE_INTERVAL_MS = 30_000L

    /** Start periodic game state updates to the matchmaking server (host only). */
    fun startGameStateUpdates() {
        gameStateUpdateJob?.cancel()
        gameStateUpdateJob =
            scope.launch {
                delay(3000) // initial delay to let the game settle
                while (true) {
                    try {
                        pollAndSendGameState()
                    } catch (e: Exception) {
                        Log.w(TAG, "Game state update failed", e)
                    }
                    delay(GAME_STATE_UPDATE_INTERVAL_MS)
                }
            }
        NetLog.log("GAME", "Started game state update loop")
    }

    /** Stop periodic game state updates. */
    fun stopGameStateUpdates() {
        gameStateUpdateJob?.cancel()
        gameStateUpdateJob = null
    }

    private fun pollAndSendGameState() {
        val activity = activityRef?.get() ?: return
        if (activity !is com.dxxredux.app.MainActivity) return
        val arr = activity.nativeGetNetgameState()
        if (arr.size < 5) return
        val gameStatus = arr[0]
        val numConnected = arr[1]
        val maxPlayers = arr[2]
        val levelNum = arr[3]
        val gameMode = arr[4]
        // Only send updates when in a network game
        if (gameMode and GM_NETWORK == 0) return
        // Build and send the update message
        val msg = org.json.JSONObject()
        msg.put("type", "UPDATE_GAME_STATE")
        msg.put("player_count", numConnected)
        msg.put("max_players", maxPlayers)
        msg.put("current_level", levelNum)
        msg.put("game_status", gameStatus)
        send(msg.toString())
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

    // -- Friend methods --

    fun requestFriendList() {
        send(protocolJson.encodeToString(FriendListRequestMsg.serializer(), FriendListRequestMsg()))
    }

    fun sendFriendRequest(targetCallsign: String) {
        val msg = FriendRequestMsg(targetCallsign = targetCallsign)
        send(protocolJson.encodeToString(FriendRequestMsg.serializer(), msg))
        state.appendLog("Friend request sent to '$targetCallsign'")
        NetLog.log("SOCIAL", "Friend request sent to '$targetCallsign'")
    }

    fun acceptFriend(playerId: String) {
        val msg = FriendAcceptMsg(playerId = playerId)
        send(protocolJson.encodeToString(FriendAcceptMsg.serializer(), msg))
        // Remove from pending requests
        state.update { s ->
            s.copy(pendingFriendRequests = s.pendingFriendRequests.filter { it.fromPlayerId != playerId })
        }
        state.appendLog("Accepted friend request from $playerId")
        NetLog.log("SOCIAL", "Accepted friend request from $playerId")
        requestFriendList()
    }

    fun removeFriend(playerId: String) {
        val msg = FriendRemoveMsg(playerId = playerId)
        send(protocolJson.encodeToString(FriendRemoveMsg.serializer(), msg))
        state.appendLog("Removed friend $playerId")
        NetLog.log("SOCIAL", "Removed friend $playerId")
        requestFriendList()
    }

    fun blockPlayer(playerId: String) {
        val msg = FriendBlockMsg(playerId = playerId)
        send(protocolJson.encodeToString(FriendBlockMsg.serializer(), msg))
        state.appendLog("Blocked player $playerId")
        NetLog.log("SOCIAL", "Blocked player $playerId")
        requestFriendList()
    }

    fun joinFriendGame(friendPlayerId: String) {
        val msg = JoinFriendGameMsg(friendPlayerId = friendPlayerId)
        send(protocolJson.encodeToString(JoinFriendGameMsg.serializer(), msg))
        state.appendLog("Joining friend's game...")
        NetLog.log("SOCIAL", "Joining friend's game: $friendPlayerId")
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
        val lobby = state.state.value.currentLobby
        val myId = state.state.value.playerId
        if (lobby == null) {
            Log.w(TAG, "sendLobbyChat: no current lobby, dropping '$text'")
            return
        }
        if (myId == null) {
            Log.w(TAG, "sendLobbyChat: no player id, dropping '$text'")
            return
        }
        Log.i(TAG, "sendLobbyChat: text='$text' lobby=${lobby.lobbyId} myId=$myId players=${lobby.players.size}")
        // Send to each player in the lobby except ourselves
        for (player in lobby.players) {
            if (player.playerId != myId) {
                Log.i(TAG, "sendLobbyChat: sending to ${player.playerId}")
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
        Log.i(TAG, "sendLobbyChat: done, chatMessages=${state.state.value.chatMessages.size}")
    }

    private fun scheduleReconnect() {
        if (manualDisconnect) return
        if (reconnectAttempt >= MAX_RECONNECT_ATTEMPTS) {
            state.update {
                it.copy(
                    status = ConnectionStatus.DISCONNECTED,
                    errorMessage = "Connection lost after $MAX_RECONNECT_ATTEMPTS attempts",
                )
            }
            state.appendLog("Gave up after $MAX_RECONNECT_ATTEMPTS reconnect attempts")
            NetLog.log("CONNECT", "Gave up reconnecting after $MAX_RECONNECT_ATTEMPTS attempts")
            return
        }
        reconnectJob?.cancel()
        reconnectJob =
            scope.launch {
                val delayMs =
                    min(
                        NetworkConstants.RECONNECT_BASE_DELAY_MS * (1L shl reconnectAttempt.coerceAtMost(20)),
                        NetworkConstants.RECONNECT_MAX_DELAY_MS,
                    )
                state.update { it.copy(status = ConnectionStatus.RECONNECTING) }
                state.appendLog("Reconnecting in ${delayMs}ms (attempt ${reconnectAttempt + 1})...")
                NetLog.log("CONNECT", "Reconnecting in ${delayMs}ms (attempt ${reconnectAttempt + 1})")
                delay(delayMs)
                reconnectAttempt++
                val s = state.state.value
                connect(s.serverUrl, s.callsign)
            }
    }

    private fun cleanupUpnp() {
        upnpMapping?.let { mapping ->
            scope.launch(Dispatchers.IO) {
                UpnpClient.removeMapping(mapping)
            }
        }
        upnpMapping = null
    }

    private fun closeCandidateSocket() {
        candidateSocket?.close()
        candidateSocket = null
    }

    private fun launchStunDiscovery() {
        val addrs = state.state.value.stunAddrs
        stunJob =
            scope.launch {
                state.update { it.copy(iceStatus = IceStatus(phase = IcePhase.STUN_DISCOVERY)) }
                state.appendLog("Starting candidate discovery (${addrs.size} STUN servers)...")
                NetLog.log("STUN", "Starting candidate discovery (${addrs.size} STUN servers)")
                try {
                    // Create the shared candidate socket. It persists through STUN,
                    // connectivity checking, and is handed to the first PeerProxy so
                    // that the UPnP port mapping remains associated with an open socket.
                    val socket = java.net.DatagramSocket()
                    candidateSocket = socket
                    val candidatePort = socket.localPort

                    // Run STUN and UPnP in parallel, both using the same port
                    val stunDeferred =
                        async(Dispatchers.IO) {
                            StunClient.discover(addrs, socket)
                        }
                    val upnpDeferred =
                        async(Dispatchers.IO) {
                            tryUpnpMapping(candidatePort)
                        }

                    val report = stunDeferred.await()
                    val upnpResult = upnpDeferred.await()

                    // Merge UPnP candidate into the report if mapping succeeded
                    val allCandidates = report.candidates.toMutableList()
                    if (upnpResult != null) {
                        allCandidates.add(
                            ConnectionCandidate("upnp", "${upnpResult.externalIp}:${upnpResult.externalPort}"),
                        )
                        state.appendLog("UPnP: mapped ${upnpResult.externalIp}:${upnpResult.externalPort}")
                        NetLog.log("UPNP", "Mapped ${upnpResult.externalIp}:${upnpResult.externalPort}")
                    }

                    stunCompleted = true
                    state.update { s ->
                        s.copy(
                            iceStatus =
                                s.iceStatus.copy(
                                    phase = IcePhase.STUN_COMPLETE,
                                    stunNatType = report.natType,
                                    stunCandidateCount = allCandidates.size,
                                    upnpMapped = upnpResult != null,
                                    upnpAddr = upnpResult?.let { "${it.externalIp}:${it.externalPort}" },
                                ),
                        )
                    }
                    state.appendLog("Discovery: ${report.natType}, ${allCandidates.size} candidates")
                    NetLog.log("STUN", "Result: natType=${report.natType} candidates=${allCandidates.size}")
                    sendStunResult(allCandidates, report.natType)
                } catch (e: Exception) {
                    Log.e(TAG, "Candidate discovery failed", e)
                    state.update { s ->
                        s.copy(
                            iceStatus =
                                s.iceStatus.copy(
                                    phase = IcePhase.FAILED,
                                    errorMessage = e.message,
                                ),
                        )
                    }
                    state.appendLog("Candidate discovery failed: ${e.message}")
                    NetLog.log("ERROR", "Candidate discovery failed: ${e.message}")
                    // Send minimal result so the server can proceed (will use relay)
                    stunCompleted = true
                    sendStunResult(emptyList(), "unknown")
                } finally {
                    stunJob = null
                }
            }
    }

    /**
     * Attempt UPnP port mapping for [port] on the first available local IP.
     * Returns the mapping or null. Catches all exceptions internally.
     */
    private fun tryUpnpMapping(port: Int): UpnpMapping? {
        return try {
            val localIps = StunClient.getLocalIpv4Addresses()
            if (localIps.isEmpty()) return null
            val localIp = localIps.first()
            val mapping = UpnpClient.tryMap(port, localIp)
            if (mapping != null) {
                upnpMapping = mapping
            }
            mapping
        } catch (e: Exception) {
            Log.w(TAG, "UPnP attempt failed: ${e.message}")
            null
        }
    }

    private fun launchConnectivityCheck(pairs: List<CandidatePair>) {
        // C11: cancel previous check to avoid duplicates
        connectivityCheckJob?.cancel()
        connectivityCheckJob =
            scope.launch {
                try {
                    val result =
                        ConnectivityChecker.probe(
                            pairs,
                            existingSocket = candidateSocket,
                        )
                    if (result != null) {
                        state.update { s ->
                            s.copy(
                                iceStatus =
                                    s.iceStatus.copy(
                                        phase = IcePhase.COMPLETE,
                                        probeResult = result.winningCandidateType,
                                        probeRttMs = result.rttMs,
                                    ),
                            )
                        }
                        state.appendLog("Direct connection: ${result.winningCandidateType} (${result.rttMs}ms)")
                        NetLog.log(
                            "HOLEPUNCH",
                            "Direct: type=${result.winningCandidateType} rtt=${result.rttMs}ms peer=${result.peerId}",
                        )
                        sendConnectivityOk(result.peerId, result.winningCandidateType, result.rttMs)
                    } else {
                        state.update { s ->
                            s.copy(
                                iceStatus =
                                    s.iceStatus.copy(
                                        phase = IcePhase.COMPLETE,
                                        probeResult = "relay",
                                    ),
                            )
                        }
                        state.appendLog("No direct connection, will use relay")
                        NetLog.log("HOLEPUNCH", "No direct connection, falling back to relay")
                        // Notify the server for each unique peer so it allocates relay
                        pairs.map { it.peerId }.distinct().forEach { peerId ->
                            sendConnectivityOk(peerId, "relay", 0)
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Connectivity check failed", e)
                    state.update { s ->
                        s.copy(
                            iceStatus =
                                s.iceStatus.copy(
                                    phase = IcePhase.COMPLETE,
                                    probeResult = "relay",
                                    errorMessage = e.message,
                                ),
                        )
                    }
                    state.appendLog("Connectivity check failed: ${e.message}")
                    NetLog.log("ERROR", "Connectivity check failed: ${e.message}")
                    // Still notify the server so game launch isn't blocked
                    pairs.map { it.peerId }.distinct().forEach { peerId ->
                        sendConnectivityOk(peerId, "relay", 0)
                    }
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
            NetLog.log("CONNECT", "WebSocket open")
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
            // C9: clean up proxy on connection loss
            localhostProxy?.shutdown()
            localhostProxy = null
            // Save lobby ID for re-join after reconnect
            val currentLobby = state.state.value.currentLobby
            lastLobbyId = currentLobby?.lobbyId
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
            NetLog.log("CONNECT", "Closed code=$code reason='$reason'")
            scheduleReconnect()
        }

        override fun onFailure(
            webSocket: WebSocket,
            t: Throwable,
            response: Response?,
        ) {
            Log.e(TAG, "WebSocket failure: ${t.message}", t)
            this@MatchmakingService.webSocket = null
            // C9: clean up proxy on connection loss
            localhostProxy?.shutdown()
            localhostProxy = null
            // Save lobby ID for re-join after reconnect
            if (lastLobbyId == null) {
                val currentLobby = state.state.value.currentLobby
                lastLobbyId = currentLobby?.lobbyId
            }
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
            NetLog.log("ERROR", "WebSocket failure: ${t.message}")
            scheduleReconnect()
        }
    }

    private fun handleMessage(text: String) {
        try {
            when (val msg = ServerMessage.parse(text)) {
                is ServerMessage.AuthOkMsg -> {
                    state.update {
                        it.copy(
                            status = ConnectionStatus.CONNECTED,
                            playerId = msg.data.playerId,
                            sessionToken = msg.data.sessionToken,
                            stunAddrs = msg.data.stunAddrs,
                            errorMessage = null,
                            maintenanceMessage = null,
                        )
                    }
                    state.appendLog("Authenticated! Player ID: ${msg.data.playerId}")
                    NetLog.log("AUTH", "Authenticated as ${msg.data.playerId}")
                    // Auto-request lobby list and friend list after auth
                    requestLobbyList()
                    requestFriendList()
                    // Re-join lobby if we had one before disconnect
                    lastLobbyId?.let { lobbyId ->
                        state.appendLog("Re-joining lobby $lobbyId...")
                        NetLog.log("LOBBY", "Re-joining lobby $lobbyId")
                        joinLobby(lobbyId)
                        lastLobbyId = null
                    }
                }

                is ServerMessage.AuthFailMsg -> {
                    state.update {
                        it.copy(
                            status = ConnectionStatus.DISCONNECTED,
                            errorMessage = "Auth failed: ${msg.data.reason}",
                        )
                    }
                    state.appendLog("Auth failed: ${msg.data.reason}")
                    NetLog.log("AUTH", "Auth failed: ${msg.data.reason}")
                    manualDisconnect = true // don't auto-reconnect on auth failure
                }

                is ServerMessage.PowChallengeMsg -> {
                    state.appendLog("PoW challenge received (not yet implemented)")
                    NetLog.log("AUTH", "PoW challenge received (not yet implemented)")
                }

                is ServerMessage.ErrorMsg -> {
                    state.update { it.copy(errorMessage = "${msg.data.code}: ${msg.data.message}") }
                    state.appendLog("Server error: ${msg.data.code} - ${msg.data.message}")
                    NetLog.log("ERROR", "Server: ${msg.data.code} - ${msg.data.message}")
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
                    NetLog.log("SERVER", "MOTD: ${msg.data.message}")
                }

                is ServerMessage.LobbyListReceived -> {
                    state.update { it.copy(lobbies = msg.data.lobbies) }
                    state.appendLog("Lobby list: ${msg.data.lobbies.size} lobbies")
                    NetLog.log("SERVER", "Lobby list: ${msg.data.lobbies.size} lobbies")
                }

                is ServerMessage.LobbyUpdated -> {
                    val update = msg.data
                    val myId = state.state.value.playerId
                    // First player in the list is the host
                    val hostId = update.players.firstOrNull()?.playerId
                    val isHost = myId != null && myId == hostId
                    state.update {
                        // Preserve gameInfo from existing lobby state, look up from lobby list,
                        // or use pendingGameInfo (set when host creates a lobby)
                        val existingInfo =
                            it.currentLobby?.takeIf { c -> c.lobbyId == update.lobbyId }?.gameInfo
                                ?: it.lobbies.find { l -> l.lobbyId == update.lobbyId }?.gameInfo
                                ?: pendingGameInfo
                        it.copy(
                            currentLobby =
                                CurrentLobbyState(
                                    lobbyId = update.lobbyId,
                                    players = update.players,
                                    isHost = isHost,
                                    hostPlayerId = hostId,
                                    gameInfo = existingInfo,
                                ),
                            nav = MultiplayerNav.LOBBY,
                        )
                    }
                    state.appendLog(
                        "Lobby updated: ${update.lobbyId} (${update.players.size} players)",
                    )
                    NetLog.log("LOBBY", "Updated: ${update.lobbyId} players=${update.players.size} isHost=$isHost")
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
                    NetLog.log("SERVER", "Status: ${msg.data.onlinePlayers} online, ${msg.data.activeGamesCount} games")
                }

                is ServerMessage.GameStarting -> {
                    val gs = msg.data
                    val missionName = gs.gameInfo["mission"]?.jsonPrimitive?.content ?: ""
                    state.appendLog("Game starting: $missionName (slot ${gs.yourSlot})")
                    NetLog.log(
                        "GAME",
                        "Starting: ${gs.game} mission=$missionName slot=${gs.yourSlot} peers=${gs.peers.size}",
                    )

                    // Hand the shared candidate socket to LocalhostProxy so all
                    // peers share a single UDP socket (preserving UPnP/NAT pinholes).
                    val sharedSocket = candidateSocket
                    candidateSocket = null
                    // Clear residual soTimeout from STUN/connectivity probing
                    // so the proxy's receive loop blocks indefinitely.
                    sharedSocket?.soTimeout = 0

                    // Set up localhost proxy for each peer
                    localhostProxy?.shutdown()
                    val proxy = LocalhostProxy(scope, sharedRealSocket = sharedSocket)
                    for (peer in gs.peers) {
                        val addrParts = peer.addr.split(":")
                        if (addrParts.size != 2) {
                            state.appendLog("Bad peer addr: ${peer.addr}")
                            NetLog.log("ERROR", "Bad peer addr: ${peer.addr}")
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
                            mission = gs.gameInfo["mission"]?.jsonPrimitive?.content ?: "",
                            mode = gs.gameInfo["mode"]?.jsonPrimitive?.content ?: "",
                            difficulty = gs.gameInfo["difficulty"]?.jsonPrimitive?.intOrNull ?: 1,
                            levelNum = gs.gameInfo["level_num"]?.jsonPrimitive?.intOrNull ?: 1,
                            maxPlayers = gs.maxPlayers,
                            yourSlot = gs.yourSlot,
                            isHost = isHost,
                            peers = gs.peers,
                        )
                    state.update { it.copy(gameLaunchInfo = launchInfo) }
                    // Host sends periodic game state updates to the matchmaking server
                    if (isHost) {
                        startGameStateUpdates()
                    }
                }

                is ServerMessage.RateLimited -> {
                    state.appendLog("Rate limited, retry in ${msg.data.retryAfterMs}ms")
                    NetLog.log("SERVER", "Rate limited, retry in ${msg.data.retryAfterMs}ms")
                }

                is ServerMessage.VersionRejected -> {
                    state.update {
                        it.copy(errorMessage = "Version rejected: ${msg.data.reason}")
                    }
                    state.appendLog("Version rejected: ${msg.data.reason}")
                    NetLog.log("ERROR", "Version rejected: ${msg.data.reason}")
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
                    for (ci in msg.data.connections) {
                        val relay = if (ci.serverRelay) "relay" else "direct"
                        val detail = ci.detail?.let { " ($it)" } ?: ""
                        val latency = ci.estimatedLatencyMs?.let { " ${it}ms" } ?: ""
                        NetLog.log("CONNECTION", "${ci.peerCallsign}: ${ci.method} [$relay]$detail$latency")
                        state.appendLog("Connection: ${ci.peerCallsign} ${ci.method} [$relay]$latency")
                    }
                }

                is ServerMessage.PeerCandidatesReceived -> {
                    val pc = msg.data
                    state.update { s ->
                        val info = PeerNatInfo(pc.peerId, pc.candidates, pc.natType)
                        s.copy(
                            peerCandidates = s.peerCandidates + (pc.peerId to info),
                            iceStatus =
                                s.iceStatus.copy(
                                    peerCandidatesReceived = s.iceStatus.peerCandidatesReceived + 1,
                                ),
                        )
                    }
                    state.appendLog("Peer candidates: ${pc.peerId} (${pc.natType}, ${pc.candidates.size} candidates)")
                    NetLog.log("STUN", "Peer ${pc.peerId}: natType=${pc.natType} candidates=${pc.candidates.size}")
                }

                is ServerMessage.ConnectivityCheckGoReceived -> {
                    val pairs = msg.data.peerAddrs
                    state.update { s ->
                        s.copy(
                            connectivityPairs = pairs,
                            iceStatus = s.iceStatus.copy(phase = IcePhase.PROBING),
                        )
                    }
                    state.appendLog("Connectivity check GO: ${pairs.size} pairs to probe")
                    NetLog.log("HOLEPUNCH", "Connectivity check: ${pairs.size} pairs")
                    launchConnectivityCheck(pairs)
                }

                is ServerMessage.RelayAssignedReceived -> {
                    val relay = msg.data
                    state.update { it.copy(relayInfo = RelayInfo(relay.relayAddr, relay.sessionToken)) }
                    state.appendLog("Relay assigned: ${relay.relayAddr}")
                    NetLog.log("RELAY", "Assigned: ${relay.relayAddr} token=${relay.sessionToken}")
                }

                is ServerMessage.LateJoinProbeReceived -> {
                    val probe = msg.data
                    state.appendLog("Late-join probe: ${probe.joinerCallsign} (${probe.probeAddrs.size} addrs)")
                    NetLog.log("LATEJOIN", "Probe for ${probe.joinerCallsign}: ${probe.probeAddrs.size} addrs")
                    // Send blind probes from shared socket and enable probe echo
                    localhostProxy?.sendLateJoinProbes(probe.probeAddrs)
                }

                is ServerMessage.LateJoinApprovedReceived -> {
                    val peer = msg.data.peer
                    state.appendLog("Late-join approved: slot ${peer.slot} -> ${peer.addr}")
                    NetLog.log("LATEJOIN", "Approved: slot=${peer.slot} addr=${peer.addr} relay=${peer.isRelay}")
                    // Add the new peer to the existing proxy
                    val addrParts = peer.addr.split(":")
                    if (addrParts.size == 2) {
                        val port = addrParts[1].toIntOrNull()
                        if (port != null) {
                            val addr = InetSocketAddress(addrParts[0], port)
                            localhostProxy?.addPeer(
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
                    }
                }

                is ServerMessage.Unknown -> {
                    state.appendLog("Unknown message type: ${msg.type}")
                    NetLog.log("CONNECT", "Unknown message type: ${msg.type}")
                }

                is ServerMessage.MaintenanceReceived -> {
                    val m = msg.data
                    state.update { it.copy(maintenanceMessage = m.message) }
                    state.appendLog("Server maintenance: ${m.message}")
                    NetLog.log("CONNECT", "Maintenance shutdown: ${m.message}")
                    // Server is going down -- disconnect cleanly
                    manualDisconnect = true
                    disconnect()
                }

                is ServerMessage.MaintenanceWarningReceived -> {
                    val m = msg.data
                    val detail = m.shutdownAt?.let { " (shutdown at $it)" } ?: ""
                    state.update { it.copy(maintenanceMessage = "${m.message}$detail") }
                    state.appendLog("Maintenance warning: ${m.message}$detail")
                    NetLog.log("SERVER", "Maintenance warning: ${m.message}$detail")
                }

                is ServerMessage.FriendListReceived -> {
                    state.update { it.copy(friends = msg.data.friends) }
                }

                is ServerMessage.FriendRequestReceived -> {
                    val req = msg.data
                    state.update { s ->
                        val pending = s.pendingFriendRequests.filter { it.fromPlayerId != req.fromPlayerId } + req
                        s.copy(pendingFriendRequests = pending)
                    }
                    state.appendLog("Friend request from ${req.fromCallsign}")
                    NetLog.log("SOCIAL", "Friend request from ${req.fromCallsign}")
                }

                is ServerMessage.FriendAccepted -> {
                    state.appendLog("Friend accepted: ${msg.data.playerId}")
                    NetLog.log("SOCIAL", "Friend accepted: ${msg.data.playerId}")
                    requestFriendList()
                }

                is ServerMessage.FriendRemoved -> {
                    state.appendLog("Friend removed: ${msg.data.playerId}")
                    NetLog.log("SOCIAL", "Friend removed: ${msg.data.playerId}")
                    requestFriendList()
                }

                is ServerMessage.FriendPresenceUpdated -> {
                    val upd = msg.data
                    state.update { s ->
                        val updated =
                            s.friends.map { f ->
                                if (f.playerId == upd.playerId) {
                                    f.copy(presence = upd.presence, inGameDetails = upd.details)
                                } else {
                                    f
                                }
                            }
                        s.copy(friends = updated)
                    }
                }

                is ServerMessage.JoinFriendGameResponse -> {
                    val resp = msg.data
                    if (resp.success && resp.lobbyId != null) {
                        state.appendLog("Joining friend's lobby: ${resp.lobbyId}")
                        NetLog.log("SOCIAL", "Joining friend's lobby: ${resp.lobbyId}")
                        joinLobby(resp.lobbyId)
                    } else {
                        state.appendLog("Cannot join friend's game: ${resp.reason ?: "unknown"}")
                        NetLog.log("ERROR", "Cannot join friend's game: ${resp.reason ?: "unknown"}")
                        state.update { it.copy(errorMessage = resp.reason) }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "handleMessage error", e)
            state.appendLog("Internal error: ${e.message}")
            NetLog.log("ERROR", "Internal error: ${e.message}")
        }
    }
}
