package com.dxxredux.app.lobby

import android.content.Context
import android.net.wifi.WifiManager
import android.util.Log
import com.dxxredux.app.multiplayer.ClientIdentity
import com.dxxredux.app.multiplayer.NetLog
import com.dxxredux.app.multiplayer.NetworkConstants
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.NetworkInterface
import java.net.SocketTimeoutException
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong

/**
 * LAN lobby discovery service. Manages a UDP socket on port 42400 for
 * broadcasting/receiving lobby announcements and lobby membership messages.
 *
 * Two modes:
 * - **Discovery mode**: listens for ANNOUNCE packets from hosts, exposes
 *   discovered lobbies via [discoveredLobbies] StateFlow.
 * - **Host mode**: broadcasts ANNOUNCE every 3 seconds and manages a player
 *   list for the hosted lobby.
 */
object LobbyService {
    private const val TAG = "LobbyService"
    private const val RECV_BUF_SIZE = 2048
    private const val SOCKET_TIMEOUT_MS = 500
    private const val LOBBY_EXPIRY_MS = 10_000L
    private const val JOINED_HOST_TIMEOUT_MS = 15_000L
    private const val JOIN_RETRY_COUNT = 3
    private const val JOIN_RETRY_DELAY_MS = 1000L
    private const val BROADCAST_FAILURE_WARNING_THRESHOLD = 3

    // -- Public state --

    data class DiscoveredLobby(
        val announce: LanLobbyAnnounce,
        val lastSeenMs: Long = System.currentTimeMillis(),
    )

    private val _discoveredLobbies = MutableStateFlow<List<DiscoveredLobby>>(emptyList())
    val discoveredLobbies: StateFlow<List<DiscoveredLobby>> = _discoveredLobbies.asStateFlow()

    private val _hostedLobbyPlayers = MutableStateFlow<List<LanPlayer>>(emptyList())
    val hostedLobbyPlayers: StateFlow<List<LanPlayer>> = _hostedLobbyPlayers.asStateFlow()

    private val _isHosting = MutableStateFlow(false)
    val isHosting: StateFlow<Boolean> = _isHosting.asStateFlow()

    private val _isDiscovering = MutableStateFlow(false)
    val isDiscovering: StateFlow<Boolean> = _isDiscovering.asStateFlow()

    // Emitted when a game should launch (host pressed Start, or joiner received START)
    private val _lanLaunchEvent = MutableStateFlow<com.dxxredux.app.multiplayer.GameLaunchInfo?>(null)
    val lanLaunchEvent: StateFlow<com.dxxredux.app.multiplayer.GameLaunchInfo?> = _lanLaunchEvent.asStateFlow()

    // Joiner state: set when we successfully join a LAN lobby (JOIN_ACK received)
    data class JoinedLobbyInfo(
        val lobbyId: String,
        val hostAddr: String,
        val game: String,
        val mission: String,
        val mode: String,
        val maxPlayers: Int,
        val hostBuild: String = "",
        val hostCallsign: String? = null,
        val hostClientId: String? = null,
    )

    private val _joinedLobby = MutableStateFlow<JoinedLobbyInfo?>(null)
    val joinedLobby: StateFlow<JoinedLobbyInfo?> = _joinedLobby.asStateFlow()

    // Diagnostic counters
    internal val packetsSent = AtomicLong(0)
    internal val packetsReceived = AtomicLong(0)
    private val _diagnostics = MutableStateFlow("")
    val diagnostics: StateFlow<String> = _diagnostics.asStateFlow()
    private val _broadcastFailing = MutableStateFlow(false)
    val broadcastFailing: StateFlow<Boolean> = _broadcastFailing.asStateFlow()

    private val _chatMessages =
        MutableStateFlow<List<com.dxxredux.app.multiplayer.ChatMessage>>(emptyList())
    val chatMessages: StateFlow<List<com.dxxredux.app.multiplayer.ChatMessage>> =
        _chatMessages.asStateFlow()

    /** Clear the launch event after it has been consumed. */
    fun clearLaunchEvent() {
        _lanLaunchEvent.value = null
    }

    fun notifyAppBackgrounded() {
        if (!_isDiscovering.value) return
        appBackgrounded = true
        socketRefreshNeededOnResume = true
        NetLog.log("LAN", "App backgrounded with LAN discovery active")
    }

    fun notifyAppResumed(
        context: Context,
        callsign: String,
    ) {
        val wasBackgrounded = appBackgrounded || socketRefreshNeededOnResume
        appBackgrounded = false
        socketRefreshNeededOnResume = false
        if (!_isDiscovering.value) return

        hostCallsign = callsign
        resetTransientBroadcastFailure()
        val socketUnavailable = isSocketUnavailable()
        if (!shouldRefreshLanDiscoveryAfterResume(_isDiscovering.value, wasBackgrounded, socketUnavailable)) return

        NetLog.log(
            "LAN",
            "App resumed, refreshing LAN discovery socket " +
                "(wasBackgrounded=$wasBackgrounded, socketUnavailable=$socketUnavailable)",
        )
        try {
            openSocket(context)
            if (_isHosting.value) restartAnnounceLoop()
        } catch (e: Exception) {
            socketRefreshNeededOnResume = true
            _diagnostics.value = "LAN discovery resume failed: ${e.message ?: e.javaClass.simpleName}"
            Log.w(TAG, "Failed to refresh LAN discovery after resume: ${e.message}", e)
        }
    }

    // -- Internal state --

    private var scope: CoroutineScope? = null
    private var socket: DatagramSocket? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    private var receiveJob: Job? = null
    private var announceJob: Job? = null
    private var pruneJob: Job? = null
    private var joinRetryJob: Job? = null

    @Volatile private var lastHostSeenMs: Long = 0L

    @Volatile private var appBackgrounded = false

    @Volatile private var socketRefreshNeededOnResume = false

    // Keyed by lobbyId
    private val lobbies = ConcurrentHashMap<String, DiscoveredLobby>()

    // Host state -- accessed from UI + IO threads, needs @Volatile
    @Volatile private var hostedLobbyId: String? = null

    @Volatile private var hostedGame: String = "d2"

    @Volatile private var hostedMission: String = ""

    @Volatile private var hostedMode: String = "coop"

    @Volatile private var hostedMaxPlayers: Int = 4

    @Volatile private var hostCallsign: String = "Host"

    @Volatile private var localClientId: String? = null

