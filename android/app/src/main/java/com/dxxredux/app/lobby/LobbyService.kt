package com.dxxredux.app.lobby

import android.content.Context
import android.net.wifi.WifiManager
import android.util.Log
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

    /** Clear the launch event after it has been consumed. */
    fun clearLaunchEvent() {
        _lanLaunchEvent.value = null
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

    // Keyed by lobbyId
    private val lobbies = ConcurrentHashMap<String, DiscoveredLobby>()

    // Host state -- accessed from UI + IO threads, needs @Volatile
    @Volatile private var hostedLobbyId: String? = null

    @Volatile private var hostedGame: String = "d2"

    @Volatile private var hostedMission: String = ""

    @Volatile private var hostedMode: String = "coop"

    @Volatile private var hostedMaxPlayers: Int = 4

    @Volatile private var hostCallsign: String = "Host"

    @Volatile private var gameStarted: Boolean = false

    @Volatile private var inGameDifficulty: Int = -1

    @Volatile private var inGameLevelNum: Int = -1

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
                LanPlayer(callsign = callsign, address = "127.0.0.1", ready = true),
            )
        _isHosting.value = true
        gameStarted = false

        announceJob?.cancel()
        announceJob =
            scope?.launch(Dispatchers.IO) {
                while (isActive && _isHosting.value) {
                    broadcastAnnounce()
                    delay(NetworkConstants.LAN_ANNOUNCE_INTERVAL_MS)
                }
            }
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
        _hostedLobbyPlayers.value = emptyList()
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
                val data = buildJoin(lobbyId, callsign)
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
                            it.announce.hostAddress == hostAddress
                        }
                    if (lobby != null) {
                        Log.i(TAG, "joinLobbyByIp: discovered lobby ${lobby.announce.lobbyId}, joining")
                        _diagnostics.value = ""
                        joinLobby(lobby.announce.lobbyId, hostAddress, callsign)
                        return@launch
                    }
                }
                _diagnostics.value = "No lobby found at $hostAddress"
                Log.w(TAG, "joinLobbyByIp: no ANNOUNCE from $hostAddress after $JOIN_RETRY_COUNT attempts")
            }
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
            MSG_ANNOUNCE -> handleAnnounce(json, senderAddr)
            MSG_JOIN -> handleJoin(json, senderAddr)
            MSG_LEAVE -> handleLeave(json)
            MSG_READY -> handleReady(json)
            MSG_PLAYER_LIST -> handlePlayerList(json)
            MSG_START -> handleStart(json, senderAddr)
            MSG_PING -> handlePing(json, senderAddr)
            MSG_PONG -> {} // future: calculate RTT
            MSG_JOIN_ACK -> handleJoinAck(json, senderAddr)
            MSG_JOIN_REJECT -> handleJoinReject(json)
            MSG_QUERY -> handleQuery(senderAddr)
            else -> Log.w(TAG, "Unknown packet type '$type' from $senderAddr")
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
            Log.d(TAG, "handleJoin: game already started, rejecting JOIN from $senderAddr")
            sendTo(buildJoinReject(hostedLobbyId ?: "", "game already started"), senderAddr)
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
                    buildJoinAck(lobbyId, hostedGame, hostedMission, hostedMode, hostedMaxPlayers),
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

        val updated = current + LanPlayer(callsign = callsign, address = senderAddr)
        _hostedLobbyPlayers.value = updated
        sendTo(
            buildJoinAck(lobbyId, hostedGame, hostedMission, hostedMode, hostedMaxPlayers),
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
    ) {
        if (!_isHosting.value) return
        val lid = hostedLobbyId ?: return
        val players = _hostedLobbyPlayers.value

        // Send START to every joiner (redundant sends for reliability)
        val data =
            buildStart(
                lobbyId = lid,
                hostAddress = "0.0.0.0", // joiners use senderAddr
                hostPort = NetworkConstants.ENGINE_PORT,
                game = hostedGame,
                mission = hostedMission,
                mode = hostedMode,
                difficulty = difficulty,
                levelNum = levelNum,
                maxPlayers = hostedMaxPlayers,
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
                )
            NetLog.log("LAN", "Game started: $game/$mission lvl=$levelNum diff=$difficulty")
            Log.i(TAG, "Game started: $game/$mission lvl=$levelNum diff=$difficulty")
        }
    }

    /** Stop the in-game announce loop when the game exits back to setup */
    fun stopInGameBroadcast() {
        announceJob?.cancel()
        announceJob = null
        gameStarted = false
        inGameDifficulty = -1
        inGameLevelNum = -1
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
                isLan = true,
            )
        NetLog.log("LAN", "Launch event emitted for joiner: game=$game host=$senderAddr")
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
            )
        sendTo(data, senderAddr)
        Log.i(TAG, "handleQuery: sent ANNOUNCE to $senderAddr for lobby $lid")
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
            )
        sendBroadcast(data)
    }

    private fun broadcastPlayerList() {
        val lid = hostedLobbyId ?: return
        val data = buildPlayerList(lid, _hostedLobbyPlayers.value)
        // Send to each joiner (not ourselves)
        for (p in _hostedLobbyPlayers.value) {
            if (p.address != "127.0.0.1") {
                sendTo(data, p.address)
            }
        }
    }

    @Volatile private var consecutiveBroadcastFailures: Int = 0

    private fun sendBroadcast(data: ByteArray) {
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
                socket?.send(packet)
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
            consecutiveBroadcastFailures = 0
            _broadcastFailing.value = false
        } else {
            consecutiveBroadcastFailures++
            if (consecutiveBroadcastFailures == 3) {
                _diagnostics.value = "Broadcasts failing -- check Wi-Fi and Nearby Devices permission"
                _broadcastFailing.value = true
            }
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
            val addr = InetAddress.getByName(address)
            val packet =
                DatagramPacket(
                    data,
                    data.size,
                    addr,
                    NetworkConstants.LAN_LOBBY_PORT,
                )
            socket?.send(packet)
            val txCount = packetsSent.incrementAndGet()
            Log.d(TAG, "sendTo: ${data.size}B -> $address:${NetworkConstants.LAN_LOBBY_PORT} (total=$txCount)")
        } catch (e: Exception) {
            NetLog.log("LAN", "Send FAILED to $address: ${e.message}")
            Log.w(TAG, "sendTo error $address: ${e.message}", e)
        }
    }

    private fun pruneStaleLobbies() {
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
}
