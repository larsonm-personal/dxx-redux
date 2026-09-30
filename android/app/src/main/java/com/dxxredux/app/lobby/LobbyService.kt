package com.dxxredux.app.lobby

import android.content.Context
import android.net.wifi.WifiManager
import android.util.Log
import com.dxxredux.app.AssetManifest
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.VisualReplacementPolicy
import com.dxxredux.app.lanGameReadinessWarning
import com.dxxredux.app.multiplayer.ClientIdentity
import com.dxxredux.app.multiplayer.MissionCompatibilityResolver
import com.dxxredux.app.multiplayer.MissionCompatibilityStatus
import com.dxxredux.app.multiplayer.MissionRequirement
import com.dxxredux.app.multiplayer.MissionStatusReport
import com.dxxredux.app.multiplayer.MissionTransferGrant
import com.dxxredux.app.multiplayer.MissionTransferService
import com.dxxredux.app.multiplayer.MultiplayerForegroundService
import com.dxxredux.app.multiplayer.MultiplayerResumePrefs
import com.dxxredux.app.multiplayer.NetLog
import com.dxxredux.app.multiplayer.NetworkConstants
import com.dxxredux.app.multiplayer.RecentAddressPrefs
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
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
    private const val RECV_BUF_SIZE = 8 * 1024
    private const val SOCKET_TIMEOUT_MS = 500

    @Volatile private var failNextSocketOpenForTest = false
    private const val DISCOVERY_QUERY_INTERVAL_MS = 6000L
    private var lastDiscoveryQueryMs = 0L
    private var loggedDiscoveryTargets: List<String>? = null
    private val socketGeneration = AtomicLong(0)
    private const val DISCOVERY_PEER_EXPIRY_MS = 30_000L
    private const val MAX_DISCOVERY_PEERS = 64
    private val discoveryPeers = ConcurrentHashMap<String, Long>()
    private const val LOBBY_EXPIRY_MS = 10_000L
    private const val JOIN_RETRY_COUNT = 3
    private const val JOIN_RETRY_DELAY_MS = 1000L
    private const val JOINED_LOBBY_REFRESH_MS = 3000L
    private const val TRANSPORT_WATCHDOG_MS = 2000L
    private const val TRANSPORT_HEALTH_LOG_MS = 15_000L
    private const val BROADCAST_FAILURE_WARNING_THRESHOLD = 3
    private val MISSION_TRANSFER_REFRESH_STATES =
        setOf(
            MissionCompatibilityStatus.QUEUED,
            MissionCompatibilityStatus.DOWNLOADING,
            MissionCompatibilityStatus.PAUSED,
            MissionCompatibilityStatus.RETRYING,
            MissionCompatibilityStatus.FAILED_RESUMABLE,
            MissionCompatibilityStatus.VERIFYING,
            MissionCompatibilityStatus.FINALIZING,
        )
    private val MISSION_TERMINAL_STATES =
        setOf(
            MissionCompatibilityStatus.MATCH,
            MissionCompatibilityStatus.INSTALLED_DISABLED,
            MissionCompatibilityStatus.MISSING,
            MissionCompatibilityStatus.SIZE_MISMATCH,
            MissionCompatibilityStatus.HASH_MISMATCH,
            MissionCompatibilityStatus.UNSUPPORTED_SOURCE,
            MissionCompatibilityStatus.ERROR,
            MissionCompatibilityStatus.FAILED_RESUMABLE,
        )
    private val MISSION_TRANSFER_PHASES =
        mapOf(
            MissionCompatibilityStatus.QUEUED to 0,
            MissionCompatibilityStatus.RETRYING to 0,
            MissionCompatibilityStatus.DOWNLOADING to 1,
            MissionCompatibilityStatus.PAUSED to 1,
            MissionCompatibilityStatus.VERIFYING to 2,
            MissionCompatibilityStatus.FINALIZING to 3,
        )

    internal fun shouldResolveMissionAfterRefresh(report: MissionStatusReport?): Boolean =
        report?.status !in MISSION_TRANSFER_REFRESH_STATES

    internal fun shouldAcceptMissionStatus(
        previous: MissionStatusReport?,
        report: MissionStatusReport,
    ): Boolean {
        if (previous == null) return true
        if (report.attempt < previous.attempt) return false
        if (report.attempt > previous.attempt) return true
        if (report.status in MISSION_TERMINAL_STATES) return true
        if (previous.status in MISSION_TERMINAL_STATES) return false
        if (report.transferId != previous.transferId) return previous.transferId == null
        val previousPhase = MISSION_TRANSFER_PHASES[previous.status] ?: return true
        val reportPhase = MISSION_TRANSFER_PHASES[report.status] ?: return true
        return reportPhase > previousPhase ||
            (reportPhase == previousPhase && report.verifiedBytes >= previous.verifiedBytes)
    }

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
        val stockVisualsEnforced: Boolean = false,
        val omittedVisualModCount: Int = 0,
        val omittedVisualTextureCount: Int = 0,
        val omittedVisualModNames: List<String> = emptyList(),
        val missionRequirement: MissionRequirement? = null,
        val missionStatus: MissionStatusReport? = null,
        val saveCompatibilityWarning: String? = null,
    )

    private val _joinedLobby = MutableStateFlow<JoinedLobbyInfo?>(null)
    val joinedLobby: StateFlow<JoinedLobbyInfo?> = _joinedLobby.asStateFlow()

    // Diagnostic counters
    internal val packetsSent = AtomicLong(0)
    internal val packetsReceived = AtomicLong(0)
    private val packetsReceivedByAddress = ConcurrentHashMap<String, AtomicLong>()
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

    internal fun failNextTransportRecoveryForTest() {
        check(com.dxxredux.app.BuildConfig.DEBUG)
        failNextSocketOpenForTest = true
        socket?.close()
    }

    fun notifyAppBackgrounded() {
        manualIpAttempt.incrementAndGet()
        if (!_isDiscovering.value) return
        appBackgrounded = true
        socketRefreshNeededOnResume = true
        NetLog.log("LAN", "App backgrounded with LAN discovery active")
    }

    @Synchronized
    fun notifyAppResumed(
        context: Context,
        callsign: String,
    ) {
        val wasBackgrounded = appBackgrounded || socketRefreshNeededOnResume
        appBackgrounded = false
        socketRefreshNeededOnResume = false
        lastTransportHealthLogMs = 0L
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
        appContext = context.applicationContext
        startTransportWatchdog()
        recoverTransport("app resumed")
    }

    // -- Internal state --

    private var scope: CoroutineScope? = null

    // Socket replacement must not cancel the owner responsible for retrying it
    private val transportSupervisorScope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    @Volatile private var socket: DatagramSocket? = null
    private var multicastLock: WifiManager.MulticastLock? = null
    private var receiveJob: Job? = null
    private var announceJob: Job? = null
    private var pruneJob: Job? = null
    private var transportWatchdogJob: Job? = null
    private var joinRetryJob: Job? = null
    private var joinedLobbyRefreshJob: Job? = null
    private var missionTransferGrantTimeoutJob: Job? = null

    private var appContext: Context? = null

    @Volatile private var lastHostSeenMs: Long = 0L

    @Volatile private var lastHostPongMs: Long = 0L

    @Volatile private var hostReconnectStartedMs: Long = 0L

    @Volatile private var joinedLobbyReady = false

    @Volatile private var clientTransferAttempt = 0

    @Volatile private var pendingMissionTransferRequest: String? = null

    @Volatile private var acceptedMissionTransferRequest: String? = null

    @Volatile private var appBackgrounded = false

    @Volatile private var socketRefreshNeededOnResume = false

    @Volatile private var lastTransportHealthLogMs = 0L

    @Volatile private var lanForegroundSessionStarted = false

    // Keyed by lobbyId
    private val lobbies = ConcurrentHashMap<String, DiscoveredLobby>()

    // Host state -- accessed from UI + IO threads, needs @Volatile
    @Volatile private var hostedLobbyId: String? = null

    @Volatile private var hostedGame: String = "d2"

    @Volatile private var hostedMission: String = ""

    @Volatile private var hostedMissionRequirement: MissionRequirement? = null

    @Volatile private var hostedMode: String = "coop"

    @Volatile private var hostedMaxPlayers: Int = 4

    @Volatile private var hostCallsign: String = "Host"

    @Volatile private var localClientId: String? = null

    @Volatile private var gameStarted: Boolean = false

    private data class PendingHostStart(
        val lobbyId: String,
        val info: com.dxxredux.app.multiplayer.GameLaunchInfo,
        val packet: ByteArray,
        val players: List<LanPlayer>,
    )

    @Volatile private var pendingHostStart: PendingHostStart? = null
    private var hostStartJob: Job? = null
    private val manualIpAttempt =
        java.util.concurrent.atomic
            .AtomicLong()
    private var hostLaunchAttempt = 0L

    @Volatile private var hostLaunchPacket: ByteArray? = null
    private val clientLaunchPreparation = MutableStateFlow<LanLaunchPreparation?>(null)
    private val _hostedSaveWarning = MutableStateFlow<String?>(null)
    val hostedSaveWarning: StateFlow<String?> = _hostedSaveWarning.asStateFlow()
    private val _hostedSaveChecking = MutableStateFlow(false)
    val hostedSaveChecking: StateFlow<Boolean> = _hostedSaveChecking.asStateFlow()
    private var hostedSaveCheckJob: Job? = null
    private var hostedSaveCheckGeneration = 0L
    private var hostedSaveRevision: String? = null
    internal var hostedSaveCheckForTest: ((String, String) -> String?)? = null
        set(value) {
            check(com.dxxredux.app.BuildConfig.DEBUG)
            field = value
        }

    @Volatile private var inGameDifficulty: Int = -1

    @Volatile private var inGameLevelNum: Int = -1

    // Host proxy port override (non-zero after host migration with proxy)
    @Volatile private var hostedHostPort: Int = NetworkConstants.ENGINE_PORT

    @Volatile private var hostedRestrictNonCoopFovToBase: Boolean = false

    @Volatile private var hostedStockVisualsEnforced: Boolean = false

    @Volatile private var hostedOmittedVisualModCount: Int = 0

    @Volatile private var hostedOmittedVisualTextureCount: Int = 0

    @Volatile private var hostedOmittedVisualModNames: List<String> = emptyList()

    /**
     * Start discovery mode. Acquires multicast lock, opens the UDP socket,
     * and begins listening for ANNOUNCE packets.
     */
    @Synchronized
    fun startDiscovery(
        context: Context,
        callsign: String,
    ) {
        hostCallsign = callsign
        appContext = context.applicationContext
        localClientId = ClientIdentity.getInstallationId(context)
        appBackgrounded = false
        _isDiscovering.value = true
        startTransportWatchdog()
        if (isSocketUnavailable() || receiveJob?.isActive != true) {
            recoverTransport("discovery requested")
        }
        NetLog.log("LAN", "Discovery started on port ${NetworkConstants.LAN_LOBBY_PORT}, callsign=$callsign")
        Log.i(TAG, "LAN discovery started on port ${NetworkConstants.LAN_LOBBY_PORT}")
    }

    /** Stop discovery (and hosting if active). Releases all resources. */
    @Synchronized
    fun stopDiscovery() {
        manualIpAttempt.incrementAndGet()
        cancelHostedSaveCheck()
        clientLaunchPreparation.value = null
        hostLaunchPacket = null
        NetLog.log(
            "LAN",
            "Discovery stopping (wasHosting=${_isHosting.value}, wasJoined=${_joinedLobby.value != null})",
        )
        _isDiscovering.value = false
        transportWatchdogJob?.cancel()
        transportWatchdogJob = null
        _isHosting.value = false
        pendingHostStart = null
        hostStartJob?.cancel()
        hostStartJob = null
        hostedLobbyId = null
        lobbies.clear()
        discoveryPeers.clear()
        _discoveredLobbies.value = emptyList()
        _hostedLobbyPlayers.value = emptyList()
        _lanLaunchEvent.value = null
        _joinedLobby.value = null
        joinRetryJob?.cancel()
        joinRetryJob = null
        joinedLobbyRefreshJob?.cancel()
        joinedLobbyRefreshJob = null
        missionTransferGrantTimeoutJob?.cancel()
        missionTransferGrantTimeoutJob = null
        packetsSent.set(0)
        packetsReceived.set(0)
        packetsReceivedByAddress.clear()
        _diagnostics.value = ""
        _broadcastFailing.value = false
        consecutiveBroadcastFailures = 0
        appBackgrounded = false
        socketRefreshNeededOnResume = false
        hostedHostPort = NetworkConstants.ENGINE_PORT
        hostedRestrictNonCoopFovToBase = false
        hostedStockVisualsEnforced = false
        hostedOmittedVisualModCount = 0
        hostedOmittedVisualTextureCount = 0
        hostedOmittedVisualModNames = emptyList()
        localClientId = null
        hostedMissionRequirement = null
        MissionTransferService.stopHost()
        cancelMissionDownload()
        updateLanForegroundSession()
        appContext = null
        joinedLobbyReady = false
        lastHostPongMs = 0L
        hostReconnectStartedMs = 0L
        closeSocket()
        NetLog.log("LAN", "Discovery stopped")
        Log.i(TAG, "LAN discovery stopped")
    }

    private fun defaultMissionRequirement(
        game: String,
        mission: String,
    ) = MissionRequirement(
        revision = "builtin:$game:$mission",
        game = game,
        missionKey = mission,
        displayName = mission.ifBlank { if (game == "d1") "Descent: First Strike" else "Base mission" },
        kind =
            if (mission in
                setOf("", "descent", "d2", "d2demo")
            ) {
                MissionRequirement.KIND_BUILTIN
            } else {
                MissionRequirement.KIND_LOOSE
            },
    )

    /**
     * Start hosting a LAN lobby. Begins broadcasting ANNOUNCE every 3 seconds.
     * Discovery must already be started.
     */
    @Synchronized
    fun hostLobby(
        callsign: String,
        game: String,
        mission: String,
        mode: String,
        maxPlayers: Int,
        missionRequirement: MissionRequirement = defaultMissionRequirement(game, mission),
        restrictNonCoopFovToBase: Boolean = false,
        stockVisualsEnforced: Boolean = false,
        omittedVisualModCount: Int = 0,
        omittedVisualTextureCount: Int = 0,
        omittedVisualModNames: List<String> = emptyList(),
    ) {
        if (!_isDiscovering.value) return
        if (
            !missionRequirement.isValid ||
            missionRequirement.game != game ||
            missionRequirement.missionKey != mission
        ) {
            _diagnostics.value = "Cannot host: invalid mission requirement"
            return
        }
        leaveLanLobby(hostCallsign)
        hostedLobbyId = UUID.randomUUID().toString()
        hostLaunchAttempt = 0L
        hostLaunchPacket = null
        hostCallsign = callsign
        hostedGame = game
        hostedMission = mission
        hostedMode = mode
        hostedMaxPlayers = maxPlayers
        val transferStarted = appContext?.let { MissionTransferService.startHost(it, missionRequirement) } == true
        val effectiveMissionRequirement =
            if (missionRequirement.offerAvailable && !transferStarted) {
                missionRequirement.copy(offerAvailable = false)
            } else {
                missionRequirement
            }
        hostedMissionRequirement = effectiveMissionRequirement
        hostedRestrictNonCoopFovToBase = restrictNonCoopFovToBase
        hostedStockVisualsEnforced = stockVisualsEnforced
        hostedOmittedVisualModCount = omittedVisualModCount
        hostedOmittedVisualTextureCount = omittedVisualTextureCount
        hostedOmittedVisualModNames = omittedVisualModNames
        _hostedLobbyPlayers.value =
            listOf(
                LanPlayer(
                    callsign = callsign,
                    address = "127.0.0.1",
                    clientId = localClientId,
                    ready = true,
                    missionStatus =
                        MissionStatusReport(
                            revision = effectiveMissionRequirement.revision,
                            status = MissionCompatibilityStatus.MATCH,
                            totalBytes = effectiveMissionRequirement.sizeBytes ?: 0L,
                        ),
                ),
            )
        _isHosting.value = true
        pendingHostStart = null
        hostStartJob?.cancel()
        hostStartJob = null
        gameStarted = false
        hostedHostPort = NetworkConstants.ENGINE_PORT
        refreshHostedSaveWarning()
        updateLanForegroundSession()

        restartAnnounceLoop()
        NetLog.log("LAN", "Hosting lobby $hostedLobbyId ($game, $mission, $mode, max=$maxPlayers)")
        Log.i(TAG, "Hosting LAN lobby $hostedLobbyId ($game, $mission, $mode)")
    }

    /** Advertise an existing engine session without entering the new-game launch flow */
    @Synchronized
    fun adoptMigratedHost(
        callsign: String,
        game: com.dxxredux.app.multiplayer.GameLaunchInfo,
        hostPort: Int,
    ) {
        hostLobby(
            callsign,
            game.game,
            game.mission,
            game.mode,
            game.maxPlayers,
            missionRequirement = game.missionRequirement ?: defaultMissionRequirement(game.game, game.mission),
            restrictNonCoopFovToBase = game.restrictNonCoopFovToBase,
        )
        if (!_isHosting.value) return
        _lanLaunchEvent.value = null
        hostedHostPort = hostPort
        inGameDifficulty = game.difficulty
        inGameLevelNum = game.levelNum
        gameStarted = true
        _diagnostics.value = ""
        NetLog.log("LAN", "Adopted migrated host: ${game.game}/${game.mission} level=${game.levelNum} port=$hostPort")
        // Announcement and query handlers share this lock, so no waiting-lobby packet escapes
        restartAnnounceLoop()
    }

    /** Stop hosting (but keep discovery running). */
    @Synchronized
    fun stopHosting() {
        cancelHostedSaveCheck()
        NetLog.log("LAN", "Stopped hosting lobby $hostedLobbyId")
        _isHosting.value = false
        discoveryPeers.clear()
        _lanLaunchEvent.value = null
        stopInGameBroadcast()
        announceJob?.cancel()
        announceJob = null
        hostedLobbyId = null
        hostedMissionRequirement = null
        MissionTransferService.stopHost()
        hostedRestrictNonCoopFovToBase = false
        hostedStockVisualsEnforced = false
        hostedOmittedVisualModCount = 0
        hostedOmittedVisualTextureCount = 0
        hostedOmittedVisualModNames = emptyList()
        _hostedLobbyPlayers.value = emptyList()
        _chatMessages.value = emptyList()
        updateLanForegroundSession()
        Log.i(TAG, "Stopped hosting LAN lobby")
    }

    /** Send a JOIN packet to the given host address, with retry. */
    fun joinLobby(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
    ) {
        manualIpAttempt.incrementAndGet()
        val advertisedGame =
            _discoveredLobbies.value
                .find {
                    it.announce.lobbyId == lobbyId && it.announce.hostAddress == hostAddress
                }?.announce
                ?.game
        if (advertisedGame != null && !checkJoinGameReady(advertisedGame)) return
        if (_isHosting.value) stopHosting()
        val previous = _joinedLobby.value
        if (previous != null && (previous.lobbyId != lobbyId || previous.hostAddr != hostAddress)) {
            leaveLanLobby(hostCallsign)
        }
        hostCallsign = callsign
        Log.i(TAG, "joinLobby: lobbyId=$lobbyId host=$hostAddress callsign=$callsign")
        NetLog.log("LAN", "Joining lobby $lobbyId at $hostAddress as $callsign")
        Log.i(TAG, "joinLobby: socket=${socket != null} bound=${socket?.isBound} closed=${socket?.isClosed}")
        setLanForegroundSessionActive(true)
        joinRetryJob?.cancel()
        joinedLobbyReady = false
        lastHostPongMs = 0L
        hostReconnectStartedMs = 0L
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
                    updateLanForegroundSession()
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
                        joinDiscoveredLobby(lobby.announce, callsign)
                        return@launch
                    }
                }
                _diagnostics.value = "No matching lobby found at $hostAddress"
                Log.w(TAG, "joinLobbyByIp: no matching ANNOUNCE from $hostAddress after $JOIN_RETRY_COUNT attempts")
            }
    }

    /**
     * Prefer a fresh launcher lobby; optionally verify the engine in parallel for manual joins
     * Allow delayed Wi-Fi replies for manual joins; resume callers supply their shorter deadline
     * Runs on the caller's coroutine context (should be called from Dispatchers.IO).
     */
    suspend fun tryJoinLobbyByIp(
        hostAddress: String,
        callsign: String,
        timeoutMs: Long = 30_000L,
        probeEngine: Boolean = false,
        acceptLobby: (LanLobbyAnnounce) -> Boolean = { true },
    ): Boolean =
        coroutineScope {
            val attempt = manualIpAttempt.incrementAndGet()
            val started = System.currentTimeMillis()
            val startedElapsed = android.os.SystemClock.elapsedRealtime()
            val joinedBefore = _joinedLobby.value
            val hostingBefore = hostedLobbyId

            fun stillCurrent() =
                _isDiscovering.value && !appBackgrounded && manualIpAttempt.get() == attempt &&
                    _joinedLobby.value === joinedBefore && hostedLobbyId == hostingBefore
            _diagnostics.value = ""
            val engine =
                if (probeEngine) {
                    async(
                        Dispatchers.IO,
                    ) { EngineQuery.probe(hostAddress, timeoutMs = timeoutMs) }
                } else {
                    null
                }
            NetLog.log(
                "LAN",
                "Manual IP probe begin host=$hostAddress timeout_ms=$timeoutMs generation=${socketGeneration.get()}",
            )
            Log.i(TAG, "tryJoinLobbyByIp: probing $hostAddress (timeout=${timeoutMs}ms)")
            val query = buildQuery()
            val deadline = startedElapsed + timeoutMs
            var nextQuery = 0L
            while (android.os.SystemClock.elapsedRealtime() < deadline && stillCurrent()) {
                val now = android.os.SystemClock.elapsedRealtime()
                if (now >= nextQuery) {
                    sendTo(query, hostAddress)
                    nextQuery = now + 250L
                }
                val lobby =
                    _discoveredLobbies.value.find {
                        it.announce.hostAddress == hostAddress &&
                            it.lastSeenMs >= started &&
                            acceptLobby(it.announce)
                    }
                if (lobby != null && (!probeEngine || lobby.announce.status != "in_game")) {
                    NetLog.log("LAN", "Manual IP probe found host=$hostAddress status=${lobby.announce.status}")
                    Log.i(TAG, "tryJoinLobbyByIp: found lobby ${lobby.announce.lobbyId}, joining")
                    joinDiscoveredLobby(lobby.announce, callsign)
                    engine?.cancel()
                    return@coroutineScope true
                }
                // Give a launcher lobby first refusal, but do not wait the whole loss-retry
                // window once an engine has actually answered
                if (engine?.isCompleted == true && now - startedElapsed >= 1000L) {
                    val result = engine.await()
                    if (result.game != null || result.error != null) break
                }
                delay(50)
            }
            currentCoroutineContext().ensureActive()
            if (!stillCurrent()) {
                engine?.cancel()
                return@coroutineScope false
            }
            if (engine != null) {
                val advertised =
                    _discoveredLobbies.value
                        .firstOrNull {
                            it.announce.hostAddress == hostAddress && it.lastSeenMs >= started &&
                                acceptLobby(it.announce)
                        }?.announce
                // A lobby reply arriving at the deadline still wins over an engine reply
                if (advertised != null && advertised.status != "in_game") {
                    engine.cancel()
                    joinDiscoveredLobby(advertised, callsign)
                    return@coroutineScope true
                }
                val result =
                    if (advertised != null && advertised.hostPort != NetworkConstants.ENGINE_PORT) {
                        engine.cancel()
                        EngineQuery.probe(hostAddress, advertised.hostPort, timeoutMs, advertised.game)
                    } else {
                        engine.await()
                    }
                currentCoroutineContext().ensureActive()
                if (!stillCurrent()) return@coroutineScope false
                val verified = result.game
                if (verified != null && acceptLobby(verified)) {
                    val target =
                        advertised?.takeIf {
                            it.game == verified.game && it.mission == verified.mission &&
                                it.hostPort == verified.hostPort
                        } ?: verified
                    if (!checkJoinGameReady(target.game)) return@coroutineScope false
                    joinDiscoveredLobby(target, callsign)
                    return@coroutineScope true
                }
                if (result.error != null) {
                    _diagnostics.value = result.error
                    return@coroutineScope false
                }
            }
            _diagnostics.value =
                "No response from $hostAddress. Check the address and that both devices are on the same network, then try again."
            Log.i(TAG, "tryJoinLobbyByIp: no lobby found at $hostAddress within ${timeoutMs}ms")
            NetLog.log(
                "LAN",
                "Manual IP probe timeout host=$hostAddress elapsed_ms=${android.os.SystemClock.elapsedRealtime() - startedElapsed}",
            )
            false
        }

    /** Leave the LAN lobby we've joined (as a joiner). */
    fun leaveLanLobby(callsign: String) {
        joinRetryJob?.cancel()
        joinRetryJob = null
        val info = _joinedLobby.value ?: return
        NetLog.log("LAN", "Leaving lobby ${info.lobbyId} (host=${info.hostAddr})")
        val data = buildLeave(info.lobbyId, callsign, localClientId)
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
        lastHostPongMs = 0L
        hostReconnectStartedMs = 0L
        joinedLobbyReady = false
        joinedLobbyRefreshJob?.cancel()
        joinedLobbyRefreshJob = null
        missionTransferGrantTimeoutJob?.cancel()
        missionTransferGrantTimeoutJob = null
        cancelMissionDownload()
        updateLanForegroundSession()
        Log.i(TAG, "Left LAN lobby ${info.lobbyId}")
    }

    /** Send a LEAVE packet to the given host address. */
    fun leaveLobby(
        lobbyId: String,
        hostAddress: String,
        callsign: String,
    ) {
        val data = buildLeave(lobbyId, callsign, localClientId)
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
        joinedLobbyReady = ready
        val data = buildReady(lobbyId, callsign, ready, localClientId)
        scope?.launch(Dispatchers.IO) {
            val status = _joinedLobby.value?.missionStatus
            if (ready && status?.status == MissionCompatibilityStatus.MATCH) {
                sendTo(buildMissionStatus(lobbyId, callsign, localClientId, status), hostAddress)
                delay(50L)
            }
            sendTo(data, hostAddress)
        }
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
            scope?.launch(Dispatchers.IO) { broadcastToJoiners(data) }
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

    private fun notifyPlayerConnectionChanged(
        callsign: String,
        connected: Boolean,
    ) {
        val text = if (connected) "$callsign reconnected" else "$callsign lost connection, waiting for reconnect"
        notifyLobbySystemMessage(text)
        broadcastPlayerList()
    }

    private fun notifyLobbySystemMessage(text: String) {
        appendChatMessage(
            com.dxxredux.app.multiplayer
                .ChatMessage("System", text),
        )
        hostedLobbyId?.let { lobbyId ->
            val chat = buildChat(lobbyId, "System", text)
            scope?.launch(Dispatchers.IO) { broadcastToJoiners(chat) }
        }
        NetLog.log("LAN", text)
    }

    private fun noteDirectedHostContact(source: String) {
        val now = System.currentTimeMillis()
        val wasReconnecting = hostReconnectStartedMs > 0L
        lastHostSeenMs = now
        lastHostPongMs = now
        hostReconnectStartedMs = 0L
        if (_diagnostics.value == LAN_RECONNECTING_DIAGNOSTIC) _diagnostics.value = ""
        if (wasReconnecting) NetLog.log("LAN", "Host connection restored by $source")
    }

    /** Send data to all joiners (host only, skips self). */
    private fun broadcastToJoiners(data: ByteArray) {
        for (p in _hostedLobbyPlayers.value) {
            if (p.address != "127.0.0.1" && p.connected) {
                sendTo(data, p.address)
            }
        }
    }

    // ------------------------------------------------------------------
    // Internal
    // ------------------------------------------------------------------

    @Synchronized
    private fun openSocket(context: Context) {
        closeSocket()
        scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

        // Acquire multicast lock so Android doesn't filter broadcast/multicast
        val wifiManager =
            context.applicationContext
                .getSystemService(Context.WIFI_SERVICE) as? WifiManager
        multicastLock =
            wifiManager?.createMulticastLock("dxx-lan-discovery")?.apply {
                setReferenceCounted(false)
                acquire()
            }

        val openedSocket = DatagramSocket(null)
        val generation = socketGeneration.incrementAndGet()
        socket = openedSocket
        try {
            openedSocket.apply {
                if (failNextSocketOpenForTest) {
                    failNextSocketOpenForTest = false
                    throw java.net.BindException("Injected LAN socket bind failure")
                }
                reuseAddress = true
                broadcast = true
                bind(InetSocketAddress(NetworkConstants.LAN_LOBBY_PORT))
                soTimeout = SOCKET_TIMEOUT_MS
            }
        } catch (e: Exception) {
            closeSocket()
            throw e
        }
        NetLog.log(
            "LAN",
            "Socket opened: generation=$generation local=${openedSocket.localSocketAddress} port=${socket?.localPort} bound=${socket?.isBound} broadcast=${socket?.broadcast}",
        )
        Log.i(TAG, "Socket opened: port=${socket?.localPort} bound=${socket?.isBound} broadcast=${socket?.broadcast}")
        NetLog.log("LAN", "Multicast lock: ${multicastLock?.isHeld}")
        Log.i(TAG, "Multicast lock acquired: ${multicastLock?.isHeld}")
        logLocalAddresses()
        lastDiscoveryQueryMs = 0L
        loggedDiscoveryTargets = null

        // Receive loop
        receiveJob =
            scope?.launch(Dispatchers.IO) {
                val buf = ByteArray(RECV_BUF_SIZE)
                Log.i(TAG, "Receive loop started")
                while (isActive) {
                    try {
                        val packet = DatagramPacket(buf, buf.size)
                        openedSocket.receive(packet)
                        if (!isActive || socket !== openedSocket) break
                        val senderAddr = packet.address.hostAddress ?: continue
                        val json = parsePacket(packet.data, packet.length)
                        if (json == null) {
                            NetLog.log(
                                "LAN",
                                "UDP RX invalid generation=$generation from=$senderAddr:${packet.port} bytes=${packet.length}",
                            )
                            Log.w(TAG, "recv: unparseable ${packet.length} bytes from $senderAddr")
                            continue
                        }
                        val rxCount = packetsReceived.incrementAndGet()
                        packetsReceivedByAddress.getOrPut(senderAddr) { AtomicLong(0) }.incrementAndGet()
                        val msgType = json.optString("type", "?")
                        if (msgType == MSG_QUERY || msgType == MSG_ANNOUNCE) {
                            NetLog.log(
                                "LAN",
                                "UDP RX generation=$generation socket_id=${System.identityHashCode(openedSocket)} " +
                                    "recv_ms=${android.os.SystemClock.elapsedRealtime()} " +
                                    "from=$senderAddr:${packet.port} " +
                                    "bytes=${packet.length} ${discoveryTraceSummary(json)}",
                            )
                        }
                        Log.d(TAG, "recv: $msgType from $senderAddr (${packet.length}B, total=$rxCount)")
                        val handlingStarted = android.os.SystemClock.elapsedRealtime()
                        handlePacket(json, senderAddr)
                        val handlingMs = android.os.SystemClock.elapsedRealtime() - handlingStarted
                        if (msgType == MSG_QUERY || handlingMs >= 100) {
                            NetLog.log(
                                "LAN",
                                "UDP handled generation=$generation type=$msgType id=${json.optString(
                                    "trace_id",
                                )} elapsed_ms=$handlingMs",
                            )
                        }
                    } catch (_: SocketTimeoutException) {
                        // Normal, just loop back to check isActive
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        if (isActive) {
                            NetLog.log("LAN", "Receive error: ${e.message}")
                            Log.w(TAG, "receive error: ${e.message}")
                        }
                        if (openedSocket.isClosed) break
                    }
                }
                Log.i(TAG, "Receive loop ended")
            }
        trackTransportJob("receive", receiveJob) { _isDiscovering.value }

        startPruneLoop()
    }

    private fun startPruneLoop() {
        pruneJob?.cancel()
        pruneJob =
            scope?.launch(Dispatchers.IO) {
                while (isActive) {
                    delay(2000)
                    queryDiscoveryHosts()
                    refreshChangedHostedSave()
                    pruneStaleLobbies()
                }
            }
        trackTransportJob("prune", pruneJob) { _isDiscovering.value }
    }

    private fun queryDiscoveryHosts() {
        if (!_isDiscovering.value || _isHosting.value || _joinedLobby.value != null || appBackgrounded) return
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastDiscoveryQueryMs < DISCOVERY_QUERY_INTERVAL_MS) return
        lastDiscoveryQueryMs = now
        val context = appContext ?: return
        val lastHost =
            MultiplayerResumePrefs
                .load(context)
                ?.takeIf {
                    it.transport == "lan" && it.role == "client"
                }?.lanHostAddr
        val targets = (RecentAddressPrefs.LAN_IPS.load(context) + listOfNotNull(lastHost)).distinct()
        if (loggedDiscoveryTargets != targets) {
            loggedDiscoveryTargets = targets
            NetLog.log("LAN", "Active discovery: broadcast QUERY plus remembered hosts=$targets")
        }
        val query = buildQuery()
        sendBroadcast(query)
        targets.forEach { sendTo(query, it) }
    }

    private fun traceDiscoverySend(
        data: ByteArray,
        destination: String,
        strategy: String,
        activeSocket: DatagramSocket,
    ): ByteArray {
        val json = parsePacket(data, data.size) ?: return data
        if (json.optString("type") !in listOf(MSG_QUERY, MSG_ANNOUNCE)) return data
        json.put("trace_id", UUID.randomUUID().toString())
        json.put("trace_ms", android.os.SystemClock.elapsedRealtime())
        json.put("trace_strategy", strategy)
        NetLog.log(
            "LAN",
            "UDP TX attempt generation=${socketGeneration.get()} socket_id=${System.identityHashCode(activeSocket)} " +
                "local=${activeSocket.localSocketAddress} to=$destination:${NetworkConstants.LAN_LOBBY_PORT} " +
                discoveryTraceSummary(json),
        )
        return json.toString().toByteArray(Charsets.UTF_8)
    }

    private fun logDiscoverySent(data: ByteArray) {
        val json = parsePacket(data, data.size) ?: return
        if (json.optString("type") in listOf(MSG_QUERY, MSG_ANNOUNCE)) {
            NetLog.log("LAN", "UDP TX accepted id=${json.optString("trace_id")} bytes=${data.size}")
        }
    }

    @Synchronized
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
        joinRetryJob = null
        joinedLobbyRefreshJob = null
        try {
            multicastLock?.release()
        } catch (_: Exception) {
            // ignore
        }
        multicastLock = null
    }

    @Synchronized
    private fun startTransportWatchdog() {
        if (transportWatchdogJob?.isActive == true) return
        transportWatchdogJob =
            transportSupervisorScope.launch {
                while (isActive) {
                    delay(TRANSPORT_WATCHDOG_MS)
                    val recoveryReason =
                        lanTransportRecoveryReason(
                            isDiscovering = _isDiscovering.value,
                            appBackgrounded = appBackgrounded,
                            socketAvailable = !isSocketUnavailable(),
                            receiveLoopActive = receiveJob?.isActive == true,
                        )
                    if (recoveryReason != null) {
                        recoverTransport(recoveryReason)
                        continue
                    }
                    if (_isDiscovering.value && pruneJob?.isActive != true) {
                        NetLog.log("LAN", "Restarting stopped prune loop")
                        startPruneLoop()
                    }
                    if (_isHosting.value && announceJob?.isActive != true) {
                        NetLog.log("LAN", "Restarting stopped announce loop")
                        restartAnnounceLoop()
                    }
                    if (_joinedLobby.value != null && joinedLobbyRefreshJob?.isActive != true) {
                        NetLog.log("LAN", "Restarting stopped joined-lobby heartbeat")
                        startJoinedLobbyRefresh(immediate = true)
                    }
                    val now = System.currentTimeMillis()
                    if (now - lastTransportHealthLogMs >= TRANSPORT_HEALTH_LOG_MS) {
                        lastTransportHealthLogMs = now
                        logTransportHealth(now)
                    }
                }
            }
    }

    @Synchronized
    private fun recoverTransport(reason: String) {
        if (!_isDiscovering.value || appBackgrounded) return
        val context = appContext ?: return
        NetLog.log("LAN", "Recovering lobby transport: $reason")
        Log.w(TAG, "Recovering lobby transport: $reason")
        try {
            openSocket(context)
            val now = System.currentTimeMillis()
            if (_isHosting.value) {
                _hostedLobbyPlayers.value = refreshLanPlayerLeasesAfterResume(_hostedLobbyPlayers.value, now)
                restartAnnounceLoop()
            }
            if (_joinedLobby.value != null) {
                lastHostSeenMs = now
                lastHostPongMs = now
                hostReconnectStartedMs = 0L
                startJoinedLobbyRefresh(immediate = true)
            }
            socketRefreshNeededOnResume = false
            if (_diagnostics.value.startsWith("LAN transport unavailable:")) _diagnostics.value = ""
            NetLog.log("LAN", "Lobby transport recovery complete")
        } catch (e: Exception) {
            closeSocket()
            socketRefreshNeededOnResume = true
            _diagnostics.value = "LAN transport unavailable: ${e.message ?: e.javaClass.simpleName}. Retrying..."
            NetLog.log("LAN", "Lobby transport recovery failed: ${e.message}")
            Log.w(TAG, "Lobby transport recovery failed", e)
        }
    }

    private fun trackTransportJob(
        name: String,
        job: Job?,
        expectedActive: () -> Boolean,
    ) {
        job?.invokeOnCompletion { cause ->
            if (expectedActive() && !appBackgrounded && cause !is CancellationException) {
                val detail = cause?.let { "${it.javaClass.simpleName}: ${it.message}" } ?: "completed"
                NetLog.log("LAN", "Transport job '$name' ended unexpectedly: $detail")
                Log.w(TAG, "Transport job '$name' ended unexpectedly: $detail")
            }
        }
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
                handleLeave(json, senderAddr)
            }

            MSG_READY -> {
                handleReady(json, senderAddr)
            }

            MSG_MISSION_STATUS -> {
                handleMissionStatus(json, senderAddr)
            }

            MSG_MISSION_TRANSFER_REQUEST -> {
                handleMissionTransferRequest(json, senderAddr)
            }

            MSG_MISSION_TRANSFER_GRANT -> {
                handleMissionTransferGrant(json, senderAddr)
            }

            MSG_PLAYER_LIST -> {
                handlePlayerList(json)
            }

            MSG_START, MSG_PREPARE, MSG_START_CANCEL -> {
                handleStart(json, senderAddr)
            }

            MSG_PING -> {
                handlePing(json, senderAddr)
            }

            MSG_PONG -> {
                handlePong(json, senderAddr)
            }

            MSG_JOIN_ACK -> {
                handleJoinAck(json, senderAddr)
            }

            MSG_JOIN_REJECT -> {
                handleJoinReject(json)
            }

            MSG_QUERY -> {
                handleQuery(json, senderAddr)
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
                )} ${json.optString("game", "?")}/${json.optString("mission", "?")}) " +
                    "source=${if (json.optBoolean("query_reply")) "query-reply" else "broadcast"} " +
                    "status=${json.optString(
                        "status",
                        "lobby",
                    )} port=${json.optInt("host_port", NetworkConstants.ENGINE_PORT)}",
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
                stockVisualsEnforced = json.optBoolean(VisualReplacementPolicy.STOCK_VISUALS_ENFORCED, false),
                omittedVisualModCount = json.optInt(VisualReplacementPolicy.OMITTED_VISUAL_MOD_COUNT, 0),
                omittedVisualTextureCount = json.optInt(VisualReplacementPolicy.OMITTED_VISUAL_TEXTURE_COUNT, 0),
                omittedVisualModNames = VisualReplacementPolicy.namesFromJson(json),
                saveCompatibilityWarning = json.optString("save_compatibility_warning").takeIf { it.isNotBlank() },
                missionRequirement = missionRequirementFromJson(json.optJSONObject("mission_requirement")),
            )
        lobbies[lobbyId] = DiscoveredLobby(announce = announce)
        publishLobbies()
        // Track host liveness: ANNOUNCE from the host of our joined lobby
        if (_joinedLobby.value?.lobbyId == lobbyId) {
            lastHostSeenMs = System.currentTimeMillis()
            _joinedLobby.value = _joinedLobby.value?.copy(saveCompatibilityWarning = announce.saveCompatibilityWarning)
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
        val lobbyId = json.optString("lobby_id", "")
        val callsign = json.optString("callsign", "Player")
        val clientId = json.optString("client_id", "").takeIf { it.isNotBlank() }
        val current = _hostedLobbyPlayers.value
        if (lanLobbyHasClientIdConflict(current, callsign, clientId)) {
            NetLog.log("LAN", "JOIN rejected: duplicate client ID for '$callsign' from $senderAddr")
            Log.w(TAG, "handleJoin: duplicate client ID for '$callsign' from $senderAddr")
            sendTo(buildJoinReject(lobbyId, "duplicate client identity"), senderAddr)
            return
        }
        if (gameStarted) {
            // Allow mid-game joins: send ACK with in-game info so the
            // joiner's C engine can negotiate via UPID_REQUEST/NETSTAT_PLAYING
            Log.i(TAG, "handleJoin: game in progress, allowing mid-game join from $senderAddr")
            sendTo(
                buildJoinAck(
                    hostedLobbyId ?: "",
                    hostedGame,
                    hostedMission,
                    hostedMode,
                    hostedMaxPlayers,
                    hostCallsign,
                    localClientId,
                    hostedStockVisualsEnforced,
                    hostedOmittedVisualModCount,
                    hostedOmittedVisualTextureCount,
                    hostedOmittedVisualModNames,
                    missionRequirement = hostedMissionRequirement,
                ),
                senderAddr,
            )
            NetLog.log("LAN", "Mid-game JOIN_ACK sent to $callsign at $senderAddr")
            if (lobbyId == hostedLobbyId &&
                current.any { lanPlayerMatchesJoinIdentity(it, callsign, clientId, senderAddr) }
            ) {
                hostLaunchPacket?.let { sendTo(it, senderAddr) }
            }
            return
        }
        if (lobbyId != hostedLobbyId) {
            Log.w(TAG, "handleJoin: lobby mismatch expected=$hostedLobbyId got=$lobbyId from $senderAddr")
            sendTo(buildJoinReject(lobbyId, "unknown lobby"), senderAddr)
            return
        }

        val existing = current.find { lanPlayerMatchesJoinIdentity(it, callsign, clientId, senderAddr) }
        if (existing != null) {
            val reconnected = !existing.connected
            Log.i(TAG, "handleJoin: $callsign refreshing membership from $senderAddr")
            _hostedLobbyPlayers.value =
                current.map { player ->
                    if (player === existing) {
                        player.copy(
                            address = senderAddr,
                            clientId = player.clientId ?: clientId,
                            connected = true,
                            disconnectedAtMs = null,
                            lastSeenMs = System.currentTimeMillis(),
                        )
                    } else {
                        player
                    }
                }
            sendJoinAck(lobbyId, senderAddr)
            // Heartbeat JOINs recover a lost prepare/commit/cancel without restarting preparation
            hostLaunchPacket?.let { sendTo(it, senderAddr) }
            if (reconnected) {
                notifyPlayerConnectionChanged(callsign, connected = true)
            } else {
                sendPlayerList(senderAddr)
            }
            return
        }
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
        if (current.any { it.callsign.equals(callsign, ignoreCase = true) }) {
            NetLog.log("LAN", "JOIN rejected: duplicate callsign '$callsign' from $senderAddr")
            Log.w(TAG, "handleJoin: duplicate callsign '$callsign' from $senderAddr")
            sendTo(buildJoinReject(lobbyId, "duplicate callsign"), senderAddr)
            return
        }

        val updated =
            current +
                LanPlayer(
                    callsign = callsign,
                    address = senderAddr,
                    clientId = clientId,
                    missionStatus =
                        hostedMissionRequirement?.let {
                            MissionStatusReport(
                                revision = it.revision,
                                status = MissionCompatibilityStatus.CHECKING,
                                totalBytes = it.sizeBytes ?: 0L,
                            )
                        },
                )
        _hostedLobbyPlayers.value = updated
        sendJoinAck(lobbyId, senderAddr)
        broadcastPlayerList()
        NetLog.log("LAN", "Player joined: $callsign from $senderAddr (${updated.size} players)")
        Log.i(TAG, "Player joined: $callsign from $senderAddr")
    }

    private fun handleLeave(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value) return
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) return

        val callsign = json.optString("callsign", "")
        val clientId = json.optString("client_id", "").takeIf { it.isNotBlank() }
        val leavingPlayer =
            _hostedLobbyPlayers.value.find { lanPlayerMatchesSender(it, callsign, clientId, senderAddr) }
                ?: return
        val updated = _hostedLobbyPlayers.value.filter { it !== leavingPlayer }
        _hostedLobbyPlayers.value = updated
        broadcastPlayerList()
        NetLog.log("LAN", "Player left: $callsign")
        Log.i(TAG, "Player left: $callsign")
    }

    private fun handleReady(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value) return
        val lobbyId = json.optString("lobby_id", "")
        if (lobbyId != hostedLobbyId) return

        val callsign = json.optString("callsign", "")
        val clientId = json.optString("client_id", "").takeIf { it.isNotBlank() }
        val ready = json.optBoolean("ready", false)
        val matchingPlayer =
            _hostedLobbyPlayers.value.find { lanPlayerMatchesSender(it, callsign, clientId, senderAddr) }
        if (matchingPlayer == null) {
            NetLog.log("LAN", "Ignored READY from unknown player $callsign at $senderAddr")
            return
        }
        val reconnected = !matchingPlayer.connected
        val acceptedReady = ready && matchingPlayer.missionStatus?.status == MissionCompatibilityStatus.MATCH
        val updated =
            _hostedLobbyPlayers.value.map { p ->
                if (p === matchingPlayer) {
                    p.copy(
                        ready = acceptedReady,
                        connected = true,
                        disconnectedAtMs = null,
                        lastSeenMs = System.currentTimeMillis(),
                    )
                } else {
                    p
                }
            }
        _hostedLobbyPlayers.value = updated
        if (matchingPlayer.ready != acceptedReady) {
            NetLog.log("LAN", "READY: $callsign -> $acceptedReady")
        }
        if (reconnected) {
            notifyPlayerConnectionChanged(callsign, connected = true)
        } else {
            broadcastPlayerList()
        }
    }

    private fun handleMissionStatus(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value) return
        val requirement = hostedMissionRequirement ?: return
        if (json.optString("lobby_id") != hostedLobbyId) return
        val callsign = json.optString("callsign")
        val clientId = json.optString("client_id").takeIf(String::isNotBlank)
        val report = missionStatusFromJson(json.optJSONObject("mission_status")) ?: return
        if (!report.validFor(requirement)) return
        val player =
            _hostedLobbyPlayers.value.find { lanPlayerMatchesSender(it, callsign, clientId, senderAddr) }
                ?: return
        val previous = player.missionStatus
        if (!shouldAcceptMissionStatus(previous, report)) return
        val reconnected = !player.connected
        _hostedLobbyPlayers.value =
            _hostedLobbyPlayers.value.map {
                if (it === player) {
                    it.copy(
                        ready = it.ready && report.status == MissionCompatibilityStatus.MATCH,
                        connected = true,
                        disconnectedAtMs = null,
                        missionStatus = report,
                        lastSeenMs = System.currentTimeMillis(),
                    )
                } else {
                    it
                }
            }
        if (reconnected) {
            notifyPlayerConnectionChanged(callsign, connected = true)
        } else {
            broadcastPlayerList()
        }
    }

    @Synchronized
    fun requestMissionDownload() {
        val joined = _joinedLobby.value ?: return
        val requirement = joined.missionRequirement ?: return
        if (!requirement.isWrapper || !requirement.offerAvailable) return
        clientTransferAttempt = 1
        sendMissionTransferRequest(joined, requirement, clientTransferAttempt)
    }

    @Synchronized
    private fun cancelMissionDownload() {
        pendingMissionTransferRequest = null
        acceptedMissionTransferRequest = null
        missionTransferGrantTimeoutJob?.cancel()
        missionTransferGrantTimeoutJob = null
        MissionTransferService.cancelClient()
    }

    fun enableMatchingMission() {
        val joined = _joinedLobby.value ?: return
        val requirement = joined.missionRequirement ?: return
        val context = appContext ?: return
        scope?.launch(Dispatchers.IO) {
            val report = MissionCompatibilityResolver.enableMatchingWrapper(context, requirement, joined.mode)
            val current = _joinedLobby.value ?: return@launch
            if (current.missionRequirement?.revision != requirement.revision) return@launch
            _joinedLobby.value = current.copy(missionStatus = report)
            sendTo(buildMissionStatus(current.lobbyId, hostCallsign, localClientId, report), current.hostAddr)
        }
    }

    @Synchronized
    private fun sendMissionTransferRequest(
        joined: JoinedLobbyInfo,
        requirement: MissionRequirement,
        attempt: Int,
    ) {
        val requestId = UUID.randomUUID().toString()
        pendingMissionTransferRequest = requestId
        acceptedMissionTransferRequest = null
        MissionTransferService.cancelClient()
        val queued =
            MissionStatusReport(
                revision = requirement.revision,
                status = if (attempt > 1) MissionCompatibilityStatus.RETRYING else MissionCompatibilityStatus.QUEUED,
                totalBytes = requirement.sizeBytes ?: 0L,
                attempt = attempt,
            )
        _joinedLobby.value = joined.copy(missionStatus = queued)
        scope?.launch(Dispatchers.IO) {
            sendTo(buildMissionStatus(joined.lobbyId, hostCallsign, localClientId, queued), joined.hostAddr)
            sendTo(
                buildMissionTransferRequest(
                    joined.lobbyId,
                    hostCallsign,
                    localClientId,
                    requirement.revision,
                    attempt,
                    requestId,
                ),
                joined.hostAddr,
            )
        }
        missionTransferGrantTimeoutJob?.cancel()
        missionTransferGrantTimeoutJob =
            scope?.launch(Dispatchers.IO) {
                delay(5_000L)
                synchronized(this@LobbyService) {
                    if (pendingMissionTransferRequest != requestId) return@synchronized
                    val current = _joinedLobby.value ?: return@synchronized
                    val status = current.missionStatus ?: return@synchronized
                    if (
                        current.missionRequirement?.revision == requirement.revision &&
                        status.attempt == attempt &&
                        status.status in setOf(MissionCompatibilityStatus.QUEUED, MissionCompatibilityStatus.RETRYING)
                    ) {
                        if (attempt < 3) {
                            clientTransferAttempt = attempt + 1
                            sendMissionTransferRequest(current, requirement, attempt + 1)
                        } else {
                            val failed =
                                status.copy(
                                    status = MissionCompatibilityStatus.FAILED_RESUMABLE,
                                    failureCode = "grant_timeout",
                                )
                            _joinedLobby.value = current.copy(missionStatus = failed)
                            sendTo(
                                buildMissionStatus(current.lobbyId, hostCallsign, localClientId, failed),
                                current.hostAddr,
                            )
                        }
                    }
                }
            }
    }

    @Synchronized
    private fun retryMissionTransfer(
        requestId: String,
        requirement: MissionRequirement,
        attempt: Int,
    ) {
        if (pendingMissionTransferRequest != requestId) return
        val current = _joinedLobby.value ?: return
        if (current.missionRequirement?.revision != requirement.revision) return
        clientTransferAttempt = attempt
        sendMissionTransferRequest(current, requirement, attempt)
    }

    private fun handleMissionTransferRequest(
        json: JSONObject,
        senderAddr: String,
    ) {
        if (!_isHosting.value || json.optString("lobby_id") != hostedLobbyId) return
        val requirement = hostedMissionRequirement ?: return
        val revision = json.optString("revision")
        val attempt = json.optInt("attempt", 1).coerceIn(1, 10)
        val requestId = json.optString("request_id")
        if (requestId.isBlank() || requestId.length > 64) return
        val callsign = json.optString("callsign")
        val clientId = json.optString("client_id").takeIf(String::isNotBlank)
        val player =
            _hostedLobbyPlayers.value.find { lanPlayerMatchesSender(it, callsign, clientId, senderAddr) }
                ?: return
        val reconnected = !player.connected
        _hostedLobbyPlayers.value =
            _hostedLobbyPlayers.value.map {
                if (it === player) {
                    it.copy(connected = true, disconnectedAtMs = null, lastSeenMs = System.currentTimeMillis())
                } else {
                    it
                }
            }
        if (reconnected) notifyPlayerConnectionChanged(callsign, connected = true)
        val playerId = player.clientId ?: "${player.callsign}@$senderAddr"
        val grant = MissionTransferService.authorize(playerId, senderAddr, revision) ?: return
        sendTo(
            buildMissionTransferGrant(
                hostedLobbyId ?: return,
                grant.token,
                grant.port,
                requirement.revision,
                attempt,
                requestId,
            ),
            senderAddr,
        )
    }

    @Synchronized
    private fun handleMissionTransferGrant(
        json: JSONObject,
        senderAddr: String,
    ) {
        val joined = _joinedLobby.value ?: return
        val requirement = joined.missionRequirement ?: return
        if (senderAddr != joined.hostAddr || json.optString("lobby_id") != joined.lobbyId) return
        val revision = json.optString("revision")
        val token = json.optString("token")
        val port = json.optInt("port", 0)
        val attempt = json.optInt("attempt", 1).coerceIn(1, 10)
        val requestId = json.optString("request_id")
        if (requestId != pendingMissionTransferRequest || requestId == acceptedMissionTransferRequest) return
        if (attempt != joined.missionStatus?.attempt) return
        if (revision != requirement.revision || token.length !in 1..64 || port !in 1..65535) return
        acceptedMissionTransferRequest = requestId
        noteDirectedHostContact("MISSION_TRANSFER_GRANT")
        missionTransferGrantTimeoutJob?.cancel()
        missionTransferGrantTimeoutJob = null
        val context = appContext ?: return
        MissionTransferService.download(
            context,
            senderAddr,
            MissionTransferGrant(token, port, revision),
            requirement,
            attempt,
            onStatus = { report ->
                val current = _joinedLobby.value ?: return@download
                if (pendingMissionTransferRequest != requestId || current.lobbyId != joined.lobbyId ||
                    current.hostAddr != senderAddr || current.missionRequirement?.revision != report.revision
                ) {
                    return@download
                }
                if (!_joinedLobby.compareAndSet(current, current.copy(missionStatus = report))) return@download
                scope?.launch(Dispatchers.IO) {
                    if (pendingMissionTransferRequest != requestId) return@launch
                    sendTo(buildMissionStatus(current.lobbyId, hostCallsign, localClientId, report), current.hostAddr)
                }
            },
            onFinished = { success ->
                if (
                    !success &&
                    pendingMissionTransferRequest == requestId &&
                    attempt < 3 &&
                    _joinedLobby.value?.missionStatus?.status == MissionCompatibilityStatus.FAILED_RESUMABLE
                ) {
                    scope?.launch(Dispatchers.IO) {
                        delay(500L shl (attempt - 1))
                        retryMissionTransfer(requestId, requirement, attempt + 1)
                    }
                }
            },
        )
    }

    private fun handlePlayerList(json: JSONObject) {
        // Received by joiners -- update state for the lobby screen
        val lobbyId = json.optString("lobby_id", "")
        // The host is authoritative and must never consume a PLAYER_LIST packet.
        val joined = _joinedLobby.value
        val isOurLobby = joined != null && joined.lobbyId == lobbyId && !_isHosting.value
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
                    connected = pj.optBoolean("connected", true),
                    missionStatus = missionStatusFromJson(pj.optJSONObject("mission_status")),
                )
            }
        NetLog.log("LAN", "PLAYER_LIST: ${players.size} players, lobby=$lobbyId")
        // Track host liveness for joined lobby timeout
        if (_joinedLobby.value?.lobbyId == lobbyId) noteDirectedHostContact("PLAYER_LIST")
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
        if (!checkJoinGameReady(json.optString("game", "d2"))) {
            val leave = buildLeave(lobbyId, hostCallsign, localClientId)
            scope?.launch(Dispatchers.IO) { sendTo(leave, senderAddr) }
            updateLanForegroundSession()
            return
        }
        val requirement = missionRequirementFromJson(json.optJSONObject("mission_requirement"))
        val previous = _joinedLobby.value
        if (previous?.lobbyId != lobbyId || previous.hostAddr != senderAddr ||
            previous.missionRequirement?.revision != requirement?.revision
        ) {
            cancelMissionDownload()
        }
        val previousStatus =
            previous
                ?.takeIf {
                    it.lobbyId == lobbyId &&
                        it.missionRequirement?.revision == requirement?.revision
                }?.missionStatus
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
                stockVisualsEnforced = json.optBoolean(VisualReplacementPolicy.STOCK_VISUALS_ENFORCED, false),
                omittedVisualModCount = json.optInt(VisualReplacementPolicy.OMITTED_VISUAL_MOD_COUNT, 0),
                omittedVisualTextureCount = json.optInt(VisualReplacementPolicy.OMITTED_VISUAL_TEXTURE_COUNT, 0),
                omittedVisualModNames = VisualReplacementPolicy.namesFromJson(json),
                saveCompatibilityWarning = json.optString("save_compatibility_warning").takeIf { it.isNotBlank() },
                missionRequirement = requirement,
                missionStatus =
                    previousStatus
                        ?: requirement?.let {
                            MissionStatusReport(
                                revision = it.revision,
                                status = MissionCompatibilityStatus.CHECKING,
                                totalBytes = it.sizeBytes ?: 0L,
                            )
                        },
            )
        updateLanForegroundSession()
        NetLog.log("LAN", "JOIN_ACK received for lobby $lobbyId from $senderAddr")
        Log.i(TAG, "JOIN_ACK received for lobby $lobbyId from $senderAddr")
        noteDirectedHostContact("JOIN_ACK")
        startJoinedLobbyRefresh(immediate = false)
        if (requirement == null) {
            val ready = buildReady(lobbyId, hostCallsign, joinedLobbyReady, localClientId)
            scope?.launch(Dispatchers.IO) { sendTo(ready, senderAddr) }
        } else if (shouldResolveMissionAfterRefresh(previousStatus)) {
            resolveAndReportJoinedMission(requirement, json.optString("mode", "coop"))
        }
    }

    private fun resolveAndReportJoinedMission(
        requirement: MissionRequirement,
        mode: String,
    ) {
        val context = appContext ?: return
        scope?.launch(Dispatchers.IO) {
            val report = MissionCompatibilityResolver.resolve(context, requirement, mode)
            val joined = _joinedLobby.value ?: return@launch
            if (joined.missionRequirement?.revision != requirement.revision) return@launch
            if (!shouldResolveMissionAfterRefresh(joined.missionStatus)) return@launch
            _joinedLobby.value = joined.copy(missionStatus = report)
            sendTo(buildMissionStatus(joined.lobbyId, hostCallsign, localClientId, report), joined.hostAddr)
        }
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
            cancelMissionDownload()
            joinedLobbyRefreshJob?.cancel()
            joinedLobbyRefreshJob = null
            joinedLobbyReady = false
        }
        updateLanForegroundSession()
    }

    /**
     * Host calls this to start the game. Sends START to all joiners
     * and emits a launch event for the host side.
     */
    @Synchronized
    fun startGame(
        difficulty: Int,
        levelNum: Int,
        coopQol: Boolean = true,
        duplicateEnergyShields: Boolean = false,
        fullDeathSpew: Boolean = true,
        coopBriefings: Boolean = false,
        allowSecretWarps: Boolean = false,
        playerSpewNoExpire: Boolean = true,
        clientsCanRequestRewind: Boolean = false,
        restrictNonCoopFovToBase: Boolean = false,
        hostPort: Int = NetworkConstants.ENGINE_PORT,
    ) {
        if (!_isHosting.value) return
        if (pendingHostStart != null || gameStarted) return
        val lid = hostedLobbyId ?: return
        if (_hostedSaveChecking.value) {
            _diagnostics.value = "Checking the selected save. Please try again shortly."
            return
        }
        val saveWarning = hostSaveCompatibilityWarning()
        if (saveWarning != null) {
            _diagnostics.value = saveWarning
            NetLog.log("LAN", "Start blocked: $saveWarning")
            return
        }
        val players = _hostedLobbyPlayers.value
        if (players.size < 2) {
            _diagnostics.value = "Cannot start: at least two players are required"
            return
        }
        val incompatible =
            players.firstOrNull {
                !it.connected || it.missionStatus?.status != MissionCompatibilityStatus.MATCH || !it.ready
            }
        if (incompatible != null) {
            _diagnostics.value = "Cannot start: ${incompatible.callsign} is not ready with the matching mission"
            return
        }
        hostedHostPort = hostPort
        hostedRestrictNonCoopFovToBase = restrictNonCoopFovToBase

        // Send START to every joiner (redundant sends for reliability)
        val launchAttempt = ++hostLaunchAttempt
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
                duplicateEnergyShields = duplicateEnergyShields,
                fullDeathSpew = fullDeathSpew,
                coopBriefings = coopBriefings,
                allowSecretWarps = allowSecretWarps,
                playerSpewNoExpire = playerSpewNoExpire,
                clientsCanRequestRewind = clientsCanRequestRewind,
                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                stockVisualsEnforced = hostedStockVisualsEnforced,
                omittedVisualModCount = hostedOmittedVisualModCount,
                omittedVisualTextureCount = hostedOmittedVisualTextureCount,
                omittedVisualModNames = hostedOmittedVisualModNames,
                missionRequirement = hostedMissionRequirement,
            )
        // Capture values before launching coroutine (host fields are @Volatile)
        val game = hostedGame
        val mission = hostedMission
        val mode = hostedMode
        val maxPlayers = hostedMaxPlayers

        // Both launchers prepare concurrently; clients enter the engine only after commit
        val info =
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
                duplicateEnergyShields = duplicateEnergyShields,
                fullDeathSpew = fullDeathSpew,
                coopBriefings = coopBriefings,
                allowSecretWarps = allowSecretWarps,
                playerSpewNoExpire = playerSpewNoExpire,
                clientsCanRequestRewind = clientsCanRequestRewind,
                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                missionRequirement = hostedMissionRequirement,
                lanLobbyId = lid,
                lanLaunchId = launchAttempt,
            )
        val packet =
            JSONObject(String(data, Charsets.UTF_8))
                .put("launch_id", launchAttempt)
                .toString()
                .toByteArray(Charsets.UTF_8)
        pendingHostStart = PendingHostStart(lid, info, packet, players)
        publishHostLaunchPacket(MSG_PREPARE)
        _lanLaunchEvent.value = info
        NetLog.log("LAN", "Local host launch emitted: $game/$mission lvl=$levelNum diff=$difficulty")
    }

    @Synchronized
    fun confirmGameLaunch(info: com.dxxredux.app.multiplayer.GameLaunchInfo) {
        val pending = pendingHostStart ?: return
        if (pending.info !== info || pending.lobbyId != hostedLobbyId || !_isHosting.value || gameStarted) return
        // Mark game started (rejects further JOINs) but keep announcing
        // so in-game lobbies remain discoverable on LAN
        gameStarted = true
        inGameDifficulty = info.difficulty
        inGameLevelNum = info.levelNum
        publishHostLaunchPacket(MSG_START)
        NetLog.log("LAN", "Game started: ${info.game}/${info.mission} lvl=${info.levelNum} diff=${info.difficulty}")
    }

    private fun publishHostLaunchPacket(
        type: String,
        reason: String? = null,
    ) {
        val pending = pendingHostStart ?: return
        val packet = JSONObject(String(pending.packet, Charsets.UTF_8)).put("type", type)
        if (reason != null) packet.put("reason", reason)
        val data = packet.toString().toByteArray(Charsets.UTF_8)
        hostLaunchPacket = data
        hostStartJob?.cancel()
        hostStartJob =
            scope?.launch(Dispatchers.IO) {
                repeat(3) { retry ->
                    synchronized(this@LobbyService) {
                        if (!isActive || hostLaunchPacket !== data) return@launch
                        for (player in pending.players) {
                            if (player.address != "127.0.0.1") {
                                NetLog.log(
                                    "LAN",
                                    "Sending $type attempt=${pending.info.lanLaunchId} retry=$retry to=${player.address}",
                                )
                                sendTo(data, player.address)
                            }
                        }
                    }
                    delay(200)
                }
            }
    }

    @Synchronized
    fun failGameLaunch(
        info: com.dxxredux.app.multiplayer.GameLaunchInfo,
        message: String,
    ) {
        if (!info.isHost && info.lanLaunchId > 0) {
            val current = clientLaunchPreparation.value
            if (current != null && current.lobbyId == info.lanLobbyId && current.attempt == info.lanLaunchId) {
                clientLaunchPreparation.value = current.copy(stage = LanLaunchStage.CANCELLED, reason = message)
            }
            return
        }
        if (pendingHostStart?.info !== info) return
        stopInGameBroadcast()
        refreshHostedSaveWarning()
        if (_lanLaunchEvent.value === info) _lanLaunchEvent.value = null
        _diagnostics.value = message
        notifyLobbySystemMessage("Game start failed: $message")
    }

    /** Release a finished game's host session without discarding a waiting lobby */
    @Synchronized
    fun onGameExited() {
        if (gameStarted && _isHosting.value) {
            stopHosting()
        } else {
            stopInGameBroadcast()
        }
    }

    /** Reset launch state, preserving the waiting lobby when a launch fails or is cancelled */
    @Synchronized
    private fun stopInGameBroadcast() {
        val preparing = pendingHostStart != null && !gameStarted
        if (preparing) publishHostLaunchPacket(MSG_START_CANCEL, "Host cancelled game start")
        pendingHostStart = null
        if (!preparing) {
            hostStartJob?.cancel()
            hostStartJob = null
            hostLaunchPacket = null
        }
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

    @Synchronized
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
        val duplicateEnergyShields = json.optBoolean("duplicate_energy_shields", false)
        val fullDeathSpew = json.optBoolean("full_death_spew", true)
        val coopBriefings = json.optBoolean("coop_briefings", false)
        val allowSecretWarps = json.optBoolean("allow_secret_warps", false)
        val playerSpewNoExpire = json.optBoolean("player_spew_no_expire", true)
        val clientsCanRequestRewind = json.optBoolean("clients_can_request_rewind", false)
        val restrictNonCoopFovToBase = json.optBoolean("restrict_noncoop_fov_to_base", false)
        val requirement = missionRequirementFromJson(json.optJSONObject("mission_requirement"))
        NetLog.log(
            "LAN",
            "${json.optString("type")} received: $game/$mission lvl=$levelNum diff=$difficulty from $senderAddr",
        )
        Log.i(TAG, "START received for lobby $lobbyId: $game/$mission at $senderAddr:$hostPort")

        val joinedInfo = _joinedLobby.value
        if (joinedInfo == null) {
            NetLog.log("LAN", "START ignored: not in a joined lobby")
            Log.w(TAG, "START received but not in a joined lobby, ignoring")
            return
        }
        if (senderAddr != joinedInfo.hostAddr || lobbyId != joinedInfo.lobbyId) return
        val attempt = json.optLong("launch_id", 0L)
        val stage =
            when (json.optString("type")) {
                MSG_PREPARE -> LanLaunchStage.PREPARING
                MSG_START_CANCEL -> LanLaunchStage.CANCELLED
                else -> LanLaunchStage.COMMITTED
            }
        if (stage != LanLaunchStage.COMMITTED && attempt <= 0L) return
        val previous = clientLaunchPreparation.value
        val next =
            LanLaunchPreparation(
                lobbyId,
                senderAddr,
                attempt,
                stage,
                json.optString("reason").takeIf { it.isNotBlank() },
            )
        if (!acceptLanLaunchUpdate(previous, next)) return
        if (stage == LanLaunchStage.CANCELLED) {
            clientLaunchPreparation.value = next
            return
        }
        if (requirement != null &&
            (
                joinedInfo.missionRequirement?.revision != requirement.revision ||
                    joinedInfo.missionStatus?.status != MissionCompatibilityStatus.MATCH
            )
        ) {
            _diagnostics.value = "Cannot join: mission is missing or does not match the host"
            return
        }
        if (lobbyId.isNotEmpty() && lobbyId != joinedInfo.lobbyId) {
            NetLog.log("LAN", "START ignored: lobbyId mismatch (got=$lobbyId, joined=${joinedInfo.lobbyId})")
            Log.w(TAG, "START lobbyId mismatch: got=$lobbyId expected=${joinedInfo.lobbyId}")
            return
        }

        clientLaunchPreparation.value = next
        NetLog.log("LAN", "Launch stage=$stage lobby=$lobbyId attempt=$attempt")
        if (previous?.lobbyId == lobbyId && previous.attempt == attempt) return
        // Emit exactly once, including when START arrives before PREPARE
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
                duplicateEnergyShields = duplicateEnergyShields,
                fullDeathSpew = fullDeathSpew,
                coopBriefings = coopBriefings,
                allowSecretWarps = allowSecretWarps,
                playerSpewNoExpire = playerSpewNoExpire,
                clientsCanRequestRewind = clientsCanRequestRewind,
                restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                missionRequirement = requirement,
                lanLobbyId = lobbyId,
                lanLaunchId = attempt,
            )
        NetLog.log("LAN", "Launch event emitted for joiner: game=$game host=$senderAddr")
    }

    suspend fun awaitHostLaunch(info: com.dxxredux.app.multiplayer.GameLaunchInfo): String? {
        if (!info.isLan || info.isHost || info.lanLaunchId == 0L) return null
        NetLog.log(
            "LAN",
            "Client preparation ready; waiting for host lobby=${info.lanLobbyId} attempt=${info.lanLaunchId}",
        )
        val outcome =
            withTimeoutOrNull(120_000L) {
                combine(clientLaunchPreparation, _joinedLobby) { state, joined ->
                    when {
                        joined == null || joined.lobbyId != info.lanLobbyId || joined.hostAddr != info.lanHostAddr -> {
                            "Left the host lobby"
                        }

                        state == null || state.lobbyId != info.lanLobbyId || state.attempt != info.lanLaunchId -> {
                            "Host replaced this game start"
                        }

                        state.stage == LanLaunchStage.CANCELLED -> {
                            state.reason ?: "Host cancelled game start"
                        }

                        state.stage == LanLaunchStage.COMMITTED -> {
                            ""
                        }

                        else -> {
                            null
                        }
                    }
                }.first { it != null }
            }
        return outcome?.takeIf { it.isNotEmpty() }
            ?: if (outcome == null) "Host did not finish starting. Please try again." else null
    }

    // Discovery and joins use the host's engine, independently of single-player preferences
    fun joinDiscoveredLobby(
        announce: LanLobbyAnnounce,
        callsign: String,
    ) {
        manualIpAttempt.incrementAndGet()
        if (!checkJoinGameReady(announce.game)) return
        _diagnostics.value = ""
        if (announce.status == "in_game") {
            if (_isHosting.value) stopHosting()
            leaveLanLobby(hostCallsign)
            emitInGameJoinLaunch(announce)
        } else {
            joinLobby(announce.lobbyId, announce.hostAddress, callsign)
        }
    }

    private fun checkJoinGameReady(game: String): Boolean {
        val context = appContext ?: return false
        val fileSets = FileSetManager(context.filesDir)
        val activeSet = fileSets.getActive()
        val setDir = fileSets.getSetDir(activeSet)
        val warning =
            lanGameReadinessWarning(game, setDir, AssetManifest(setDir), fileSets.safManifestForSet(activeSet))
        if (warning != null) {
            _diagnostics.value = warning
            NetLog.log("LAN", warning)
            return false
        }
        return true
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
                missionRequirement = announce.missionRequirement,
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
        if (lobbyId.isEmpty() || lobbyId != hostedLobbyId) return
        val callsign = json.optString("callsign", "")
        val clientId = json.optString("client_id", "").takeIf { it.isNotBlank() }
        val player =
            _hostedLobbyPlayers.value.find { lanPlayerMatchesSender(it, callsign, clientId, senderAddr) }
        if (player == null) {
            NetLog.log("LAN", "Ignored PING from unknown player $callsign at $senderAddr")
            return
        }
        val reconnected = !player.connected
        _hostedLobbyPlayers.value =
            _hostedLobbyPlayers.value.map {
                if (it === player) {
                    it.copy(connected = true, disconnectedAtMs = null, lastSeenMs = System.currentTimeMillis())
                } else {
                    it
                }
            }
        val ts = json.optLong("ts", 0)
        if (ts > 0L) sendTo(buildPong(lobbyId, ts), senderAddr)
        if (reconnected) notifyPlayerConnectionChanged(callsign, connected = true)
    }

    private fun handlePong(
        json: JSONObject,
        senderAddr: String,
    ) {
        val joined = _joinedLobby.value ?: return
        if (senderAddr != joined.hostAddr || json.optString("lobby_id") != joined.lobbyId) return
        if (json.optLong("ts", 0L) <= 0L) return
        noteDirectedHostContact("PONG")
    }

    /** Respond to a QUERY with a direct ANNOUNCE so the querier discovers our lobby. */
    @Synchronized
    private fun handleQuery(
        query: JSONObject,
        senderAddr: String,
    ) {
        val lid = hostedLobbyId
        if (lid == null) {
            NetLog.log("LAN", "QUERY ignored: not hosting from=$senderAddr id=${query.optString("trace_id")}")
            return
        }
        // A client query may get through even when it cannot receive our broadcasts
        // Keep sending to that client for a bounded time if the first reply is lost
        if (discoveryPeers.containsKey(senderAddr) || discoveryPeers.size < MAX_DISCOVERY_PEERS) {
            if (discoveryPeers.put(senderAddr, android.os.SystemClock.elapsedRealtime()) == null) {
                NetLog.log("LAN", "Discovery unicast peer added address=$senderAddr")
            }
        }
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
                stockVisualsEnforced = hostedStockVisualsEnforced,
                omittedVisualModCount = hostedOmittedVisualModCount,
                omittedVisualTextureCount = hostedOmittedVisualTextureCount,
                omittedVisualModNames = hostedOmittedVisualModNames,
                missionRequirement = hostedMissionRequirement,
                saveCompatibilityWarning = hostSaveCompatibilityWarning(),
                queryReply = true,
            )
        sendTo(discoveryQueryReply(data, query), senderAddr)
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
            if (json.optString("lobby_id") != hostedLobbyId) return
            val player =
                _hostedLobbyPlayers.value.find {
                    lanPlayerMatchesSender(it, callsign, clientId = null, senderAddress = senderAddr)
                }
            if (player == null) {
                NetLog.log("LAN", "Ignored CHAT from unknown player $callsign at $senderAddr")
                return
            }
            val reconnected = !player.connected
            _hostedLobbyPlayers.value =
                _hostedLobbyPlayers.value.map {
                    if (it === player) {
                        it.copy(connected = true, disconnectedAtMs = null, lastSeenMs = System.currentTimeMillis())
                    } else {
                        it
                    }
                }
            if (reconnected) notifyPlayerConnectionChanged(callsign, connected = true)
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
            val joined = _joinedLobby.value ?: return
            if (senderAddr != joined.hostAddr || json.optString("lobby_id") != joined.lobbyId) return
            noteDirectedHostContact("CHAT")
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
        cancelMissionDownload()
        _hostedLobbyPlayers.value = emptyList()
        _chatMessages.value = emptyList()
        lastHostSeenMs = 0L
        lastHostPongMs = 0L
        hostReconnectStartedMs = 0L
        joinedLobbyReady = false
        joinedLobbyRefreshJob?.cancel()
        joinedLobbyRefreshJob = null
        _diagnostics.value = "Kicked from lobby by host"
        updateLanForegroundSession()
        Log.i(TAG, "Kicked from lobby ${joined.lobbyId}")
    }

    private fun hostSaveCompatibilityWarning(): String? =
        if (hostedMode == "coop" && !gameStarted) {
            if (_hostedSaveChecking.value) "Checking the selected save" else _hostedSaveWarning.value
        } else {
            null
        }

    private fun cancelHostedSaveCheck() {
        hostedSaveCheckGeneration++
        hostedSaveCheckJob?.cancel()
        hostedSaveCheckJob = null
        _hostedSaveChecking.value = false
        _hostedSaveWarning.value = null
        hostedSaveRevision = null
    }

    private fun refreshChangedHostedSave() {
        val context = appContext ?: return
        val lobbyId = hostedLobbyId ?: return
        if (!_isHosting.value || hostedMode != "coop" || gameStarted || _hostedSaveChecking.value) return
        val revision =
            com.dxxredux.app.multiplayer.CoopSaveCompatibility.hostRevision(
                context.filesDir,
                hostedGame,
                hostedMission,
            )
        synchronized(this) {
            if (lobbyId == hostedLobbyId && !_hostedSaveChecking.value && revision != hostedSaveRevision) {
                refreshHostedSaveWarning()
            }
        }
    }

    @Synchronized
    fun refreshHostedSaveWarning() {
        cancelHostedSaveCheck()
        if (!_isHosting.value || hostedMode != "coop" || gameStarted) return
        val context = appContext ?: return
        val generation = hostedSaveCheckGeneration
        val lobbyId = hostedLobbyId
        val game = hostedGame
        val mission = hostedMission
        _hostedSaveChecking.value = true
        // Transport replacement must not cancel validation and leave Checking stuck
        hostedSaveCheckJob =
            transportSupervisorScope.launch {
                val started = android.os.SystemClock.elapsedRealtime()
                val revision =
                    com.dxxredux.app.multiplayer.CoopSaveCompatibility.hostRevision(
                        context.filesDir,
                        game,
                        mission,
                    )
                val validator = hostedSaveCheckForTest
                val warning =
                    try {
                        if (validator != null) {
                            validator(game, mission)
                        } else {
                            com.dxxredux.app.multiplayer.CoopSaveCompatibility
                                .hostWarning(context.filesDir, game, mission)
                        }
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        NetLog.log("LAN", "Hosted save check failed lobby=$lobbyId error=${e.message}")
                        "Could not check the selected save. Please retry."
                    }
                synchronized(this@LobbyService) {
                    if (!isActive || generation != hostedSaveCheckGeneration || lobbyId != hostedLobbyId) return@launch
                    _hostedSaveWarning.value = warning
                    hostedSaveRevision = revision
                    _hostedSaveChecking.value = false
                    if (warning != null) _diagnostics.value = warning
                    NetLog.log(
                        "LAN",
                        "Hosted save check complete lobby=$lobbyId elapsed_ms=${android.os.SystemClock.elapsedRealtime() - started} compatible=${warning == null}",
                    )
                }
            }
    }

    @Synchronized
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
                stockVisualsEnforced = hostedStockVisualsEnforced,
                omittedVisualModCount = hostedOmittedVisualModCount,
                omittedVisualTextureCount = hostedOmittedVisualTextureCount,
                omittedVisualModNames = hostedOmittedVisualModNames,
                saveCompatibilityWarning = hostSaveCompatibilityWarning(),
            )
        sendBroadcast(data)
        val now = android.os.SystemClock.elapsedRealtime()
        discoveryPeers.entries.removeAll { now - it.value >= DISCOVERY_PEER_EXPIRY_MS }
        discoveryPeers.keys.forEach { sendTo(data, it, "discovery-peer") }
    }

    private fun broadcastPlayerList() {
        val lid = hostedLobbyId ?: return
        val data = buildPlayerList(lid, _hostedLobbyPlayers.value)
        broadcastToJoiners(data)
    }

    private fun sendPlayerList(address: String) {
        val lid = hostedLobbyId ?: return
        sendTo(buildPlayerList(lid, _hostedLobbyPlayers.value), address)
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
                val wireData = traceDiscoverySend(data, addr.hostAddress.orEmpty(), "subnet-broadcast", activeSocket)
                val packet =
                    DatagramPacket(
                        wireData,
                        wireData.size,
                        addr,
                        NetworkConstants.LAN_LOBBY_PORT,
                    )
                activeSocket.send(packet)
                logDiscoverySent(wireData)
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
        strategy: String = "unicast",
    ) {
        try {
            val activeSocket = socket
            if (activeSocket == null || activeSocket.isClosed) {
                NetLog.log("LAN", "Send deferred to $address: socket unavailable")
                return
            }
            val addr = InetAddress.getByName(address)
            val wireData = traceDiscoverySend(data, address, strategy, activeSocket)
            val packet =
                DatagramPacket(
                    wireData,
                    wireData.size,
                    addr,
                    NetworkConstants.LAN_LOBBY_PORT,
                )
            activeSocket.send(packet)
            logDiscoverySent(wireData)
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

        // Mark lost peers promptly, but reserve their identity and slot for seamless reconnect
        if (_isHosting.value) {
            val players = _hostedLobbyPlayers.value
            val reconnecting =
                players.filter {
                    lanPlayerLeaseAction(
                        it,
                        now,
                    ) == LanPlayerLeaseAction.MARK_RECONNECTING
                }
            val expired = players.filter { lanPlayerLeaseAction(it, now) == LanPlayerLeaseAction.REMOVE }
            if (reconnecting.isNotEmpty()) {
                _hostedLobbyPlayers.value =
                    players.map { player ->
                        if (player in reconnecting) {
                            player.copy(ready = false, connected = false, disconnectedAtMs = now)
                        } else {
                            player
                        }
                    }
                reconnecting.forEach { notifyPlayerConnectionChanged(it.callsign, connected = false) }
            }
            if (expired.isNotEmpty()) {
                _hostedLobbyPlayers.value = _hostedLobbyPlayers.value - expired.toSet()
                expired.forEach { notifyLobbySystemMessage("${it.callsign} disconnected") }
                broadcastPlayerList()
            }
        }

        // Directed replies, not broadcast announcements, prove the host path works both ways
        val joined = _joinedLobby.value
        if (joined != null && lastHostPongMs > 0L && now - lastHostPongMs > LAN_PEER_TIMEOUT_MS) {
            if (hostReconnectStartedMs == 0L) {
                hostReconnectStartedMs = now
                _diagnostics.value = LAN_RECONNECTING_DIAGNOSTIC
                NetLog.log("LAN", "Host directed contact lost, reconnecting to ${joined.hostAddr}")
            } else if (now - hostReconnectStartedMs > LAN_RECONNECT_GRACE_MS) {
                NetLog.log("LAN", "Host reconnect grace expired for ${joined.hostAddr}")
                _joinedLobby.value = null
                cancelMissionDownload()
                _hostedLobbyPlayers.value = emptyList()
                _diagnostics.value = "Host disconnected after reconnect timeout"
                lastHostSeenMs = 0L
                lastHostPongMs = 0L
                hostReconnectStartedMs = 0L
                joinedLobbyReady = false
                joinedLobbyRefreshJob?.cancel()
                joinedLobbyRefreshJob = null
                updateLanForegroundSession()
            }
        }
    }

    private fun updateLanForegroundSession() {
        setLanForegroundSessionActive(_isHosting.value || _joinedLobby.value != null)
    }

    @Synchronized
    private fun setLanForegroundSessionActive(active: Boolean) {
        if (active == lanForegroundSessionStarted) return
        val context = appContext ?: return
        if (active) {
            MultiplayerForegroundService.startLanSession(context)
        } else {
            MultiplayerForegroundService.stopLanSession(context)
        }
        lanForegroundSessionStarted = active
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
            NetLog.log("LAN", "Broadcast destinations: ${getBroadcastAddresses().map { it.hostAddress }}")
            Log.i(TAG, "Local addresses: $addrs")
        } catch (e: Exception) {
            NetLog.log("LAN", "Failed to enumerate addresses: ${e.message}")
            Log.w(TAG, "Failed to enumerate local addresses: ${e.message}")
        }
    }

    private fun logTransportHealth(nowMs: Long) {
        val activeSocket = socket
        val joined = _joinedLobby.value
        val role =
            if (_isHosting.value) {
                "host"
            } else if (joined != null) {
                "client"
            } else {
                "discovery"
            }
        val hostAgeSeconds = if (joined != null && lastHostSeenMs > 0L) (nowMs - lastHostSeenMs) / 1000L else -1L
        val playerAges =
            _hostedLobbyPlayers.value
                .filter { it.address != "127.0.0.1" }
                .joinToString(",") { "${it.callsign}:${(nowMs - it.lastSeenMs) / 1000L}s" }
        NetLog.log(
            "LAN",
            "Transport health: role=$role socket=${activeSocket?.localPort}/${activeSocket?.isBound}/" +
                "${activeSocket?.isClosed} jobs=recv:${receiveJob?.isActive},announce:${announceJob?.isActive}," +
                "heartbeat:${joinedLobbyRefreshJob?.isActive},prune:${pruneJob?.isActive} " +
                "packets=tx:${packetsSent.get()},rx:${packetsReceived.get()} hostAge=${hostAgeSeconds}s " +
                "playerAges=[$playerAges] rxByAddress=${packetsReceivedByAddress.toSortedMap()}",
        )
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
        trackTransportJob("announce", announceJob) { _isHosting.value }
    }

    private fun sendJoinAck(
        lobbyId: String,
        address: String,
    ) {
        sendTo(
            buildJoinAck(
                lobbyId,
                hostedGame,
                hostedMission,
                hostedMode,
                hostedMaxPlayers,
                hostCallsign,
                localClientId,
                hostedStockVisualsEnforced,
                hostedOmittedVisualModCount,
                hostedOmittedVisualTextureCount,
                hostedOmittedVisualModNames,
                missionRequirement = hostedMissionRequirement,
                saveCompatibilityWarning = hostSaveCompatibilityWarning(),
            ),
            address,
        )
    }

    private fun startJoinedLobbyRefresh(immediate: Boolean) {
        if (joinedLobbyRefreshJob?.isActive == true) return
        joinedLobbyRefreshJob =
            scope?.launch(Dispatchers.IO) {
                if (!immediate) delay(JOINED_LOBBY_REFRESH_MS)
                while (isActive) {
                    val joined = _joinedLobby.value ?: return@launch
                    val heartbeat =
                        buildJoinedLobbyHeartbeat(
                            joined.lobbyId,
                            hostCallsign,
                            localClientId,
                            joinedLobbyReady,
                            joined.missionStatus,
                        )
                    heartbeat.forEach { sendTo(it, joined.hostAddr) }
                    delay(JOINED_LOBBY_REFRESH_MS)
                }
            }
        trackTransportJob("joined-lobby heartbeat", joinedLobbyRefreshJob) { _joinedLobby.value != null }
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