    @Volatile private var gameStarted: Boolean = false

    @Volatile private var inGameDifficulty: Int = -1

    @Volatile private var inGameLevelNum: Int = -1

    // Host proxy port override (non-zero after host migration with proxy)
    @Volatile private var hostedHostPort: Int = NetworkConstants.ENGINE_PORT

    @Volatile private var hostedRestrictNonCoopFovToBase: Boolean = false

    /**
     * Start discovery mode. Acquires multicast lock, opens the UDP socket,
     * and begins listening for ANNOUNCE packets.
     */
    fun startDiscovery(
        context: Context,
        callsign: String,
    ) {
        if (_isDiscovering.value) return
        hostCallsign = callsign
        localClientId = ClientIdentity.getInstallationId(context)
        openSocket(context)
        _isDiscovering.value = true
        NetLog.log("LAN", "Discovery started on port ${NetworkConstants.LAN_LOBBY_PORT}, callsign=$callsign")
        Log.i(TAG, "LAN discovery started on port ${NetworkConstants.LAN_LOBBY_PORT}")
    }

    /** Stop discovery (and hosting if active). Releases all resources. */
    fun stopDiscovery() {
        NetLog.log(
            "LAN",
            "Discovery stopping (wasHosting=${_isHosting.value}, wasJoined=${_joinedLobby.value != null})",
        )
        _isDiscovering.value = false
        _isHosting.value = false
        hostedLobbyId = null
        lobbies.clear()
        _discoveredLobbies.value = emptyList()
        _hostedLobbyPlayers.value = emptyList()
        _lanLaunchEvent.value = null
        _joinedLobby.value = null
        joinRetryJob?.cancel()
        joinRetryJob = null
        packetsSent.set(0)
        packetsReceived.set(0)
        _diagnostics.value = ""
        _broadcastFailing.value = false
        consecutiveBroadcastFailures = 0
        appBackgrounded = false
        socketRefreshNeededOnResume = false
        hostedHostPort = NetworkConstants.ENGINE_PORT
        hostedRestrictNonCoopFovToBase = false
        localClientId = null
        closeSocket()
        NetLog.log("LAN", "Discovery stopped")
        Log.i(TAG, "LAN discovery stopped")
    }

    /**
     * Start hosting a LAN lobby. Begins broadcasting ANNOUNCE every 3 seconds.
     * Discovery must already be started.
     */
    fun hostLobby(
        callsign: String,
        game: String,
        mission: String,
        mode: String,
        maxPlayers: Int,
    ) {
        if (!_isDiscovering.value) return
        hostedLobbyId = UUID.randomUUID().toString()
        hostCallsign = callsign
        hostedGame = game
        hostedMission = mission
        hostedMode = mode
        hostedMaxPlayers = maxPlayers
        _hostedLobbyPlayers.value =
            listOf(
                LanPlayer(callsign = callsign, address = "127.0.0.1", clientId = localClientId, ready = true),
            )
        _isHosting.value = true
        gameStarted = false

        restartAnnounceLoop()
        NetLog.log("LAN", "Hosting lobby $hostedLobbyId ($game, $mission, $mode, max=$maxPlayers)")
        Log.i(TAG, "Hosting LAN lobby $hostedLobbyId ($game, $mission, $mode)")
    }

    /** Stop hosting (but keep discovery running). */
    fun stopHosting() {
        NetLog.log("LAN", "Stopped hosting lobby $hostedLobbyId")
        _isHosting.value = false
        announceJob?.cancel()
        announceJob = null
        hostedLobbyId = null
        hostedRestrictNonCoopFovToBase = false
        _hostedLobbyPlayers.value = emptyList()
        _chatMessages.value = emptyList()
        Log.i(TAG, "Stopped hosting LAN lobby")
    }

    /** Send a JOIN packet to the given host address, with retry. */
    fun joinLobby(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
    ) {
        Log.i(TAG, "joinLobby: lobbyId=$lobbyId host=$hostAddress callsign=$callsign")
        NetLog.log("LAN", "Joining lobby $lobbyId at $hostAddress as $callsign")
        Log.i(TAG, "joinLobby: socket=${socket != null} bound=${socket?.isBound} closed=${socket?.isClosed}")
        joinRetryJob?.cancel()
        joinRetryJob =
            scope?.launch(Dispatchers.IO) {
                val data = buildJoin(lobbyId, callsign, localClientId)
                for (attempt in 1..JOIN_RETRY_COUNT) {
                    if (_joinedLobby.value != null) {
                        Log.i(TAG, "joinLobby: already joined, stopping retries")
                        return@launch
                    }
                    Log.i(TAG, "joinLobby: attempt $attempt/$JOIN_RETRY_COUNT -> $hostAddress (${data.size} bytes)")
                    sendTo(data, hostAddress)
                    if (attempt < JOIN_RETRY_COUNT) delay(JOIN_RETRY_DELAY_MS)
                }
                // After retries, check if we got an ACK
                delay(JOIN_RETRY_DELAY_MS)
                if (_joinedLobby.value == null) {
                    Log.w(TAG, "joinLobby: no JOIN_ACK after $JOIN_RETRY_COUNT attempts")
                    _diagnostics.value = "Join failed: no response from $hostAddress"
                }
            }
    }

    /**
     * Join a lobby by IP: send QUERY to the host, wait for ANNOUNCE, then auto-join.
     * Used when broadcast discovery isn't working but you know the host's IP.
     */
    fun joinLobbyByIp(
        hostAddress: String,
        callsign: String,
        acceptLobby: (LanLobbyAnnounce) -> Boolean = { true },
    ) {
        NetLog.log("LAN", "Querying lobby at $hostAddress")
        Log.i(TAG, "joinLobbyByIp: querying $hostAddress")
        _diagnostics.value = "Querying $hostAddress..."
        joinRetryJob?.cancel()
        joinRetryJob =
            scope?.launch(Dispatchers.IO) {
                val query = buildQuery()
                for (attempt in 1..JOIN_RETRY_COUNT) {
                    sendTo(query, hostAddress)
                    delay(JOIN_RETRY_DELAY_MS)
                    // Check if we discovered a lobby from this host
                    val lobby =
                        _discoveredLobbies.value.find {
                            it.announce.hostAddress == hostAddress && acceptLobby(it.announce)
                        }
                    if (lobby != null) {
                        Log.i(TAG, "joinLobbyByIp: discovered lobby ${lobby.announce.lobbyId}, joining")
                        _diagnostics.value = ""
                        if (lobby.announce.status == "in_game") {
                            emitInGameJoinLaunch(lobby.announce)
                        } else {
                            joinLobby(lobby.announce.lobbyId, hostAddress, callsign)
                        }
                        return@launch
                    }
                }
                _diagnostics.value = "No matching lobby found at $hostAddress"
                Log.w(TAG, "joinLobbyByIp: no matching ANNOUNCE from $hostAddress after $JOIN_RETRY_COUNT attempts")
            }
    }

