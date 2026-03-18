package com.dxxredux.app.lobby

import android.content.Context
import android.net.wifi.WifiManager
import android.util.Log
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
    )

    private val _joinedLobby = MutableStateFlow<JoinedLobbyInfo?>(null)
    val joinedLobby: StateFlow<JoinedLobbyInfo?> = _joinedLobby.asStateFlow()

    // Diagnostic counters
    private val packetsSent = AtomicLong(0)
    private val packetsReceived = AtomicLong(0)
    private val _diagnostics = MutableStateFlow("")
    val diagnostics: StateFlow<String> = _diagnostics.asStateFlow()

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

    // Keyed by lobbyId
    private val lobbies = ConcurrentHashMap<String, DiscoveredLobby>()

    // Host state
    private var hostedLobbyId: String? = null
    private var hostedGame: String = "d2"
    private var hostedMission: String = ""
    private var hostedMode: String = "coop"
    private var hostedMaxPlayers: Int = 4
    private var hostCallsign: String = "Host"

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
        Log.i(TAG, "LAN discovery started on port ${NetworkConstants.LAN_LOBBY_PORT}")
    }

    /** Stop discovery (and hosting if active). Releases all resources. */
    fun stopDiscovery() {
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
        closeSocket()
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
                LanPlayer(callsign = callsign, address = "127.0.0.1", ready = false),
            )
        _isHosting.value = true

        announceJob?.cancel()
        announceJob =
            scope?.launch(Dispatchers.IO) {
                while (isActive && _isHosting.value) {
                    broadcastAnnounce()
                    delay(NetworkConstants.LAN_ANNOUNCE_INTERVAL_MS)
                }
            }
        Log.i(TAG, "Hosting LAN lobby $hostedLobbyId ($game, $mission, $mode)")
    }

    /** Stop hosting (but keep discovery running). */
    fun stopHosting() {
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

    /** Leave the LAN lobby we've joined (as a joiner). */
    fun leaveLanLobby(callsign: String) {
        val info = _joinedLobby.value ?: return
        val data = buildLeave(info.lobbyId, callsign)
        sendTo(data, info.hostAddr)
        _joinedLobby.value = null
        _hostedLobbyPlayers.value = emptyList()
        Log.i(TAG, "Left LAN lobby ${info.lobbyId}")
    }

    /** Send a LEAVE packet to the given host address. */
    fun leaveLobby(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
    ) {
        val data = buildLeave(lobbyId, callsign)
        sendTo(data, hostAddress)
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
        sendTo(data, hostAddress)
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
        Log.i(TAG, "Socket opened: port=${socket?.localPort} bound=${socket?.isBound} broadcast=${socket?.broadcast}")
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
                        if (isActive) Log.w(TAG, "receive error: ${e.message}")
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
        scope?.cancel()
        scope = null
        receiveJob = null
        announceJob = null
        pruneJob = null
        try {
            socket?.close()
        } catch (_: Exception) {
            // ignore
        }
        socket = null
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
            )
        lobbies[lobbyId] = DiscoveredLobby(announce = announce)
        publishLobbies()
    }

    private fun handleJoin(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value) {
            Log.d(TAG, "handleJoin: not hosting, ignoring JOIN from $senderAddr")
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
            Log.w(
                TAG,
                "handleJoin: lobby full (${current.size}/$hostedMaxPlayers), rejecting $callsign from $senderAddr",
            )
            sendTo(buildJoinReject(lobbyId, "lobby full"), senderAddr)
            return
        }
        if (current.any { it.callsign == callsign }) {
            Log.i(TAG, "handleJoin: $callsign already in lobby, re-sending ACK to $senderAddr")
            sendTo(
                buildJoinAck(lobbyId, hostedGame, hostedMission, hostedMode, hostedMaxPlayers),
                senderAddr,
            )
            return
        }

        val updated = current + LanPlayer(callsign = callsign, address = senderAddr)
        _hostedLobbyPlayers.value = updated
        sendTo(
            buildJoinAck(lobbyId, hostedGame, hostedMission, hostedMode, hostedMaxPlayers),
            senderAddr,
        )
        broadcastPlayerList()
        Log.i(TAG, "Player joined: $callsign from $senderAddr")
    }

    private fun handleLeave(json: JSONObject) {
        if (!_isHosting.value) return
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) return

        val callsign = json.optString("callsign", "")
        val updated = _hostedLobbyPlayers.value.filter { it.callsign != callsign }
        _hostedLobbyPlayers.value = updated
        broadcastPlayerList()
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
                if (p.callsign == callsign) p.copy(ready = ready) else p
            }
        _hostedLobbyPlayers.value = updated
        broadcastPlayerList()
    }

    private fun handlePlayerList(json: JSONObject) {
        // Received by joiners -- update state for the lobby screen
        val lobbyId = json.optString("lobby_id", "")
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
        _joinedLobby.value =
            JoinedLobbyInfo(
                lobbyId = lobbyId,
                hostAddr = senderAddr,
                game = json.optString("game", "d2"),
                mission = json.optString("mission", ""),
                mode = json.optString("mode", "coop"),
                maxPlayers = json.optInt("max_players", 4),
            )
        Log.i(TAG, "JOIN_ACK received for lobby $lobbyId from $senderAddr")
    }

    private fun handleJoinReject(json: JSONObject) {
        val lobbyId = json.optString("lobby_id", "")
        val reason = json.optString("reason", "unknown")
        Log.w(TAG, "JOIN_REJECT for lobby $lobbyId: $reason")
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

        // Send START to every joiner
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
        for (p in players) {
            if (p.address != "127.0.0.1") {
                sendTo(data, p.address)
            }
        }

        // Stop announcing
        announceJob?.cancel()
        announceJob = null

        // Emit launch event for the host
        _lanLaunchEvent.value =
            com.dxxredux.app.multiplayer.GameLaunchInfo(
                game = hostedGame,
                mission = hostedMission,
                mode = hostedMode,
                difficulty = difficulty,
                levelNum = levelNum,
                maxPlayers = hostedMaxPlayers,
                yourSlot = 0,
                isHost = true,
                peers = emptyList(),
                isLan = true,
            )
        Log.i(TAG, "Game started: $hostedGame/$hostedMission lvl=$levelNum diff=$difficulty")
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
        Log.i(TAG, "START received for lobby $lobbyId: $game/$mission at $senderAddr:$hostPort")

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
    }

    private fun handlePing(
        json: JSONObject,
        senderAddr: String,
    ) {
        val lobbyId = json.optString("lobby_id", "")
        val ts = json.optLong("ts", 0)
        val pong = buildPong(lobbyId, ts)
        sendTo(pong, senderAddr)
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

    private fun sendBroadcast(data: ByteArray) {
        try {
            val addr = InetAddress.getByName("255.255.255.255")
            val packet =
                DatagramPacket(
                    data,
                    data.size,
                    addr,
                    NetworkConstants.LAN_LOBBY_PORT,
                )
            socket?.send(packet)
            val txCount = packetsSent.incrementAndGet()
            Log.d(
                TAG,
                "broadcast: ${data.size}B to 255.255.255.255:${NetworkConstants.LAN_LOBBY_PORT} (total=$txCount)",
            )
        } catch (e: Exception) {
            Log.w(TAG, "broadcast send error: ${e.message}", e)
        }
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
            Log.w(TAG, "sendTo error $address: ${e.message}", e)
        }
    }

    private fun pruneStaleLobbies() {
        val now = System.currentTimeMillis()
        val removed = lobbies.entries.removeAll { (now - it.value.lastSeenMs) > LOBBY_EXPIRY_MS }
        if (removed) publishLobbies()
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
            Log.i(TAG, "Local addresses: $addrs")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to enumerate local addresses: ${e.message}")
        }
    }
}
