package com.dxxredux.app.multiplayer

import android.app.Activity
import android.util.Log
import com.dxxredux.app.BuildConfig
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.lang.ref.WeakReference
import java.net.DatagramSocket
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

/** Only the explicit debug emulator host alias may translate an observed host-loopback endpoint */
internal fun localDevelopmentEndpoint(
    debug: Boolean,
    controlHost: String?,
    endpoint: String,
): String {
    if (!debug || controlHost != "10.0.2.2" || !endpoint.startsWith("127.0.0.1:")) return endpoint
    val parsed = parseProbeEndpoint(endpoint) ?: return endpoint
    return "10.0.2.2:${parsed.port}"
}

/** Retained PoW fixtures are limited to debug builds and literal local test endpoints */
internal fun allowsInstallationAuth(
    debug: Boolean,
    host: String,
): Boolean =
    debug && (
        host == "localhost" || host == "::1" || host == "10.0.2.2" ||
            host.split('.').let { octets ->
                octets.size == 4 && octets[0] == "127" && octets.all { it.toIntOrNull() in 0..255 }
            }
    )

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
    // Host integration tests replace this private scope to advance reconnect time deterministically
    private var scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
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
    private var connectionGeneration = 0L
    private var networkGeneration = 0L
    private val probeMutex = Mutex()
    private var gameHandoffJob: Job? = null

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

    private data class PendingCandidateChallenge(
        val connection: WebSocket,
        val generation: Long,
        val lobbyId: String,
        val admissionId: String,
        val requestId: String,
        val reply: CompletableDeferred<CandidateChallengeMsg>,
    )

    private var pendingCandidateChallenge: PendingCandidateChallenge? = null

    @Volatile
    private var stunCompleted = false

    /** gameInfo sent with the last CREATE_LOBBY, so the first LOBBY_UPDATE can carry it. */
    @Volatile
    private var pendingGameInfo: JsonObject = JsonObject(emptyMap())

    @Volatile
    private var localhostProxy: LocalhostProxy? = null

    @Volatile
    private var connectivityCheckJob: Job? = null
    private var discoveredCandidates: List<ConnectionCandidate> = emptyList()
    private var probeDiagnostics: Map<String, Int> = emptyMap()
    private var connectivityCheckEpoch = 0L
    private var connectivityResults: Map<String, Map<String, Any>> = emptyMap()

    // Host updates are published by MainActivity through the foreground-service
    // Binder because the native engine runs in the separate :game process.
    @Volatile
    private var gameStateUpdatesEnabled = false

    @Volatile
    private var upnpMapping: UpnpMapping? = null

    // Shared UDP socket used for STUN, connectivity checks, and then handed
    // to the first PeerProxy so the UPnP port mapping stays valid.
    @Volatile
    private var candidateSocket: java.net.DatagramSocket? = null

    // Test-only: override STUN server addresses for Docker NAT testing.
    // When set, launchStunDiscovery() uses these instead of AUTH_OK's stunAddrs.
    @Volatile
    private var stunOverrideAddrs: List<String>? = null

    fun getProxyStats(): List<PeerProxyStats> = localhostProxy?.getStats() ?: emptyList()

    /** Set (or clear) STUN server address override for NAT testing. */
    fun setStunOverride(addrs: List<String>?) {
        stunOverrideAddrs = addrs
    }

    @Synchronized
    fun shutdownProxy(expected: LocalhostProxy? = null) {
        if (expected != null && localhostProxy !== expected) return
        localhostProxy?.shutdown()
        localhostProxy = null
    }

    internal fun currentProxy(): LocalhostProxy? = localhostProxy

    private var runtimeAttempt: String? = null
    private var runtimeAttemptEpoch = 0L
    private var gameEpoch: String? = null
    private val pendingAdmissions = mutableMapOf<Int, String>()

    @Synchronized
    internal fun hostNativeAdmitted(
        attempt: String,
        admissionId: String,
        slot: Int,
    ) {
        if (!ownsRuntimeAttempt(attempt) || pendingAdmissions[slot] != admissionId) return
        val room = state.state.value.currentLobby ?: return
        val epoch = gameEpoch ?: return
        if (!room.isHost) return
        send(
            kotlinx.serialization.json
                .buildJsonObject {
                    put("type", kotlinx.serialization.json.JsonPrimitive("HOST_NATIVE_ADMITTED"))
                    put("lobby_id", kotlinx.serialization.json.JsonPrimitive(room.lobbyId))
                    put("game_epoch", kotlinx.serialization.json.JsonPrimitive(epoch))
                    put("admission_id", kotlinx.serialization.json.JsonPrimitive(admissionId))
                    put("slot", kotlinx.serialization.json.JsonPrimitive(slot))
                }.toString(),
        )
        pendingAdmissions.remove(slot)
    }

    @Synchronized
    internal fun ownsRuntimeAttempt(attempt: String): Boolean = runtimeAttempt == attempt

    @Synchronized
    internal fun networkAttempt(): Long = networkGeneration

    @Synchronized
    internal fun failGameLaunch(
        generation: Long,
        isHost: Boolean,
    ) {
        if (generation != networkGeneration) return
        if (isHost) endGame() else leaveLobby()
    }

    private fun newProxy(
        sharedSocket: DatagramSocket?,
        dynamic: Boolean = false,
    ): LocalhostProxy {
        lateinit var proxy: LocalhostProxy
        proxy =
            LocalhostProxy(scope, sharedSocket, dynamic) { message ->
                synchronized(this) {
                    if (localhostProxy === proxy) {
                        if (state.state.value.currentLobby != null) leaveLobby() else shutdownProxy()
                        state.update { it.copy(errorMessage = message, gameLaunchInfo = null) }
                        state.appendLog(message)
                        NetLog.log("ERROR", message)
                        activityRef?.get()?.applicationContext?.let { context ->
                            android.os.Handler(android.os.Looper.getMainLooper()).post {
                                android.widget.Toast
                                    .makeText(context, message, android.widget.Toast.LENGTH_LONG)
                                    .show()
                            }
                        }
                    }
                }
            }
        state.state.value.currentLobby?.let { lobby ->
            val ownId = state.state.value.playerId
            proxy.reconcileRoster(lobby.players.filter { it.playerId != ownId }.associate { it.slot to it.playerId })
        }
        return proxy
    }

    /**
     * Create a proxy between the game engine (loopback) and the network.
     * Used for both client-side (joining) and host-side (after migration)
     * proxying -- the same system, just different configuration.
     *
     * @param listenPort if non-null, bind a shared socket on this port and
     *                   accept dynamic peers (host/listening mode)
     * @param peerAddr   if non-null, add an initial peer at this address
     *                   (client/joining mode)
     * @param peerPort   port for the initial peer (ignored if peerAddr is null)
     */
    @Synchronized
    fun createProxy(
        listenPort: Int? = null,
        peerAddr: String? = null,
        peerPort: Int = NetworkConstants.ENGINE_PORT,
    ): LocalhostProxy {
        shutdownProxy()
        val sharedSocket =
            if (listenPort != null) {
                val socket = DatagramSocket(null)
                try {
                    socket.reuseAddress = true
                    socket.bind(InetSocketAddress(listenPort))
                    socket
                } catch (e: Exception) {
                    socket.close()
                    throw e
                }
            } else {
                null
            }
        val proxy = newProxy(sharedSocket, dynamic = listenPort != null)
        try {
            if (peerAddr != null) {
                proxy.addPeer(
                    PeerProxyConfig(
                        peerSlot = 0,
                        localPort = NetworkConstants.PROXY_PORT_BASE,
                        realAddr = InetSocketAddress(peerAddr, peerPort),
                        isRelay = false,
                    ),
                )
            }
            localhostProxy = proxy
            proxy.start()
        } catch (e: Exception) {
            proxy.shutdown()
            if (localhostProxy === proxy) localhostProxy = null
            throw e
        }
        Log.i("DXX-MP", "Proxy created: listen=${listenPort ?: "none"} peer=${peerAddr ?: "dynamic"}")
        return proxy
    }

    // Unique per app process -- ensures two emulators/devices get different player IDs
    // when using dev mode (SKIP_GPGS_VERIFY=true on the server)
    private var installationAuth: OnlineInstallationAuth? = null
    private var installationAuthJob: Job? = null
    private var installationDeadlineJob: Job? = null
    private var installationAttempts = 0
    private var installationBudgetGeneration = 0L
    private var installationDeadlineNanos = 0L

    private fun clearInstallationBudget() {
        installationBudgetGeneration++
        installationDeadlineJob?.cancel()
        installationDeadlineJob = null
        installationAttempts = 0
        installationDeadlineNanos = 0L
    }

    private fun installationTimeRemaining(): Boolean {
        if (installationDeadlineJob != null && System.nanoTime() - installationDeadlineNanos >= 0) {
            authenticationUnavailable("Installation authentication could not complete (TOTAL_DEADLINE)")
            return false
        }
        return true
    }

    // This budget survives socket replacement; only explicit connect or AUTH_OK resets it
    private fun beginInstallationWork(): Boolean {
        if (!installationTimeRemaining()) return false
        if (installationAttempts >= 3) {
            authenticationUnavailable("Installation authentication could not complete (WORK_DEADLINE)")
            return false
        }
        installationAttempts++
        if (installationDeadlineJob == null) {
            val budgetGeneration = installationBudgetGeneration
            installationDeadlineNanos = System.nanoTime() + 45_000_000_000L
            installationDeadlineJob =
                scope.launch {
                    delay(((installationDeadlineNanos - System.nanoTime()) / 1_000_000L).coerceAtLeast(0L))
                    synchronized(this@MatchmakingService) {
                        if (budgetGeneration == installationBudgetGeneration) {
                            authenticationUnavailable("Installation authentication could not complete (TOTAL_DEADLINE)")
                        }
                    }
                }
        }
        return true
    }

    private fun retryInstallationWork() {
        if (!installationTimeRemaining()) return
        if (installationAttempts >= 3) {
            authenticationUnavailable("Installation authentication could not complete (WORK_DEADLINE)")
            return
        }
        // A new socket obtains a new single-use challenge and a fresh server preauth deadline
        val previous = webSocket
        webSocket = null
        previous?.cancel()
        retireNetworkWork()
        state.appendLog("Installation work expired; requesting a fresh challenge")
        scheduleReconnect()
    }

    @Volatile private var installationWork: Map<String, Any> = emptyMap()

    // Weak reference to current activity for GPGS auth code requests.
    // Set by SetupActivity.onCreate(), cleared automatically by GC.
    private var activityRef: WeakReference<Activity>? = null
    private var googleSignInJob: Job? = null
    private var googleSession: GoogleServerSession? = null

    @Synchronized
    fun setActivity(activity: Activity) {
        if (activityRef?.get() !== activity && googleSignInJob?.isActive == true) {
            authenticationUnavailable("Sign-in interrupted; choose Connect again")
        }
        activityRef = WeakReference(activity)
    }

    @Synchronized
    fun clearActivity(activity: Activity) {
        if (activityRef?.get() !== activity) return
        if (googleSignInJob?.isActive == true) {
            authenticationUnavailable("Sign-in interrupted; choose Connect again")
        }
        activityRef = null
    }

    @Synchronized
    fun connect(
        serverUrl: String,
        callsign: String,
    ) {
        disconnect()
        reconnectAttempt = 0
        clearInstallationBudget()
        installationWork = emptyMap()
        lastLobbyId = null
        state.update {
            it.copy(
                currentLobby = null,
                gameLaunchInfo = null,
                nav = MultiplayerNav.BROWSER,
                playerId = null,
                sessionToken = null,
                connectionInfo = emptyList(),
                peerCandidates = emptyMap(),
                connectivityPairs = emptyList(),
                relayInfo = null,
                iceStatus = IceStatus(),
            )
        }
        val normalized = normalizeServerUrl(serverUrl)
        val request = runCatching { Request.Builder().url(normalized).build() }.getOrNull()
        if (request == null) {
            authenticationUnavailable("Invalid matchmaking server address")
            return
        }
        if (allowsInstallationAuth(BuildConfig.DEBUG, request.url.host)) {
            openConnection(serverUrl, callsign)
            return
        }
        val activity = activityRef?.get()
        if (activity == null || !GoogleMatchmakingAuth.isConfigured) {
            authenticationUnavailable("Google sign-in is unavailable; LAN play remains available")
            return
        }
        val login = runCatching { GoogleLoginClient(normalized) }.getOrNull()
        if (login == null) {
            authenticationUnavailable("Google sign-in requires a secure matchmaking server address")
            return
        }
        manualDisconnect = false
        val generation = connectionGeneration
        state.update {
            it.copy(
                status = ConnectionStatus.CONNECTING,
                serverUrl = serverUrl.trim(),
                callsign = callsign,
                errorMessage = null,
            )
        }
        googleSignInJob =
            scope.launch {
                try {
                    val session = login.signIn { nonce -> GoogleMatchmakingAuth.signIn(activity, nonce) }
                    synchronized(this@MatchmakingService) {
                        if (generation != connectionGeneration || manualDisconnect) {
                            revokeGoogleSession(session)
                        } else {
                            googleSession = session
                            googleSignInJob = null
                            openConnection(serverUrl, callsign)
                        }
                    }
                } catch (cancelled: CancellationException) {
                    throw cancelled
                } catch (error: Exception) {
                    synchronized(this@MatchmakingService) {
                        if (generation == connectionGeneration && !manualDisconnect) {
                            authenticationUnavailable(googleLoginFailureMessage(error))
                        }
                    }
                }
            }
    }

    @Synchronized
    private fun openConnection(
        serverUrl: String,
        callsign: String,
    ) {
        lobbyPageJob?.cancel()
        lobbyPageRequest = null
        lobbyListingId = null
        connectionGeneration++
        manualDisconnect = false
        reconnectJob?.cancel()
        val previous = webSocket
        webSocket = null
        previous?.cancel()
        retireNetworkWork()

        val normalizedUrl = normalizeServerUrl(serverUrl)
        val endpoint = runCatching { Request.Builder().url(normalizedUrl).build() }.getOrNull()
        if (endpoint == null) {
            authenticationUnavailable("Invalid matchmaking server address")
            return
        }
        if (!allowsInstallationAuth(BuildConfig.DEBUG, endpoint.url.host) &&
            !runCatching { googleSession?.validFor(googleLoginOrigin(normalizedUrl)) == true }.getOrDefault(false)
        ) {
            authenticationUnavailable("Sign-in expired; choose Connect to sign in again")
            return
        }
        if (!BuildConfig.DEBUG && !normalizedUrl.startsWith("wss://")) {
            authenticationUnavailable("Online connections require TLS")
            return
        }
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
        val httpClient = if (BuildConfig.DEBUG && isPrivateAddress(host)) lanClient else client
        webSocket = httpClient.newWebSocket(request, Listener(callsign))
    }

    @Synchronized
    fun disconnect() {
        clearGoogleSignIn()
        clearInstallationBudget()
        lobbyPageJob?.cancel()
        lobbyPageRequest = null
        lobbyListingId = null
        connectionGeneration++
        manualDisconnect = true
        lastLobbyId = null
        reconnectJob?.cancel()
        retireNetworkWork()
        val previous = webSocket
        webSocket = null
        previous?.cancel()
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
        Log.d(TAG, "Sending matchmaking message")
        ws.send(json)
    }

    fun sendAuthenticate(callsign: String) {
        val connection = webSocket ?: return
        withConnection(connection) {
            if (allowsInstallationAuth(BuildConfig.DEBUG, connection.request().url.host)) {
                sendInstallationAuthenticate(callsign)
            } else {
                val session = googleSession
                val origin =
                    runCatching {
                        googleLoginOrigin(
                            connection
                                .request()
                                .url
                                .toString()
                                .replaceFirst("https://", "wss://"),
                        )
                    }.getOrNull()
                if (session == null || origin == null || !session.validFor(origin)) {
                    authenticationUnavailable("Sign-in expired; choose Connect to sign in again")
                } else {
                    val msg =
                        AuthenticateMsg(
                            callsign = callsign,
                            authMethod = "google_session",
                            sessionCredential = session.credential,
                        )
                    send(protocolJson.encodeToString(AuthenticateMsg.serializer(), msg))
                }
            }
        }
    }

    private fun revokeGoogleSession(session: GoogleServerSession) {
        scope.launch {
            try {
                GoogleLoginClient(session.origin.replaceFirst("https://", "wss://") + "/ws").revoke(session)
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                // Local credential is already discarded; server expiry bounds failed logout
            }
        }
    }

    private fun clearGoogleSignIn() {
        googleSignInJob?.cancel()
        googleSignInJob = null
        googleSession?.let { revokeGoogleSession(it) }
        googleSession = null
    }

    private fun authenticationUnavailable(message: String) {
        connectionGeneration++
        clearGoogleSignIn()
        clearInstallationBudget()
        manualDisconnect = true
        lastLobbyId = null
        reconnectJob?.cancel()
        retireNetworkWork()
        webSocket?.cancel()
        webSocket = null
        state.update {
            it.copy(
                status = ConnectionStatus.DISCONNECTED,
                errorMessage = message,
                playerId = null,
                sessionToken = null,
                currentLobby = null,
                gameLaunchInfo = null,
                nav = MultiplayerNav.BROWSER,
            )
        }
    }

    private fun sendInstallationAuthenticate(callsign: String) {
        val connection = webSocket ?: return
        check(allowsInstallationAuth(BuildConfig.DEBUG, connection.request().url.host))
        val context = activityRef?.get()?.applicationContext
        if (context == null) {
            authenticationUnavailable("Installation identity storage is unavailable")
            return
        }
        val url = connection.request().url
        val host = if (url.host.contains(':')) "[${url.host}]" else url.host
        val audience = "${url.scheme}://$host:${url.port}"
        installationAuthJob?.cancel()
        installationAuthJob =
            scope.launch(Dispatchers.IO) {
                try {
                    val producer = OnlineInstallationAuth.fromStorage(context.noBackupFilesDir, callsign, audience)
                    withConnection(connection) {
                        installationAuth = producer
                        send(protocolJson.encodeToString(AuthenticateMsg.serializer(), producer.request()))
                    }
                } catch (e: kotlinx.coroutines.CancellationException) {
                    throw e
                } catch (_: Exception) {
                    withConnection(connection) { authenticationUnavailable("Could not open installation identity") }
                }
            }
    }

    private var lobbyListingId: String? = null
    private var lobbyPageRequest: ListLobbiesMsg? = null
    private var lobbyPageJob: Job? = null

    @Synchronized
    fun requestLobbyList() {
        lobbyPageJob?.cancel()
        lobbyListingId = null
        val request = ListLobbiesMsg()
        lobbyPageRequest = request
        send(protocolJson.encodeToString(ListLobbiesMsg.serializer(), request))
    }

    private fun scheduleLobbyPage(delayMs: Long) {
        val request = lobbyPageRequest ?: return
        val connection = webSocket ?: return
        lobbyPageJob?.cancel()
        lobbyPageJob =
            scope.launch {
                delay(delayMs)
                withConnection(connection) {
                    if (lobbyPageRequest ==
                        request
                    ) {
                        send(protocolJson.encodeToString(ListLobbiesMsg.serializer(), request))
                    }
                }
            }
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

    @Synchronized
    fun leaveLobby() {
        lastLobbyId = null
        send(protocolJson.encodeToString(LeaveLobbyMsg.serializer(), LeaveLobbyMsg()))
        retireNetworkWork()
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

    fun reportMissionStatus(report: MissionStatusReport) {
        val msg =
            MissionStatusMsg(
                revision = report.revision,
                status = report.status,
                verifiedBytes = report.verifiedBytes,
                totalBytes = report.totalBytes,
                transferId = report.transferId,
                attempt = report.attempt,
                failureCode = report.failureCode,
                gameDataWarning = report.gameDataWarning,
            )
        send(protocolJson.encodeToString(MissionStatusMsg.serializer(), msg))
    }

    fun startGame(filesDir: java.io.File) {
        val gameInfo =
            state.state.value.currentLobby
                ?.gameInfo
        if (gameInfo?.get("mode")?.jsonPrimitive?.content == "coop") {
            val warning =
                CoopSaveCompatibility.hostWarning(
                    filesDir,
                    gameInfo["game"]?.jsonPrimitive?.content ?: "d2",
                    gameInfo["mission"]?.jsonPrimitive?.content,
                )
            if (warning != null) {
                state.appendLog(warning)
                NetLog.log("LOBBY", "Start blocked: $warning")
                return
            }
        }
        send(protocolJson.encodeToString(StartGameMsg.serializer(), StartGameMsg()))
        state.appendLog("Requesting game start...")
        NetLog.log("LOBBY", "Requesting game start")
    }

    fun updateGameInfo(gameInfo: JsonObject) {
        pendingGameInfo = gameInfo
        send(protocolJson.encodeToString(UpdateGameInfoMsg.serializer(), UpdateGameInfoMsg(gameInfo = gameInfo)))
        NetLog.log("LOBBY", "Updating game info")
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

    /** Accept runtime game state from the bound host game process. */
    fun startGameStateUpdates() {
        gameStateUpdatesEnabled = true
        NetLog.log("GAME", "Ready for game-process state updates")
    }

    /** Stop accepting runtime state from the game process. */
    fun stopGameStateUpdates() {
        gameStateUpdatesEnabled = false
    }

    fun publishRuntimeGameState(arr: IntArray) {
        if (!gameStateUpdatesEnabled) return
        RuntimeGameStateCodec.encode(arr)?.let(::send)
    }

    @Synchronized
    fun runtimeGameProcessDisconnected(
        attempt: String,
        wasHost: Boolean,
    ) {
        if (!ownsRuntimeAttempt(attempt)) return
        runtimeAttempt = null
        if (wasHost) {
            retireNetworkWork()
            endGame()
        } else {
            leaveLobby()
        }
    }

    fun sendMessage(
        targetPlayerId: String,
        text: String,
    ) {
        val msg = SendMessageMsg(targetPlayerId = targetPlayerId, text = text)
        send(protocolJson.encodeToString(SendMessageMsg.serializer(), msg))
    }

    private fun sendStunResult(
        attemptId: String,
        receipts: List<String>,
    ) {
        val msg = StunResultMsg(attemptId = attemptId, receipts = receipts)
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
        checkId: String,
        remoteAddr: String = "",
    ) {
        connectivityResults = connectivityResults + (
            peerId to
                mapOf(
                    "room_id" to (
                        state.state.value.currentLobby
                            ?.lobbyId ?: ""
                    ),
                    "network_epoch" to networkGeneration,
                    "check_epoch" to connectivityCheckEpoch,
                    "peer_id" to peerId,
                    "candidate_type" to winningCandidateType,
                    "rtt_ms" to rttMs,
                    "check_id" to checkId,
                    "remote_addr" to remoteAddr,
                )
        )
        val msg =
            ConnectivityOkMsg(
                checkId = checkId,
                peerId = peerId,
                winningCandidateType = winningCandidateType,
                rttMs = rttMs,
            )
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
        val generation = connectionGeneration
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
                synchronized(this@MatchmakingService) {
                    if (generation != connectionGeneration || manualDisconnect) return@synchronized
                    reconnectAttempt++
                    val s = state.state.value
                    openConnection(s.serverUrl, s.callsign)
                }
            }
    }

    @Synchronized
    private fun withConnection(
        connection: WebSocket,
        action: () -> Unit,
    ) {
        if (webSocket === connection && !manualDisconnect) action()
    }

    @Synchronized
    private fun withNetwork(
        generation: Long,
        action: () -> Unit,
    ) {
        if (generation == networkGeneration) action()
    }

    @Synchronized
    private fun retireNetworkWork() {
        networkGeneration++
        discoveredCandidates = emptyList()
        connectivityResults = emptyMap()
        probeDiagnostics = emptyMap()
        runtimeAttempt = null
        gameEpoch = null
        pendingAdmissions.clear()
        installationAuthJob?.cancel()
        installationAuthJob = null
        installationAuth = null
        gameHandoffJob?.cancel()
        gameHandoffJob = null
        stunJob?.cancel()
        stunJob = null
        pendingCandidateChallenge?.reply?.cancel()
        pendingCandidateChallenge = null
        stunCompleted = false
        connectivityCheckJob?.cancel()
        connectivityCheckJob = null
        closeCandidateSocket()
        stopGameStateUpdates()
        cleanupUpnp()
        shutdownProxy()
    }

    /** Aggregate Android test diagnostics; never includes keys, tokens or endpoint addresses */
    @Synchronized
    fun resourceDiagnostics(): Map<String, Any> =
        mapOf(
            "installation_work" to installationWork,
            "connectivity" to
                mapOf(
                    "network_epoch" to networkGeneration,
                    "check_epoch" to connectivityCheckEpoch,
                    "phase" to state.state.value.iceStatus.phase.name,
                    "candidate_count" to state.state.value.iceStatus.stunCandidateCount,
                    "peer_candidate_count" to state.state.value.iceStatus.peerCandidatesReceived,
                    "results" to connectivityResults.values.toList(),
                    "probe_counts" to probeDiagnostics,
                    "local_candidates" to
                        discoveredCandidates.map { mapOf("type" to it.candidateType, "addr" to it.addr) },
                    "pairs" to
                        state.state.value.connectivityPairs.map {
                            mapOf(
                                "type" to it.remoteType,
                                "addr" to it.remoteAddr,
                            )
                        },
                ),
            "runtime_attempt_epoch" to if (runtimeAttempt == null) 0L else runtimeAttemptEpoch,
            "proxy_workers" to LocalhostProxy.activeWorkerCount(),
            "candidate_socket" to (candidateSocket?.isClosed == false),
            "upnp_mapping" to (upnpMapping != null),
            "background_jobs" to (scope.coroutineContext[Job]?.children?.count { !it.isCompleted } ?: 0),
            "game_state_updates" to gameStateUpdatesEnabled,
            "peers" to (
                localhostProxy?.getStats()?.map { peer ->
                    mapOf(
                        "slot" to peer.peerSlot,
                        "sent" to peer.packetsSent,
                        "received" to peer.packetsReceived,
                        "bound" to peer.relayBound,
                        "route_kind" to if (peer.isRelay) "relay" else "direct",
                        "route_epoch" to runtimeAttemptEpoch,
                        "gameplay_received" to peer.gameplayReceived,
                    )
                } ?: emptyList()
            ),
        )

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

    @Synchronized
    private fun launchStunDiscovery() {
        if (gameHandoffJob?.isActive == true) return
        val connection = webSocket ?: return
        val lobby = state.state.value.currentLobby ?: return
        val admission =
            lobby.players.firstOrNull { it.playerId == state.state.value.playerId }?.candidateAdmissionId ?: return
        stunJob?.cancel()
        closeCandidateSocket()
        connectivityResults = emptyMap()
        probeDiagnostics = emptyMap()
        discoveredCandidates = emptyList()
        val generation = ++networkGeneration
        val socket = DatagramSocket()
        candidateSocket = socket
        val addrs = stunOverrideAddrs ?: state.state.value.stunAddrs
        stunJob =
            scope.launch(start = CoroutineStart.LAZY) {
                var issued: CandidateChallengeMsg? = null
                withNetwork(generation) {
                    state.update { it.copy(iceStatus = IceStatus(phase = IcePhase.STUN_DISCOVERY)) }
                }
                try {
                    // Map first so the short-lived proof observes the actual mapped port
                    val work = currentCoroutineContext()[Job]!!
                    val mapping =
                        withContext(Dispatchers.IO) {
                            val mapped = tryUpnpMapping(socket.localPort)
                            val accepted =
                                synchronized(this@MatchmakingService) {
                                    if (generation == networkGeneration && work.isActive) {
                                        upnpMapping = mapped
                                        true
                                    } else {
                                        false
                                    }
                                }
                            if (!accepted && mapped != null) UpnpClient.removeMapping(mapped)
                            mapped
                        }
                    currentCoroutineContext().ensureActive()
                    val requestId = UUID.randomUUID().toString()
                    val reply = CompletableDeferred<CandidateChallengeMsg>()
                    withNetwork(generation) {
                        pendingCandidateChallenge =
                            PendingCandidateChallenge(
                                connection,
                                generation,
                                lobby.lobbyId,
                                admission,
                                requestId,
                                reply,
                            )
                        val request =
                            GatherCandidatesMsg(lobbyId = lobby.lobbyId, admissionId = admission, requestId = requestId)
                        connection.send(protocolJson.encodeToString(GatherCandidatesMsg.serializer(), request))
                    }
                    val challenge =
                        withTimeoutOrNull(5000) { reply.await() } ?: error("Candidate challenge unavailable")
                    issued = challenge
                    val observations =
                        withContext(Dispatchers.IO) {
                            val servers =
                                addrs.take(2).map { address ->
                                    val parts = address.split(':')
                                    require(parts.size == 2)
                                    InetSocketAddress(parts[0], parts[1].toInt())
                                }
                            StunProofClient.discover(
                                servers,
                                UUID.fromString(challenge.credential),
                                socket,
                            ) { work.ensureActive() }
                        }
                    currentCoroutineContext().ensureActive()
                    val candidates =
                        observations
                            .map {
                                ConnectionCandidate("srflx", "${it.endpoint.address.hostAddress}:${it.endpoint.port}")
                            }.distinctBy { it.addr }
                    val nat =
                        when {
                            observations.size < 2 -> "unknown"
                            observations[0].endpoint == observations[1].endpoint -> "full_cone"
                            else -> "symmetric"
                        }
                    withNetwork(generation) {
                        discoveredCandidates = candidates
                        stunCompleted = true
                        state.update { s ->
                            s.copy(
                                iceStatus =
                                    s.iceStatus.copy(
                                        phase = IcePhase.STUN_COMPLETE,
                                        stunNatType = nat,
                                        stunCandidateCount = candidates.size,
                                        upnpMapped = mapping != null,
                                        upnpAddr = mapping?.let { "${it.externalIp}:${it.externalPort}" },
                                    ),
                            )
                        }
                        // Addresses are diagnostic only; the server constructs destinations from receipts
                        sendStunResult(challenge.attemptId, observations.map { it.receipt.toString() })
                    }
                } catch (e: CancellationException) {
                    throw e
                } catch (_: Exception) {
                    withNetwork(generation) {
                        // Never log exceptions that could contain a malformed challenge credential
                        Log.e(TAG, "Candidate discovery unavailable")
                        state.appendLog("Candidate discovery unavailable; using relay")
                        state.update { s ->
                            s.copy(
                                iceStatus =
                                    s.iceStatus.copy(
                                        phase = IcePhase.FAILED,
                                        errorMessage = "Candidate discovery unavailable",
                                    ),
                            )
                        }
                        stunCompleted = true
                        issued?.let { sendStunResult(it.attemptId, emptyList()) }
                    }
                } finally {
                    withNetwork(generation) {
                        pendingCandidateChallenge?.reply?.cancel()
                        pendingCandidateChallenge = null
                        stunJob = null
                    }
                }
            }
        stunJob?.start()
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
            UpnpClient.tryMap(port, localIp)
        } catch (e: Exception) {
            Log.w(TAG, "UPnP attempt failed: ${e.message}")
            null
        }
    }

    @Synchronized
    private fun launchConnectivityCheck(pairs: List<CandidatePair>) {
        if (gameHandoffJob?.isActive == true) return
        // C11: cancel previous check to avoid duplicates
        connectivityCheckJob?.cancel()
        connectivityCheckEpoch++
        connectivityResults = emptyMap()
        probeDiagnostics = emptyMap()
        val generation = networkGeneration
        val socket = candidateSocket
        connectivityCheckJob =
            scope.launch(start = CoroutineStart.LAZY) {
                val job = currentCoroutineContext()[Job]!!
                try {
                    val result =
                        probeMutex.withLock {
                            job.ensureActive()
                            ConnectivityChecker.probe(
                                pairs,
                                existingSocket = socket,
                                checkActive = { job.ensureActive() },
                                diagnostics = { counts ->
                                    withNetwork(generation) {
                                        if (job.isActive) probeDiagnostics = counts
                                    }
                                },
                            )
                        }
                    withNetwork(generation) {
                        job.ensureActive()
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
                            sendConnectivityOk(
                                result.peerId,
                                result.winningCandidateType,
                                result.rttMs,
                                result.checkId,
                                result.remoteAddr,
                            )
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
                                sendConnectivityOk(peerId, "relay", 0, pairs.first { it.peerId == peerId }.checkId)
                            }
                        }
                    }
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    withNetwork(generation) {
                        job.ensureActive()
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
                            sendConnectivityOk(peerId, "relay", 0, pairs.first { it.peerId == peerId }.checkId)
                        }
                    }
                }
            }
        connectivityCheckJob?.start()
    }

    private class Listener(
        private val callsign: String,
    ) : WebSocketListener() {
        override fun onOpen(
            webSocket: WebSocket,
            response: Response,
        ) {
            withConnection(webSocket) {
                Log.i(TAG, "WebSocket open")
                state.update { it.copy(status = ConnectionStatus.AUTHENTICATING) }
                state.appendLog("Connected. Authenticating as '$callsign'...")
                NetLog.log("CONNECT", "WebSocket open")
                sendAuthenticate(callsign)
            }
        }

        override fun onMessage(
            webSocket: WebSocket,
            text: String,
        ) {
            withConnection(webSocket) {
                handleMessage(text)
            }
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
            withConnection(webSocket) {
                Log.i(TAG, "WebSocket closed: $code $reason")
                this@MatchmakingService.webSocket = null
                // C9: clean up proxy on connection loss
                retireNetworkWork()
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
        }

        override fun onFailure(
            webSocket: WebSocket,
            t: Throwable,
            response: Response?,
        ) {
            withConnection(webSocket) {
                Log.e(TAG, "WebSocket failure: ${t.message}", t)
                this@MatchmakingService.webSocket = null
                // C9: clean up proxy on connection loss
                retireNetworkWork()
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
    }

    @Synchronized
    private fun queueGameStart(gs: GameStartingMsg) {
        gameHandoffJob?.cancel()
        val workers = listOfNotNull(stunJob, connectivityCheckJob)
        workers.forEach { it.cancel() }
        stunCompleted = true
        val generation = networkGeneration
        val connection = webSocket
        gameHandoffJob =
            scope.launch(start = CoroutineStart.LAZY) {
                val handoff = currentCoroutineContext()[Job]!!
                try {
                    // No receiver may use the candidate socket while the proxy takes ownership
                    workers.forEach { it.join() }
                    probeMutex.withLock {
                        synchronized(this@MatchmakingService) {
                            if (generation != networkGeneration || connection !== webSocket ||
                                gameHandoffJob !== handoff || !handoff.isActive
                            ) {
                                return@synchronized
                            }
                            finishGameStart(gs)
                        }
                    }
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    withNetwork(generation) {
                        state.update {
                            it.copy(
                                errorMessage = "Could not prepare game networking: ${e.message}",
                                gameLaunchInfo = null,
                            )
                        }
                        if (gs.yourSlot == 0) endGame() else leaveLobby()
                        Log.e(TAG, "Game network handoff failed", e)
                    }
                } finally {
                    synchronized(this@MatchmakingService) {
                        if (gameHandoffJob === handoff) gameHandoffJob = null
                    }
                }
            }
        gameHandoffJob?.start()
    }

    private fun finishGameStart(gs: GameStartingMsg) {
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
        shutdownProxy()
        val proxy = newProxy(sharedSocket)
        try {
            for (peer in gs.peers) {
                val addrParts = reachableServerEndpoint(peer.addr).split(":")
                if (addrParts.size != 2) {
                    state.appendLog("Bad peer addr: ${peer.addr}")
                    NetLog.log("ERROR", "Bad peer addr: ${peer.addr}")
                    error("Invalid peer address: ${peer.addr}")
                }
                val addr = InetSocketAddress(addrParts[0], addrParts[1].toInt())
                proxy.addPeer(
                    PeerProxyConfig(
                        peerSlot = peer.slot,
                        localPort = NetworkConstants.PROXY_PORT_BASE + peer.slot,
                        realAddr = addr,
                        isRelay = peer.isRelay,
                        relayToken = peer.relayToken?.toUInt() ?: 0u,
                        relayDestSlot = peer.relayDestSlot ?: peer.slot,
                        relayOwnSlot = peer.relayCredentials?.ownSlot ?: -1,
                        relaySendKey = peer.relayCredentials?.sendKey,
                        relayReceiveKey = peer.relayCredentials?.receiveKey,
                        relayEpoch = peer.relayCredentials?.allocationEpoch,
                    ),
                )
            }
            localhostProxy = proxy
            proxy.start()
        } catch (e: Exception) {
            proxy.shutdown()
            if (localhostProxy === proxy) localhostProxy = null
            state.update {
                it.copy(
                    errorMessage = "Could not prepare game networking: ${e.message}",
                    gameLaunchInfo = null,
                )
            }
            if (gs.yourSlot == 0) endGame() else leaveLobby()
            throw e
        }

        // Determine if we're the host (slot 0 = host)
        val isHost = gs.yourSlot == 0
        runtimeAttemptEpoch++
        runtimeAttempt =
            java.util.UUID
                .randomUUID()
                .toString()
        gameEpoch = gs.gameEpoch
        pendingAdmissions.clear()
        val launchInfo =
            GameLaunchInfo(
                runtimeAttempt = runtimeAttempt,
                game = gs.game,
                mission = gs.gameInfo["mission"]?.jsonPrimitive?.content ?: "",
                mode = gs.gameInfo["mode"]?.jsonPrimitive?.content ?: "",
                difficulty = gs.gameInfo["difficulty"]?.jsonPrimitive?.intOrNull ?: 1,
                levelNum = gs.currentLevel ?: gs.gameInfo["level_num"]?.jsonPrimitive?.intOrNull ?: 1,
                maxPlayers = gs.maxPlayers,
                startPlayers = gs.peers.size + 1,
                yourSlot = gs.yourSlot,
                isHost = isHost,
                peers = gs.peers,
                coopQol =
                    gs.gameInfo["coop_qol"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: true,
                duplicateEnergyShields =
                    gs.gameInfo["duplicate_energy_shields"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: false,
                fullDeathSpew =
                    gs.gameInfo["full_death_spew"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: true,
                coopBriefings =
                    gs.gameInfo["coop_briefings"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: false,
                allowSecretWarps =
                    gs.gameInfo["allow_secret_warps"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: false,
                playerSpewNoExpire =
                    gs.gameInfo["player_spew_no_expire"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: true,
                clientsCanRequestRewind =
                    gs.gameInfo["clients_can_request_rewind"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: false,
                restrictNonCoopFovToBase =
                    gs.gameInfo["restrict_noncoop_fov_to_base"]
                        ?.jsonPrimitive
                        ?.content
                        ?.toBooleanStrictOrNull() ?: false,
                missionRequirement = missionRequirementFromGameInfo(gs.gameInfo),
            )
        state.update { it.copy(gameLaunchInfo = launchInfo) }
        // Host sends periodic game state updates to the matchmaking server
        if (isHost) {
            startGameStateUpdates()
        }
    }

    private fun reachableServerEndpoint(endpoint: String): String =
        localDevelopmentEndpoint(BuildConfig.DEBUG, webSocket?.request()?.url?.host, endpoint)

    private fun currentRekey(scope: RelayRekeyScope): Boolean {
        val current = state.state.value
        val lobby = current.currentLobby ?: return false
        return scope.lobbyId == lobby.lobbyId && scope.gameEpoch == gameEpoch &&
            scope.oldToken in 0..0xffff_ffffL && scope.newToken in 0..0xffff_ffffL &&
            scope.oldToken != scope.newToken && scope.oldEpoch != scope.newEpoch && scope.peerSlot in 0..7 &&
            lobby.players.any { it.slot == scope.peerSlot && it.playerId == scope.peerId }
    }

    private fun handleRelayRekey(
        type: String,
        message: RelayRekeyMsg,
    ) {
        val allocation = message.scope()
        if (!currentRekey(allocation)) return
        val connection = webSocket ?: return
        val proxy = localhostProxy ?: return
        val generation = networkGeneration
        when (type) {
            "RELAY_REKEY_OFFER" -> {
                proxy.stageRelayRekey(message) {
                    synchronized(this) {
                        if (generation == networkGeneration && connection === webSocket && proxy === localhostProxy &&
                            currentRekey(allocation)
                        ) {
                            val ready =
                                RelayRekeyReadyMsg(
                                    allocation.lobbyId,
                                    allocation.gameEpoch,
                                    allocation.oldToken,
                                    allocation.newToken,
                                    allocation.newEpoch,
                                )
                            send(protocolJson.encodeToString(RelayRekeyReadyMsg.serializer(), ready))
                        }
                    }
                }
            }

            "RELAY_REKEY_COMMIT" -> {
                proxy.commitRelayRekey(allocation, message.retireAfterMs)
            }

            "RELAY_REKEY_ABORT" -> {
                proxy.abortRelayRekey(allocation)
            }
        }
    }

    private fun handleMessage(text: String) {
        try {
            when (val msg = MatchmakingDiagnostics.parse(text) ?: return) {
                is ServerMessage.AuthOkMsg -> {
                    // The timer may be delayed; acceptance must still obey the monotonic budget
                    if (!installationTimeRemaining()) return
                    // An accepted socket is not a recovered authenticated session
                    reconnectAttempt = 0
                    clearInstallationBudget()
                    lobbyPageJob?.cancel()
                    lobbyListingId = null
                    lobbyPageRequest = ListLobbiesMsg()
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
                    authenticationUnavailable("Auth failed: ${msg.data.reason}")
                }

                is ServerMessage.InstallationChallengeMsg -> {
                    val connection = webSocket ?: return
                    if (!allowsInstallationAuth(BuildConfig.DEBUG, connection.request().url.host)) {
                        authenticationUnavailable("Google account authentication is required for online multiplayer")
                        return
                    }
                    val producer = installationAuth
                    if (producer == null) {
                        authenticationUnavailable("Unexpected installation challenge")
                        return
                    }
                    if (!beginInstallationWork()) return
                    installationAuth = null
                    installationAuthJob?.cancel()
                    installationAuthJob =
                        scope.launch(Dispatchers.Default) {
                            try {
                                val proof =
                                    producer.proof(msg.data.challenge, msg.data.difficulty) { work ->
                                        withConnection(connection) {
                                            installationWork =
                                                mapOf(
                                                    "attempt" to installationAttempts,
                                                    "difficulty" to work.difficulty,
                                                    "hashes" to work.hashes,
                                                    "elapsed_ms" to work.elapsedMs,
                                                    "solved" to work.solved,
                                                )
                                        }
                                    }
                                withConnection(
                                    connection,
                                ) {
                                    if (installationTimeRemaining()) {
                                        send(protocolJson.encodeToString(InstallationProofMsg.serializer(), proof))
                                    }
                                }
                            } catch (e: kotlinx.coroutines.CancellationException) {
                                throw e
                            } catch (e: Exception) {
                                withConnection(
                                    connection,
                                ) {
                                    val code = OnlineInstallationAuth.failureCode(e)
                                    if (e is OnlineInstallationAuth.WorkDeadlineExceeded) {
                                        retryInstallationWork()
                                    } else {
                                        authenticationUnavailable(
                                            "Installation authentication could not complete ($code)",
                                        )
                                    }
                                }
                            }
                        }
                }

                is ServerMessage.PowChallengeMsg -> {
                    authenticationUnavailable("This client does not support the server authentication challenge")
                }

                is ServerMessage.ErrorMsg -> {
                    state.update { it.copy(errorMessage = "${msg.data.code}: ${msg.data.message}") }
                    state.appendLog("Server error: ${msg.data.code} - ${msg.data.message}")
                    NetLog.log("ERROR", "Server: ${msg.data.code} - ${msg.data.message}")
                    // Kicked from lobby -- return to browser
                    if (msg.data.code == "HOST_DISCONNECTED" ||
                        msg.data.code.contains("KICKED", ignoreCase = true) ||
                        msg.data.code.contains("LOBBY", ignoreCase = true)
                    ) {
                        lastLobbyId = null
                        retireNetworkWork()
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
                    val page = msg.data
                    if (page.reset) lobbyListingId = page.listingId
                    if (page.listingId != lobbyListingId) return
                    state.update {
                        it.copy(
                            lobbies =
                                (if (page.reset) page.lobbies else it.lobbies + page.lobbies).distinctBy { lobby ->
                                    lobby.lobbyId
                                },
                        )
                    }
                    lobbyPageRequest = page.nextCursor?.let { ListLobbiesMsg(cursor = it, listingId = page.listingId) }
                    if (lobbyPageRequest != null) {
                        scheduleLobbyPage(75)
                    } else {
                        state.appendLog("Lobby list: ${state.state.value.lobbies.size} lobbies")
                    }
                }

                is ServerMessage.CandidateChallengeReceived -> {
                    val pending = pendingCandidateChallenge
                    val challenge = msg.data
                    if (pending != null &&
                        pending.connection === webSocket &&
                        pending.generation == networkGeneration &&
                        challenge.lobbyId == pending.lobbyId && challenge.admissionId == pending.admissionId &&
                        challenge.requestId == pending.requestId
                    ) {
                        pending.reply.complete(challenge)
                    }
                }

                is ServerMessage.LobbyUpdated -> {
                    val update = msg.data
                    val myId = state.state.value.playerId
                    val previousRoom =
                        state.state.value.currentLobby
                            ?.lobbyId
                    if (previousRoom != null && previousRoom != update.lobbyId) retireNetworkWork()
                    val previousAdmission =
                        state.state.value.currentLobby
                            ?.players
                            ?.firstOrNull {
                                it.playerId == myId
                            }?.candidateAdmissionId
                    val nextAdmission = update.players.firstOrNull { it.playerId == myId }?.candidateAdmissionId
                    if (previousAdmission != null && previousAdmission != nextAdmission) retireNetworkWork()
                    localhostProxy?.reconcileRoster(
                        update.players.filter { it.playerId != myId }.associate {
                            it.slot to
                                it.playerId
                        },
                    )
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
                    queueGameStart(msg.data)
                }

                is ServerMessage.RateLimited -> {
                    scheduleLobbyPage(msg.data.retryAfterMs.coerceIn(1000, 10000))
                    state.appendLog("Rate limited, retry in ${msg.data.retryAfterMs}ms")
                    NetLog.log("SERVER", "Rate limited, retry in ${msg.data.retryAfterMs}ms")
                }

                is ServerMessage.VersionRejected -> {
                    state.update {
                        it.copy(errorMessage = "Version rejected: ${msg.data.reason}")
                    }
                    state.appendLog("Version rejected: ${msg.data.reason}")
                    NetLog.log("ERROR", "Version rejected: ${msg.data.reason}")
                    authenticationUnavailable("Version rejected: ${msg.data.reason}")
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
                    val pairs = msg.data.peerAddrs.map { it.copy(remoteAddr = reachableServerEndpoint(it.remoteAddr)) }
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
                    NetLog.log("RELAY", "Relay assigned")
                }

                is ServerMessage.RelayRekeyReceived -> {
                    try {
                        handleRelayRekey(msg.type, msg.data)
                    } catch (_: Exception) {
                        NetLog.log("ERROR", "Invalid relay renewal")
                    }
                }

                is ServerMessage.LateJoinProbeReceived -> {
                    val probe = msg.data
                    if (state.state.value.currentLobby?.players?.none {
                            it.playerId == probe.joinerId &&
                                it.candidateAdmissionId == probe.admissionId
                        } !=
                        false
                    ) {
                        return
                    }
                    state.appendLog("Late-join probe: ${probe.joinerCallsign} (${probe.probeAddrs.size} addrs)")
                    NetLog.log("LATEJOIN", "Probe for ${probe.joinerCallsign}: ${probe.probeAddrs.size} addrs")
                    // Send blind probes from shared socket and enable probe echo
                    localhostProxy?.sendLateJoinProbes(probe.joinerId, probe.probeAddrs.map(::reachableServerEndpoint))
                }

                is ServerMessage.LateJoinApprovedReceived -> {
                    val peer = msg.data.peer
                    val admission =
                        msg.data.admissionId.takeIf {
                            state.state.value.currentLobby
                                ?.isHost == true
                        }
                    if (admission != null) pendingAdmissions[peer.slot] = admission
                    state.appendLog("Late-join approved: slot ${peer.slot} -> ${peer.addr}")
                    NetLog.log("LATEJOIN", "Approved: slot=${peer.slot} addr=${peer.addr} relay=${peer.isRelay}")
                    // Add the new peer to the existing proxy
                    val addrParts = reachableServerEndpoint(peer.addr).split(":")
                    if (addrParts.size == 2) {
                        val port = addrParts[1].toIntOrNull()
                        if (port != null) {
                            val addr = InetSocketAddress(addrParts[0], port)
                            localhostProxy?.addPeer(
                                PeerProxyConfig(
                                    peerSlot = peer.slot,
                                    admissionId = admission,
                                    localPort = NetworkConstants.PROXY_PORT_BASE + peer.slot,
                                    realAddr = addr,
                                    isRelay = peer.isRelay,
                                    relayToken = peer.relayToken?.toUInt() ?: 0u,
                                    relayDestSlot = peer.relayDestSlot ?: peer.slot,
                                    relayOwnSlot = peer.relayCredentials?.ownSlot ?: -1,
                                    relaySendKey = peer.relayCredentials?.sendKey,
                                    relayReceiveKey = peer.relayCredentials?.receiveKey,
                                    relayEpoch = peer.relayCredentials?.allocationEpoch,
                                ),
                            )
                        }
                    }
                }

                is ServerMessage.Unknown -> {
                    state.appendLog("Unknown message type: ${msg.type}")
                    NetLog.log("CONNECT", "Unknown matchmaking message type")
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
                        // The server already joined this room and follows with LOBBY_UPDATE
                        lastLobbyId = null
                    } else {
                        state.appendLog("Cannot join friend's game: ${resp.reason ?: "unknown"}")
                        NetLog.log("ERROR", "Cannot join friend's game: ${resp.reason ?: "unknown"}")
                        state.update { it.copy(errorMessage = resp.reason) }
                    }
                }
            }
        } catch (_: Exception) {
            Log.e(TAG, "Matchmaking message handling failed")
            state.appendLog("Matchmaking message handling failed")
            NetLog.log("ERROR", "Matchmaking message handling failed")
        }
    }
}