    /**
     * Quick lobby probe: send one QUERY and wait up to [timeoutMs] for an ANNOUNCE.
     * If a lobby is found, auto-join it and return true. Otherwise return false.
     * Runs on the caller's coroutine context (should be called from Dispatchers.IO).
     */
    suspend fun tryJoinLobbyByIp(
        hostAddress: String,
        callsign: String,
        timeoutMs: Long = 1000L,
        acceptLobby: (LanLobbyAnnounce) -> Boolean = { true },
    ): Boolean {
        Log.i(TAG, "tryJoinLobbyByIp: probing $hostAddress (timeout=${timeoutMs}ms)")
        val query = buildQuery()
        sendTo(query, hostAddress)
        // Poll for an ANNOUNCE response at 100ms intervals
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val lobby =
                _discoveredLobbies.value.find {
                    it.announce.hostAddress == hostAddress &&
                        acceptLobby(it.announce)
                }
            if (lobby != null) {
                Log.i(TAG, "tryJoinLobbyByIp: found lobby ${lobby.announce.lobbyId}, joining")
                if (lobby.announce.status == "in_game") {
                    emitInGameJoinLaunch(lobby.announce)
                } else {
                    joinLobby(lobby.announce.lobbyId, hostAddress, callsign)
                }
                return true
            }
            delay(100)
        }
        Log.i(TAG, "tryJoinLobbyByIp: no lobby found at $hostAddress within ${timeoutMs}ms")
        return false
    }

    /** Leave the LAN lobby we've joined (as a joiner). */
    fun leaveLanLobby(callsign: String) {
        val info = _joinedLobby.value ?: return
        NetLog.log("LAN", "Leaving lobby ${info.lobbyId} (host=${info.hostAddr})")
        val data = buildLeave(info.lobbyId, callsign)
        // Send LEAVE multiple times for UDP reliability (A9 fix)
        scope?.launch(Dispatchers.IO) {
            repeat(3) { attempt ->
                sendTo(data, info.hostAddr)
                if (attempt < 2) delay(100)
            }
        }
        _joinedLobby.value = null
        _hostedLobbyPlayers.value = emptyList()
        _chatMessages.value = emptyList()
        lastHostSeenMs = 0L
        Log.i(TAG, "Left LAN lobby ${info.lobbyId}")
    }

    /** Send a LEAVE packet to the given host address. */
    fun leaveLobby(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
    ) {
        val data = buildLeave(lobbyId, callsign)
        scope?.launch(Dispatchers.IO) { sendTo(data, hostAddress) }
        Log.i(TAG, "Sent LEAVE to $hostAddress for lobby $lobbyId")
    }

    /** Toggle ready state (as a joiner, send to host). */
    fun setReady(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
        ready: Boolean,
    ) {
        val data = buildReady(lobbyId, callsign, ready)
        scope?.launch(Dispatchers.IO) { sendTo(data, hostAddress) }
    }

    /** Send a chat message in the LAN lobby. Clients send to host, host relays to all. */
    fun sendChat(
        callsign: String,
        text: String,
    ) {
        val trimmed = text.take(200).trim()
        if (trimmed.isEmpty()) return
        val lid = hostedLobbyId ?: _joinedLobby.value?.lobbyId ?: return
        val data = buildChat(lid, callsign, trimmed)
        if (_isHosting.value) {
            // Host: add locally and relay to all joiners
            appendChatMessage(
                com.dxxredux.app.multiplayer
                    .ChatMessage(callsign, trimmed, isMe = true),
            )
            broadcastToJoiners(data)
        } else {
            // Joiner: send to host (host will relay back)
            val hostAddr = _joinedLobby.value?.hostAddr ?: return
            appendChatMessage(
                com.dxxredux.app.multiplayer
                    .ChatMessage(callsign, trimmed, isMe = true),
            )
            scope?.launch(Dispatchers.IO) { sendTo(data, hostAddr) }
        }
    }

    /** Kick a player from the hosted LAN lobby (host only). */
    fun kickPlayer(callsign: String) {
        if (!_isHosting.value) return
        val lid = hostedLobbyId ?: return
        val player = _hostedLobbyPlayers.value.find { it.callsign == callsign } ?: return
        // Send KICK to the player
        val data = buildKick(lid, callsign)
        scope?.launch(Dispatchers.IO) {
            repeat(2) { attempt ->
                sendTo(data, player.address)
                if (attempt < 1) delay(100)
            }
        }
        // Remove from player list
        _hostedLobbyPlayers.value = _hostedLobbyPlayers.value.filter { it.callsign != callsign }
        broadcastPlayerList()
        appendChatMessage(
            com.dxxredux.app.multiplayer
                .ChatMessage("System", "$callsign was kicked"),
        )
        Log.i(TAG, "Kicked player $callsign from lobby $lid")
    }

    /** Clear chat messages (called when leaving/stopping a lobby). */
    fun clearChat() {
        _chatMessages.value = emptyList()
    }

    private fun appendChatMessage(msg: com.dxxredux.app.multiplayer.ChatMessage) {
        val current = _chatMessages.value
        // Keep last 100 messages
        _chatMessages.value = if (current.size >= 100) current.drop(1) + msg else current + msg
    }

    /** Send data to all joiners (host only, skips self). */
    private fun broadcastToJoiners(data: ByteArray) {
        for (p in _hostedLobbyPlayers.value) {
            if (p.address != "127.0.0.1") {
                sendTo(data, p.address)
            }
        }
    }

    // ------------------------------------------------------------------
    // Internal
    // ------------------------------------------------------------------

    private fun openSocket(context: Context) {
        closeSocket()
        scope = CoroutineScope(Dispatchers.IO + Job())

        // Acquire multicast lock so Android doesn't filter broadcast/multicast
        val wifiManager =
            context.applicationContext
                .getSystemService(Context.WIFI_SERVICE) as? WifiManager
        multicastLock =
            wifiManager?.createMulticastLock("dxx-lan-discovery")?.apply {
                setReferenceCounted(false)
                acquire()
            }

        socket =
            DatagramSocket(null).apply {
                reuseAddress = true
                broadcast = true
                bind(InetSocketAddress(NetworkConstants.LAN_LOBBY_PORT))
                soTimeout = SOCKET_TIMEOUT_MS
            }
        NetLog.log(
            "LAN",
            "Socket opened: port=${socket?.localPort} bound=${socket?.isBound} broadcast=${socket?.broadcast}",
        )
        Log.i(TAG, "Socket opened: port=${socket?.localPort} bound=${socket?.isBound} broadcast=${socket?.broadcast}")
        NetLog.log("LAN", "Multicast lock: ${multicastLock?.isHeld}")
        Log.i(TAG, "Multicast lock acquired: ${multicastLock?.isHeld}")
        logLocalAddresses()

        // Receive loop
        receiveJob =
            scope?.launch(Dispatchers.IO) {
                val buf = ByteArray(RECV_BUF_SIZE)
                Log.i(TAG, "Receive loop started")
                while (isActive) {
                    try {
                        val packet = DatagramPacket(buf, buf.size)
                        socket?.receive(packet)
                        val senderAddr = packet.address.hostAddress ?: continue
                        val json = parsePacket(packet.data, packet.length)
                        if (json == null) {
                            Log.w(TAG, "recv: unparseable ${packet.length} bytes from $senderAddr")
                            continue
                        }
                        val rxCount = packetsReceived.incrementAndGet()
                        val msgType = json.optString("type", "?")
                        Log.d(TAG, "recv: $msgType from $senderAddr (${packet.length}B, total=$rxCount)")
                        handlePacket(json, senderAddr)
                    } catch (_: SocketTimeoutException) {
                        // Normal, just loop back to check isActive
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        if (isActive) {
                            NetLog.log("LAN", "Receive error: ${e.message}")
                            Log.w(TAG, "receive error: ${e.message}")
                        }
                    }
                }
                Log.i(TAG, "Receive loop ended")
            }

        // Prune stale lobbies
        pruneJob =
            scope?.launch(Dispatchers.IO) {
                while (isActive) {
                    delay(2000)
                    pruneStaleLobbies()
                }
            }
    }

    private fun closeSocket() {
        // Close socket first to unblock receive() immediately (A8 fix)
        try {
            socket?.close()
        } catch (_: Exception) {
            // ignore
        }
        socket = null
        scope?.cancel()
        scope = null
        receiveJob = null
        announceJob = null
        pruneJob = null
        try {
            multicastLock?.release()
        } catch (_: Exception) {
            // ignore
        }
        multicastLock = null
    }

    private fun handlePacket(
        json: JSONObject,
        senderAddr: String,
    ) {
        val type = json.optString("type")
        when (type) {
            MSG_ANNOUNCE -> {
                handleAnnounce(json, senderAddr)
            }

            MSG_JOIN -> {
                handleJoin(json, senderAddr)
            }

            MSG_LEAVE -> {
                handleLeave(json)
            }

            MSG_READY -> {
                handleReady(json)
            }

            MSG_PLAYER_LIST -> {
                handlePlayerList(json)
            }

            MSG_START -> {
                handleStart(json, senderAddr)
            }

            MSG_PING -> {
                handlePing(json, senderAddr)
            }

            MSG_PONG -> {}

            // future: calculate RTT
            MSG_JOIN_ACK -> {
                handleJoinAck(json, senderAddr)
            }

            MSG_JOIN_REJECT -> {
                handleJoinReject(json)
            }

            MSG_QUERY -> {
                handleQuery(senderAddr)
            }

            MSG_CHAT -> {
                handleChat(json, senderAddr)
            }

            MSG_KICK -> {
                handleKick(json)
            }

            else -> {
                Log.w(TAG, "Unknown packet type '$type' from $senderAddr")
            }
        }
    }

    private fun handleAnnounce(
        json: JSONObject,
        senderAddr: String,
    ) {
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId.isEmpty()) {
            Log.d(TAG, "handleAnnounce: empty lobby_id from $senderAddr, ignoring")
            return
        }
        if (lobbyId == hostedLobbyId) {
            Log.d(TAG, "handleAnnounce: own lobby from $senderAddr, ignoring")
            return
        }
        val isNew = !lobbies.containsKey(lobbyId)
        if (isNew) {
            NetLog.log(
                "LAN",
                "Discovered lobby $lobbyId from $senderAddr (${json.optString(
                    "callsign",
                    "?",
                )} ${json.optString("game", "?")}/${json.optString("mission", "?")})",
            )
        }
        Log.i(TAG, "handleAnnounce: ${if (isNew) "NEW" else "update"} lobby=$lobbyId from $senderAddr")

        val announce =
            LanLobbyAnnounce(
                lobbyId = lobbyId,
                callsign = json.optString("callsign", "Unknown"),
                game = json.optString("game", "d2"),
                mission = json.optString("mission", ""),
                mode = json.optString("mode", ""),
                playerCount = json.optInt("player_count", 1),
                maxPlayers = json.optInt("max_players", 4),
                hostAddress = senderAddr,
                build = json.optString("build", ""),
                status = json.optString("status", "lobby"),
                difficulty = json.optInt("difficulty", -1),
                levelNum = json.optInt("level_num", -1),
                hostPort = json.optInt("host_port", NetworkConstants.ENGINE_PORT),
                hostClientId = json.optString("host_client_id", "").takeIf { it.isNotBlank() },
                restrictNonCoopFovToBase = json.optBoolean("restrict_noncoop_fov_to_base", false),
            )
        lobbies[lobbyId] = DiscoveredLobby(announce = announce)
        publishLobbies()
        // Track host liveness: ANNOUNCE from the host of our joined lobby
        if (_joinedLobby.value?.lobbyId == lobbyId) {
            lastHostSeenMs = System.currentTimeMillis()
        }
    }

    private fun handleJoin(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value) {
            Log.d(TAG, "handleJoin: not hosting, ignoring JOIN from $senderAddr")
            return
        }
        if (gameStarted) {
            // Allow mid-game joins: send ACK with in-game info so the
            // joiner's C engine can negotiate via UPID_REQUEST/NETSTAT_PLAYING
            Log.i(TAG, "handleJoin: game in progress, allowing mid-game join from $senderAddr")
            val callsign = json.optString("callsign", "Player")
            sendTo(
                buildJoinAck(
                    hostedLobbyId ?: "",
                    hostedGame,
                    hostedMission,
                    hostedMode,
                    hostedMaxPlayers,
                    hostCallsign,
                    localClientId,
                ),
                senderAddr,
            )
            NetLog.log("LAN", "Mid-game JOIN_ACK sent to $callsign at $senderAddr")
            return
        }
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) {
            Log.w(TAG, "handleJoin: lobby mismatch expected=$hostedLobbyId got=$lobbyId from $senderAddr")
            sendTo(buildJoinReject(lobbyId, "unknown lobby"), senderAddr)
            return
        }

        val callsign = json.optString("callsign", "Player")
        val current = _hostedLobbyPlayers.value
        if (current.size >= hostedMaxPlayers) {
            NetLog.log(
                "LAN",
                "JOIN rejected: lobby full (${current.size}/$hostedMaxPlayers) for $callsign from $senderAddr",
            )
            Log.w(
                TAG,
                "handleJoin: lobby full (${current.size}/$hostedMaxPlayers), rejecting $callsign from $senderAddr",
            )
            sendTo(buildJoinReject(lobbyId, "lobby full"), senderAddr)
            return
        }
        val existing = current.find { it.callsign.equals(callsign, ignoreCase = true) }
        if (existing != null) {
            if (existing.address == senderAddr) {
                // Same player re-joining (retry/reconnect) -- bump lastSeen, re-send ACK
                Log.i(TAG, "handleJoin: $callsign re-joining from same addr $senderAddr, re-sending ACK")
                _hostedLobbyPlayers.value =
                    current.map { p ->
                        if (p.callsign.equals(callsign, ignoreCase = true)) {
                            p.copy(lastSeenMs = System.currentTimeMillis())
                        } else {
                            p
                        }
                    }
                sendTo(
                    buildJoinAck(
                        lobbyId,
                        hostedGame,
                        hostedMission,
                        hostedMode,
                        hostedMaxPlayers,
                        hostCallsign,
                        localClientId,
                    ),
                    senderAddr,
                )
                return
            } else {
                // Different player with duplicate callsign -- reject
                NetLog.log("LAN", "JOIN rejected: duplicate callsign '$callsign' from $senderAddr")
                Log.w(
                    TAG,
                    "handleJoin: duplicate callsign '$callsign' from $senderAddr (existing: ${existing.address})",
                )
                sendTo(buildJoinReject(lobbyId, "duplicate callsign"), senderAddr)
                return
            }
        }

        val updated =
            current +
                LanPlayer(
                    callsign = callsign,
                    address = senderAddr,
                    clientId = json.optString("client_id", "").takeIf { it.isNotBlank() },
                )
        _hostedLobbyPlayers.value = updated
        sendTo(
            buildJoinAck(
                lobbyId,
                hostedGame,
                hostedMission,
                hostedMode,
                hostedMaxPlayers,
                hostCallsign,
                localClientId,
            ),
            senderAddr,
        )
        broadcastPlayerList()
        NetLog.log("LAN", "Player joined: $callsign from $senderAddr (${updated.size} players)")
        Log.i(TAG, "Player joined: $callsign from $senderAddr")
    }

    private fun handleLeave(json: JSONObject) {
        if (!_isHosting.value) return
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) return

        val callsign = json.optString("callsign", "")
        val updated = _hostedLobbyPlayers.value.filter { !it.callsign.equals(callsign, ignoreCase = true) }
        _hostedLobbyPlayers.value = updated
        broadcastPlayerList()
        NetLog.log("LAN", "Player left: $callsign")
        Log.i(TAG, "Player left: $callsign")
    }

    private fun handleReady(json: JSONObject) {
        if (!_isHosting.value) return
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) return

        val callsign = json.optString("callsign", "")
        val ready = json.optBoolean("ready", false)
        val updated =
            _hostedLobbyPlayers.value.map { p ->
                if (p.callsign.equals(callsign, ignoreCase = true)) {
                    p.copy(ready = ready, lastSeenMs = System.currentTimeMillis())
                } else {
                    p
                }
            }
        _hostedLobbyPlayers.value = updated
        NetLog.log("LAN", "READY: $callsign -> $ready")
        broadcastPlayerList()
    }

    private fun handlePlayerList(json: JSONObject) {
        // Received by joiners -- update state for the lobby screen
        val lobbyId = json.optString("lobby_id", "")
        // Validate against our joined/hosted lobby
        val joined = _joinedLobby.value
        val isOurLobby = (joined != null && joined.lobbyId == lobbyId) || hostedLobbyId == lobbyId
        if (!isOurLobby) {
            Log.d(TAG, "handlePlayerList: ignoring for unknown lobby $lobbyId")
            return
        }
        val arr = json.optJSONArray("players") ?: return
        val players =
            (0 until arr.length()).map { i ->
                val pj = arr.getJSONObject(i)
                LanPlayer(
                    callsign = pj.optString("callsign", ""),
                    address = pj.optString("address", ""),
                    clientId = pj.optString("client_id", "").takeIf { it.isNotBlank() },
                    ready = pj.optBoolean("ready", false),
                )
            }
        NetLog.log("LAN", "PLAYER_LIST: ${players.size} players, lobby=$lobbyId")
        // Track host liveness for joined lobby timeout
        if (_joinedLobby.value?.lobbyId == lobbyId) {
            lastHostSeenMs = System.currentTimeMillis()
        }
        // Store in the lobby entry so UI can show it
        val existing = lobbies[lobbyId]
        if (existing != null) {
            lobbies[lobbyId] =
                existing.copy(
                    announce = existing.announce.copy(playerCount = players.size),
                )
            publishLobbies()
        }
        // Also publish to hostedLobbyPlayers for the joined-lobby view
        _hostedLobbyPlayers.value = players
    }

    private fun handleJoinAck(
        json: JSONObject,
        senderAddr: String,
    ) {
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId.isEmpty()) return
        joinRetryJob?.cancel()
        joinRetryJob = null
        _joinedLobby.value =
            JoinedLobbyInfo(
                lobbyId = lobbyId,
                hostAddr = senderAddr,
                game = json.optString("game", "d2"),
                mission = json.optString("mission", ""),
                mode = json.optString("mode", "coop"),
                maxPlayers = json.optInt("max_players", 4),
                hostBuild = json.optString("build", ""),
                hostCallsign = json.optString("host_callsign", "").takeIf { it.isNotBlank() },
                hostClientId = json.optString("host_client_id", "").takeIf { it.isNotBlank() },
            )
        NetLog.log("LAN", "JOIN_ACK received for lobby $lobbyId from $senderAddr")
        Log.i(TAG, "JOIN_ACK received for lobby $lobbyId from $senderAddr")
        lastHostSeenMs = System.currentTimeMillis()
    }

    private fun handleJoinReject(json: JSONObject) {
        val lobbyId = json.optString("lobby_id", "")
        val reason = json.optString("reason", "unknown")
        NetLog.log("LAN", "JOIN_REJECT for lobby $lobbyId: $reason")
        Log.w(TAG, "JOIN_REJECT for lobby $lobbyId: $reason")
        joinRetryJob?.cancel()
        joinRetryJob = null
        _diagnostics.value = "Join rejected: $reason"
        // Clear joined state in case we were in a retry
        if (_joinedLobby.value?.lobbyId == lobbyId) {
            _joinedLobby.value = null
        }
    }

    /**
     * Host calls this to start the game. Sends START to all joiners
     * and emits a launch event for the host side.
     */
    fun startGame(
        difficulty: Int,
        levelNum: Int,
        coopQol: Boolean = true,
        fullDeathSpew: Boolean = true,
        clientsCanRequestRewind: Boolean = false,
        restrictNonCoopFovToBase: Boolean = false,
        hostPort: Int = NetworkConstants.ENGINE_PORT,
    ) {
        if (!_isHosting.value) return
        val lid = hostedLobbyId ?: return
        val players = _hostedLobbyPlayers.value
        hostedHostPort = hostPort
        hostedRestrictNonCoopFovToBase = restrictNonCoopFovToBase

        // Send START to every joiner (redundant sends for reliability)
        val data =
            buildStart(
                lobbyId = lid,
                hostAddress = "0.0.0.0", // joiners use senderAddr
                hostPort = hostPort,
                game = hostedGame,
                mission = hostedMission,
                mode = hostedMode,
                difficulty = difficulty,
                levelNum = levelNum,
                maxPlayers = hostedMaxPlayers,
                coopQol = coopQol,
                fullDeathSpew = fullDeathSpew,
                clientsCanRequestRewind = clientsCanRequestRewind,
                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
            )
        // Mark game started (rejects further JOINs) but keep announcing
        // so in-game lobbies remain discoverable on LAN
        gameStarted = true
        inGameDifficulty = difficulty
        inGameLevelNum = levelNum

        // Capture values before launching coroutine (host fields are @Volatile)
        val game = hostedGame
        val mission = hostedMission
        val mode = hostedMode
        val maxPlayers = hostedMaxPlayers

        // Send START to every joiner on IO thread, then emit launch event
        // so the socket stays alive until sends complete (A7 fix)
        scope?.launch(Dispatchers.IO) {
            for (p in players) {
                if (p.address != "127.0.0.1") {
                    NetLog.log("LAN", "Sending START to ${p.callsign} at ${p.address}")
                    sendTo(data, p.address)
                }
            }
            // Redundant retries for unreliable networks
            for (retry in 1..2) {
                delay(200)
                for (p in players) {
                    if (p.address != "127.0.0.1") {
                        NetLog.log("LAN", "START retry $retry to ${p.callsign} at ${p.address}")
                        sendTo(data, p.address)
                    }
                }
            }
            // Emit launch event after sends complete
            _lanLaunchEvent.value =
                com.dxxredux.app.multiplayer.GameLaunchInfo(
                    game = game,
                    mission = mission,
                    mode = mode,
                    difficulty = difficulty,
                    levelNum = levelNum,
                    maxPlayers = maxPlayers,
                    yourSlot = 0,
                    isHost = true,
                    peers = emptyList(),
                    isLan = true,
                    hostCallsign = hostCallsign,
                    hostClientId = localClientId,
                    coopQol = coopQol,
                    fullDeathSpew = fullDeathSpew,
                    clientsCanRequestRewind = clientsCanRequestRewind,
                    restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                )
            NetLog.log("LAN", "Game started: $game/$mission lvl=$levelNum diff=$difficulty")
            Log.i(TAG, "Game started: $game/$mission lvl=$levelNum diff=$difficulty")
        }
    }

    /** Stop the in-game announce loop when the game exits back to setup.
     *  If still hosting, restart lobby-mode announces so the lobby remains
     *  discoverable for new joins. */
    fun stopInGameBroadcast() {
        announceJob?.cancel()
        announceJob = null
        gameStarted = false
        inGameDifficulty = -1
        inGameLevelNum = -1
        // Restart lobby announces if we're still hosting
        if (_isHosting.value && _isDiscovering.value) {
            restartAnnounceLoop()
        }
    }

    private fun handleStart(
        json: JSONObject,
        senderAddr: String,
    ) {
        val lobbyId = json.optString("lobby_id", "")
        val hostPort = json.optInt("host_port", NetworkConstants.ENGINE_PORT)
        val game = json.optString("game", "d2")
        val mission = json.optString("mission", "")
        val mode = json.optString("mode", "coop")
        val difficulty = json.optInt("difficulty", 1)
        val levelNum = json.optInt("level_num", 1)
        val maxPlayers = json.optInt("max_players", 4)
        val coopQol = json.optBoolean("coop_qol", true)
        val fullDeathSpew = json.optBoolean("full_death_spew", true)
        val clientsCanRequestRewind = json.optBoolean("clients_can_request_rewind", false)
        val restrictNonCoopFovToBase = json.optBoolean("restrict_noncoop_fov_to_base", false)
        NetLog.log("LAN", "START received: $game/$mission lvl=$levelNum diff=$difficulty from $senderAddr")
        Log.i(TAG, "START received for lobby $lobbyId: $game/$mission at $senderAddr:$hostPort")

        val joinedInfo = _joinedLobby.value
        if (joinedInfo == null) {
            NetLog.log("LAN", "START ignored: not in a joined lobby")
            Log.w(TAG, "START received but not in a joined lobby, ignoring")
            return
        }
        if (lobbyId.isNotEmpty() && lobbyId != joinedInfo.lobbyId) {
            NetLog.log("LAN", "START ignored: lobbyId mismatch (got=$lobbyId, joined=${joinedInfo.lobbyId})")
            Log.w(TAG, "START lobbyId mismatch: got=$lobbyId expected=${joinedInfo.lobbyId}")
            return
        }

        // Emit launch event for the joiner
        _lanLaunchEvent.value =
            com.dxxredux.app.multiplayer.GameLaunchInfo(
                game = game,
                mission = mission,
                mode = mode,
                difficulty = difficulty,
                levelNum = levelNum,
                maxPlayers = maxPlayers,
                yourSlot = 1, // non-zero = joiner
                isHost = false,
                peers = emptyList(),
                lanHostAddr = senderAddr,
                lanHostPort = hostPort,
                isLan = true,
                hostCallsign = joinedInfo.hostCallsign,
                hostClientId = joinedInfo.hostClientId,
                coopQol = coopQol,
                fullDeathSpew = fullDeathSpew,
                clientsCanRequestRewind = clientsCanRequestRewind,
                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
            )
        NetLog.log("LAN", "Launch event emitted for joiner: game=$game host=$senderAddr")
    }

    private fun emitInGameJoinLaunch(announce: LanLobbyAnnounce) {
        _lanLaunchEvent.value =
            com.dxxredux.app.multiplayer.GameLaunchInfo(
                game = announce.game,
                mission = announce.mission,
                mode = announce.mode,
                difficulty = announce.difficulty.takeIf { it >= 0 } ?: 1,
                levelNum = announce.levelNum.takeIf { it >= 1 } ?: 1,
                maxPlayers = announce.maxPlayers,
                yourSlot = 1,
                isHost = false,
                peers = emptyList(),
                lanHostAddr = announce.hostAddress,
                lanHostPort = announce.hostPort,
                isLan = true,
                hostCallsign = announce.callsign,
                hostClientId = announce.hostClientId,
                restrictNonCoopFovToBase = announce.restrictNonCoopFovToBase,
            )
        NetLog.log(
            "LAN",
            "Launch event emitted for in-game LAN join: game=${announce.game} host=${announce.hostAddress}",
        )
    }

    private fun handlePing(
        json: JSONObject,
        senderAddr: String,
    ) {
        val lobbyId = json.optString("lobby_id", "")
        // Only respond if the ping is for our hosted lobby (A11 fix)
        if (lobbyId.isEmpty() || lobbyId != hostedLobbyId) return
        val ts = json.optLong("ts", 0)
        val pong = buildPong(lobbyId, ts)
        sendTo(pong, senderAddr)
    }

    /** Respond to a QUERY with a direct ANNOUNCE so the querier discovers our lobby. */
    private fun handleQuery(senderAddr: String) {
        val lid = hostedLobbyId ?: return // not hosting
        val data =
            buildAnnounce(
                lobbyId = lid,
                callsign = hostCallsign,
                game = hostedGame,
                mission = hostedMission,
                mode = hostedMode,
                playerCount = _hostedLobbyPlayers.value.size,
                maxPlayers = hostedMaxPlayers,
                status = if (gameStarted) "in_game" else "lobby",
                difficulty = inGameDifficulty,
                levelNum = inGameLevelNum,
                hostPort = hostedHostPort,
                hostClientId = localClientId,
                restrictNonCoopFovToBase = hostedRestrictNonCoopFovToBase,
            )
        sendTo(data, senderAddr)
        Log.i(TAG, "handleQuery: sent ANNOUNCE to $senderAddr for lobby $lid")
    }

    private fun handleChat(
        json: JSONObject,
        senderAddr: String,
    ) {
        val callsign = json.optString("callsign", "")
        val text = json.optString("text", "").take(200)
        if (callsign.isEmpty() || text.isEmpty()) return

        if (_isHosting.value) {
            // Host received chat from a joiner -- add and relay to all joiners
            appendChatMessage(
                com.dxxredux.app.multiplayer
                    .ChatMessage(callsign, text),
            )
            val lid = hostedLobbyId ?: return
            val relay = buildChat(lid, callsign, text)
            // Relay to all joiners except the sender
            for (p in _hostedLobbyPlayers.value) {
                if (p.address != "127.0.0.1" && p.address != senderAddr) {
                    sendTo(relay, p.address)
                }
            }
        } else {
            // Joiner received relayed chat from host
            appendChatMessage(
                com.dxxredux.app.multiplayer
                    .ChatMessage(callsign, text),
            )
        }
    }

    private fun handleKick(json: JSONObject) {
        // Joiners receive KICK from host -- leave the lobby
        val joined = _joinedLobby.value ?: return
        _joinedLobby.value = null
        _hostedLobbyPlayers.value = emptyList()
        _chatMessages.value = emptyList()
        lastHostSeenMs = 0L
        _diagnostics.value = "Kicked from lobby by host"
        Log.i(TAG, "Kicked from lobby ${joined.lobbyId}")
    }

    private fun broadcastAnnounce() {
        val lid = hostedLobbyId ?: return
        val data =
            buildAnnounce(
                lobbyId = lid,
                callsign = hostCallsign,
                game = hostedGame,
                mission = hostedMission,
                mode = hostedMode,
                playerCount = _hostedLobbyPlayers.value.size,
                maxPlayers = hostedMaxPlayers,
                status = if (gameStarted) "in_game" else "lobby",
                difficulty = inGameDifficulty,
                levelNum = inGameLevelNum,
                hostPort = hostedHostPort,
                hostClientId = localClientId,
                restrictNonCoopFovToBase = hostedRestrictNonCoopFovToBase,
            )
        sendBroadcast(data)
    }

    private fun broadcastPlayerList() {
        val lid = hostedLobbyId ?: return
        val data = buildPlayerList(lid, _hostedLobbyPlayers.value)
        broadcastToJoiners(data)
    }

    @Volatile private var consecutiveBroadcastFailures: Int = 0

    private fun sendBroadcast(data: ByteArray) {
        val activeSocket = socket
        if (activeSocket == null || activeSocket.isClosed) {
            NetLog.log("LAN", "Broadcast send deferred: socket unavailable")
            noteBroadcastFailure()
            return
        }
        val addresses = getBroadcastAddresses()
        var anySuccess = false
        for (addr in addresses) {
            try {
                val packet =
                    DatagramPacket(
                        data,
                        data.size,
                        addr,
                        NetworkConstants.LAN_LOBBY_PORT,
                    )
                activeSocket.send(packet)
                anySuccess = true
                val txCount = packetsSent.incrementAndGet()
                Log.d(
                    TAG,
                    "broadcast: ${data.size}B to ${addr.hostAddress}:${NetworkConstants.LAN_LOBBY_PORT} (total=$txCount)",
                )
            } catch (e: Exception) {
                NetLog.log("LAN", "Broadcast send FAILED to ${addr.hostAddress}: ${e.message}")
                Log.w(TAG, "broadcast send error to ${addr.hostAddress}: ${e.message}", e)
            }
        }
        if (anySuccess) {
            noteBroadcastSuccess()
        } else {
            noteBroadcastFailure()
        }
    }

    private fun noteBroadcastSuccess() {
        consecutiveBroadcastFailures = 0
        _broadcastFailing.value = false
        _diagnostics.value = lanDiagnosticAfterBroadcastRecovery(_diagnostics.value)
    }

    private fun noteBroadcastFailure() {
        if (!shouldShowBroadcastFailureWarning(appBackgrounded)) {
            socketRefreshNeededOnResume = true
            return
        }
        consecutiveBroadcastFailures++
        if (consecutiveBroadcastFailures == BROADCAST_FAILURE_WARNING_THRESHOLD) {
            _diagnostics.value = LAN_BROADCAST_FAILURE_DIAGNOSTIC
            _broadcastFailing.value = true
        }
    }

    /**
     * Compute broadcast addresses for all active non-loopback IPv4 interfaces.
     * Subnet-directed broadcasts (e.g. 192.168.1.255) are more reliable than
     * 255.255.255.255 on Android and many consumer APs. Falls back to
     * 255.255.255.255 if no interface addresses can be determined.
     */
    private fun getBroadcastAddresses(): List<InetAddress> {
        val addrs = mutableListOf<InetAddress>()
        try {
            for (iface in NetworkInterface.getNetworkInterfaces()?.toList().orEmpty()) {
                if (!iface.isUp || iface.isLoopback) continue
                for (ifAddr in iface.interfaceAddresses) {
                    val broadcast = ifAddr.broadcast
                    if (broadcast != null && !addrs.contains(broadcast)) {
                        addrs.add(broadcast)
                    }
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to enumerate broadcast addresses: ${e.message}")
        }
        if (addrs.isEmpty()) {
            addrs.add(InetAddress.getByName("255.255.255.255"))
        }
        return addrs
    }

    private fun sendTo(
        data: ByteArray,
        address: String,
    ) {
        try {
            val activeSocket = socket
            if (activeSocket == null || activeSocket.isClosed) {
                NetLog.log("LAN", "Send deferred to $address: socket unavailable")
                return
            }
            val addr = InetAddress.getByName(address)
            val packet =
                DatagramPacket(
                    data,
                    data.size,
                    addr,
                    NetworkConstants.LAN_LOBBY_PORT,
                )
            activeSocket.send(packet)
            val txCount = packetsSent.incrementAndGet()
            Log.d(TAG, "sendTo: ${data.size}B -> $address:${NetworkConstants.LAN_LOBBY_PORT} (total=$txCount)")
        } catch (e: Exception) {
            NetLog.log("LAN", "Send FAILED to $address: ${e.message}")
            Log.w(TAG, "sendTo error $address: ${e.message}", e)
        }
    }

    private fun pruneStaleLobbies() {
        if (appBackgrounded) return
        val now = System.currentTimeMillis()
        val removed = lobbies.entries.removeAll { (now - it.value.lastSeenMs) > LOBBY_EXPIRY_MS }
        if (removed) publishLobbies()

        // A6: prune stale joiners when hosting (no packets for 10s)
        if (_isHosting.value) {
            val players = _hostedLobbyPlayers.value
            val stale = players.filter { it.address != "127.0.0.1" && (now - it.lastSeenMs) > LOBBY_EXPIRY_MS }
            if (stale.isNotEmpty()) {
                val updated = players - stale.toSet()
                _hostedLobbyPlayers.value = updated
                for (p in stale) {
                    NetLog.log("LAN", "Pruned stale joiner: ${p.callsign} (${p.address})")
                    Log.i(TAG, "Pruned stale joiner: ${p.callsign} from ${p.address}")
                }
                broadcastPlayerList()
            }
        }

        // Check joined lobby liveness
        val joined = _joinedLobby.value
        if (joined != null && lastHostSeenMs > 0L && (now - lastHostSeenMs) > JOINED_HOST_TIMEOUT_MS) {
            NetLog.log("LAN", "Host timeout: no packets from ${joined.hostAddr} in ${(now - lastHostSeenMs) / 1000}s")
            Log.w(TAG, "Joined lobby ${joined.lobbyId} host timed out")
            _joinedLobby.value = null
            _hostedLobbyPlayers.value = emptyList()
            _diagnostics.value = "Host disconnected (timeout)"
            lastHostSeenMs = 0L
        }
    }

    private fun publishLobbies() {
        _discoveredLobbies.value = lobbies.values.toList()
    }

    private fun logLocalAddresses() {
        try {
            val addrs =
                NetworkInterface
                    .getNetworkInterfaces()
                    ?.toList()
                    .orEmpty()
                    .filter { it.isUp && !it.isLoopback }
                    .flatMap { iface ->
                        iface.inetAddresses.toList().map { "${iface.name}: ${it.hostAddress}" }
                    }
            NetLog.log("LAN", "Local addresses: $addrs")
            Log.i(TAG, "Local addresses: $addrs")
        } catch (e: Exception) {
            NetLog.log("LAN", "Failed to enumerate addresses: ${e.message}")
            Log.w(TAG, "Failed to enumerate local addresses: ${e.message}")
        }
    }

    private fun restartAnnounceLoop() {
        announceJob?.cancel()
        announceJob =
            scope?.launch(Dispatchers.IO) {
                while (isActive && _isHosting.value) {
                    broadcastAnnounce()
                    delay(NetworkConstants.LAN_ANNOUNCE_INTERVAL_MS)
                }
            }
    }

    private fun resetTransientBroadcastFailure() {
        consecutiveBroadcastFailures = 0
        _broadcastFailing.value = false
        _diagnostics.value = lanDiagnosticAfterBroadcastRecovery(_diagnostics.value)
    }

    private fun isSocketUnavailable(): Boolean {
        val activeSocket = socket
        return activeSocket == null || activeSocket.isClosed || !activeSocket.isBound
    }
}
