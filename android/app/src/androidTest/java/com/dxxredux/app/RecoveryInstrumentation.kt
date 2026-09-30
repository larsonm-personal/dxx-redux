package com.dxxredux.app

import android.app.Activity
import android.app.Instrumentation
import android.os.Bundle
import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.LobbyService
import com.dxxredux.app.multiplayer.ConnectionStatus
import com.dxxredux.app.multiplayer.LocalhostProxy
import com.dxxredux.app.multiplayer.MatchmakingService
import com.dxxredux.app.multiplayer.MatchmakingStateHolder
import com.dxxredux.app.multiplayer.MissionCompatibilityStatus
import com.dxxredux.app.multiplayer.MissionRequirement
import com.dxxredux.app.multiplayer.MissionStatusReport
import com.dxxredux.app.multiplayer.MissionTransferGrant
import com.dxxredux.app.multiplayer.MissionTransferService
import com.dxxredux.app.multiplayer.PeerProxyConfig
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import okhttp3.Protocol
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/** Device tests using real sockets and app storage, without additional test dependencies */
class RecoveryInstrumentation : Instrumentation() {
    private var engineProbeHost: String? = null
    private var engineProbeGame = "d2"
    private var missionLoadingOnly = false
    private var coopSessionOnly = false
    private var discoveryOnly = false
    private var lobbyLatencyOnly = false

    override fun onCreate(arguments: Bundle?) {
        if (arguments?.getString("suite") == "engine_query") {
            engineProbeHost = arguments.getString("host")
            engineProbeGame = arguments.getString("game") ?: "d2"
        }
        lobbyLatencyOnly = arguments?.getString("suite") == "lobby_latency"
        discoveryOnly = arguments?.getString("suite") == "discovery"
        missionLoadingOnly = arguments?.getString("suite") == "mission_loading"
        coopSessionOnly = arguments?.getString("suite") == "coop_session"
        super.onCreate(arguments)
        start()
    }

    override fun onStart() {
        val result = Bundle()
        try {
            engineProbeHost?.let { host ->
                EngineQueryChecks(this).run(host, engineProbeGame)
                result.putString("stream", "PASS: engine query, loss, malformed replies, sender checks, lobby preference, engine fallback, mismatch and cancellation\n")
                finish(Activity.RESULT_OK, result)
                return
            }
            if (lobbyLatencyOnly) {
                LobbyLatencyChecks(this).run()
                result.putString("stream", "PASS: responsive discovery during save checks, concurrent launch preparation, commit/abort/retry and manual IP recovery\n")
                finish(Activity.RESULT_OK, result)
                return
            }
            if (discoveryOnly) {
                DiscoveryExperimentChecks(this).run()
                result.putString("stream", "PASS: discovery phases, UDP replies, socket replacement, query correlation and cancellation\n")
                finish(Activity.RESULT_OK, result)
                return
            }
            if (coopSessionOnly) {
                CoopSessionChecks(this).run()
                result.putString("stream", "PASS: migrated lobby adoption and former-host rejoin\n")
                finish(Activity.RESULT_OK, result)
                return
            }
            if (missionLoadingOnly) {
                MissionLoadingChecks(this).run()
                result.putString("stream", "PASS: mission dialog loading, cancellation, retry and catalog reuse\n")
                finish(Activity.RESULT_OK, result)
                return
            }
            missionTransferCancellation()
            missionTransferPhaseCancellation(MissionCompatibilityStatus.VERIFYING)
            missionTransferPhaseCancellation(MissionCompatibilityStatus.FINALIZING)
            obsoleteWebSocketCallbacks()
            proxyFailureAndRetry()
            obsoleteMissionGrants()
            hostLaunchRollback()
            probeHandoffWaitsForReader()
            launcherActivityFailureAndRetry()
            result.putString(
                "stream",
                "PASS: transfer cancellation/resume, obsolete WebSocket callbacks, proxy failure/retry, host launch rollback, probe handoff, launcher retry\n",
            )
            finish(Activity.RESULT_OK, result)
        } catch (failure: Throwable) {
            result.putString("stream", "FAIL: ${failure.stackTraceToString()}\n")
            finish(Activity.RESULT_CANCELED, result)
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun obsoleteMissionGrants() {
        fun field(name: String) = LobbyService::class.java.getDeclaredField(name).apply { isAccessible = true }
        LobbyService.stopDiscovery()
        val joined = field("_joinedLobby").get(null) as MutableStateFlow<LobbyService.JoinedLobbyInfo?>
        val previousContext = field("appContext").get(null)
        field("appContext").set(null, null)
        val requirement =
            MissionRequirement(
                revision = "grant-test",
                game = "d2",
                missionKey = "recovery-fixture",
                displayName = "Recovery fixture",
                kind = MissionRequirement.KIND_WRAPPER,
                wrapperFilename = "recovery-fixture.zip",
                sizeBytes = 1,
                sha256 = "a".repeat(64),
                offerAvailable = true,
            )
        joined.value =
            LobbyService.JoinedLobbyInfo(
                "grant-lobby",
                "192.0.2.1",
                "d2",
                "fixture",
                "coop",
                4,
                missionRequirement = requirement,
                missionStatus =
                    MissionStatusReport(
                        requirement.revision,
                        MissionCompatibilityStatus.DOWNLOADING,
                        attempt = 1,
                    ),
            )
        val grant =
            LobbyService::class.java
                .getDeclaredMethod(
                    "handleMissionTransferGrant",
                    JSONObject::class.java,
                    String::class.java,
                ).apply { isAccessible = true }
        val retry =
            LobbyService::class.java
                .getDeclaredMethod(
                    "retryMissionTransfer",
                    String::class.java,
                    MissionRequirement::class.java,
                    Int::class.javaPrimitiveType,
                ).apply { isAccessible = true }

        fun packet(request: String) =
            JSONObject()
                .put("lobby_id", "grant-lobby")
                .put("revision", requirement.revision)
                .put("token", "fixture")
                .put("port", 42424)
                .put("attempt", 1)
                .put("request_id", request)
        try {
            field("pendingMissionTransferRequest").set(null, "replacement")
            grant.invoke(LobbyService, packet("old"), "192.0.2.1")
            check(field("acceptedMissionTransferRequest").get(null) == null) { "Old grant was accepted" }
            retry.invoke(LobbyService, "old", requirement, 2)
            check(field("pendingMissionTransferRequest").get(null) == "replacement") {
                "Old automatic retry replaced manual attempt"
            }
            grant.invoke(LobbyService, packet("replacement"), "192.0.2.1")
            check(field("acceptedMissionTransferRequest").get(null) == "replacement")
            val contact = field("lastHostSeenMs").getLong(null)
            grant.invoke(LobbyService, packet("replacement"), "192.0.2.1")
            check(field("lastHostSeenMs").getLong(null) == contact) { "Duplicate grant was processed twice" }
        } finally {
            LobbyService.stopDiscovery()
            field("appContext").set(null, previousContext)
        }
    }

    private fun waitFor(
        message: String,
        condition: () -> Boolean,
    ) {
        val deadline = android.os.SystemClock.elapsedRealtime() + 10_000
        while (!condition()) {
            check(android.os.SystemClock.elapsedRealtime() < deadline) { message }
            Thread.sleep(20)
        }
    }

    private fun probeHandoffWaitsForReader() {
        fun field(name: String) = MatchmakingService::class.java.getDeclaredField(name).apply { isAccessible = true }
        MatchmakingService.disconnect()
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val socket = DatagramSocket()
        val reader =
            scope.launch {
                entered.countDown()
                release.await() // Model a blocking receive that does not observe coroutine cancellation yet
            }
        try {
            check(entered.await(5, TimeUnit.SECONDS))
            field("candidateSocket").set(null, socket)
            field("connectivityCheckJob").set(null, reader)
            val start =
                MatchmakingService::class.java
                    .getDeclaredMethod(
                        "queueGameStart",
                        com.dxxredux.app.multiplayer.GameStartingMsg::class.java,
                    ).apply { isAccessible = true }
            start.invoke(
                MatchmakingService,
                com.dxxredux.app.multiplayer
                    .GameStartingMsg("127.0.0.1", "d2"),
            )
            check(reader.isCancelled)
            check(MatchmakingService.currentProxy() == null) { "Proxy started while probe still owned its socket" }
            check(field("candidateSocket").get(null) === socket)
            release.countDown()
            waitFor("Proxy did not receive socket after reader retired") { MatchmakingService.currentProxy() != null }
            check(field("candidateSocket").get(null) == null)
            check(!socket.isClosed) { "Probe retirement closed the transferred socket" }
        } finally {
            release.countDown()
            MatchmakingService.disconnect()
            socket.close()
            scope.cancel()
        }
    }

    private fun launcherActivityFailureAndRetry() {
        val activity =
            startActivitySync(
                android.content
                    .Intent(targetContext, SetupActivity::class.java)
                    .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as SetupActivity
        val handoff =
            SetupActivity::class.java
                .getDeclaredMethod(
                    "startGameAfterRouteMetadataHandoff",
                    android.content.Intent::class.java,
                ).apply { isAccessible = true }
        try {
            repeat(2) {
                val attempted = CountDownLatch(1)
                runOnMainSync {
                    val invalid =
                        android.content
                            .Intent()
                            .setClassName(targetContext.packageName, "missing.RecoveryActivity")
                            .putExtra("game", "d2")
                    handoff.invoke(activity, invalid)
                    attempted.countDown()
                }
                check(attempted.await(5, TimeUnit.SECONDS))
                waitFor("Failed Activity start left preparation busy") {
                    var finished = false
                    runOnMainSync {
                        finished =
                            activity.launchPreparationSnapshot() == null && activity.launchPreflightFailure != null
                    }
                    finished
                }
                waitForIdleSync()
            }
        } finally {
            runOnMainSync { activity.finish() }
            waitForIdleSync()
        }
    }

    @Suppress("UNCHECKED_CAST")
    private fun hostLaunchRollback() {
        fun field(name: String) = LobbyService::class.java.getDeclaredField(name).apply { isAccessible = true }
        LobbyService.stopDiscovery()
        val hosting = field("_isHosting").get(null) as MutableStateFlow<Boolean>
        val players = field("_hostedLobbyPlayers").get(null) as MutableStateFlow<List<LanPlayer>>
        val started = field("gameStarted")
        hosting.value = true
        field("hostedLobbyId").set(null, "recovery-host")
        val status = MissionStatusReport("fixture", MissionCompatibilityStatus.MATCH)
        players.value =
            listOf(
                LanPlayer("Host", "127.0.0.1", ready = true, missionStatus = status),
                LanPlayer("Client", "192.0.2.1", ready = true, missionStatus = status),
            )
        try {
            LobbyService.startGame(1, 2)
            val first = checkNotNull(LobbyService.lanLaunchEvent.value)
            check(!started.getBoolean(null)) { "Host advertised in-game before Activity launch" }
            LobbyService.failGameLaunch(first, "Injected Activity start failure")
            check(LobbyService.lanLaunchEvent.value == null) { "Failed launch event was retained" }
            check(LobbyService.chatMessages.value.any { it.text.contains("Injected Activity start failure") })
            LobbyService.startGame(1, 2)
            val second = checkNotNull(LobbyService.lanLaunchEvent.value)
            check(second !== first) { "Retry reused old host attempt" }
            LobbyService.failGameLaunch(first, "Late failure")
            LobbyService.confirmGameLaunch(first)
            check(LobbyService.lanLaunchEvent.value === second && !started.getBoolean(null)) {
                "Obsolete failure/confirmation changed the new launch"
            }
            LobbyService.confirmGameLaunch(second)
            check(started.getBoolean(null)) { "Successful retry was not committed" }
        } finally {
            LobbyService.stopDiscovery()
        }
    }

    private class FakeWebSocket : WebSocket {
        var sends = 0

        override fun request(): Request = Request.Builder().url("http://127.0.0.1/").build()

        override fun queueSize(): Long = 0

        override fun send(text: String): Boolean {
            sends++
            return true
        }

        override fun send(bytes: ByteString): Boolean {
            sends++
            return true
        }

        override fun close(
            code: Int,
            reason: String?,
        ): Boolean = true

        override fun cancel() {}
    }

    private fun obsoleteWebSocketCallbacks() {
        MatchmakingService.disconnect()
        val socketField = MatchmakingService::class.java.getDeclaredField("webSocket").apply { isAccessible = true }
        val disconnectField =
            MatchmakingService::class.java.getDeclaredField("manualDisconnect").apply {
                isAccessible =
                    true
            }
        val listenerClass = Class.forName("com.dxxredux.app.multiplayer.MatchmakingService\$Listener")
        val constructor = listenerClass.getDeclaredConstructor(String::class.java).apply { isAccessible = true }
        val listener = constructor.newInstance("Old pilot") as WebSocketListener
        val old = FakeWebSocket()
        val current = FakeWebSocket()
        socketField.set(null, current)
        disconnectField.setBoolean(null, false)
        MatchmakingStateHolder.update {
            it.copy(status = ConnectionStatus.CONNECTED, playerId = "new-player", errorMessage = null)
        }
        val before = MatchmakingStateHolder.state.value
        try {
            listener.onOpen(
                old,
                Response
                    .Builder()
                    .request(
                        old.request(),
                    ).protocol(Protocol.HTTP_1_1)
                    .code(101)
                    .message("Switching")
                    .build(),
            )
            listener.onMessage(old, "{\"type\":\"ERROR\",\"message\":\"obsolete error\"}")
            listener.onClosed(old, 1000, "late close")
            listener.onFailure(old, java.io.IOException("late failure"), null)
            MatchmakingService.failGameLaunch(MatchmakingService.networkAttempt() - 1, true)
            check(socketField.get(null) === current) { "Old callback cleared replacement WebSocket" }
            check(MatchmakingStateHolder.state.value == before) { "Old callback changed replacement session" }
            check(current.sends == 0) { "Old connection authenticated the replacement socket" }
        } finally {
            MatchmakingService.disconnect()
        }
    }

    private fun proxyFailureAndRetry() {
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val loopback = InetAddress.getByName("127.0.0.1")
        val occupied = DatagramSocket(0, loopback)
        val port = occupied.localPort
        val config = PeerProxyConfig(0, port, InetSocketAddress(loopback, 42424), false)
        val failed = LocalhostProxy(scope)
        try {
            var rejected = false
            try {
                failed.addPeer(config)
            } catch (_: java.io.IOException) {
                rejected = true
            }
            check(rejected) { "Proxy silently accepted an occupied local port" }
            failed.shutdown()
            occupied.close()
            val notified = CountDownLatch(1)
            val real = DatagramSocket()
            val replacement = LocalhostProxy(scope, real, onFailure = { notified.countDown() })
            try {
                replacement.addPeer(config)
                replacement.start()
                check(replacement.getStats().size == 1) { "Retry did not install the peer" }
                real.close()
                check(notified.await(5, TimeUnit.SECONDS)) { "Forwarding failure was not reported" }
                DatagramSocket(port, loopback).close()
                check(replacement.getStats().isEmpty()) { "Failed proxy retained peers" }
            } finally {
                replacement.shutdown()
                real.close()
            }
        } finally {
            occupied.close()
            failed.shutdown()
            scope.cancel()
        }
    }

    private fun missionTransferPhaseCancellation(phase: MissionCompatibilityStatus) {
        val bytes = ByteArray(2048).also { java.security.SecureRandom().nextBytes(it) }
        val source = File(targetContext.cacheDir, "transfer-phase-${UUID.randomUUID()}.bin")
        source.writeBytes(bytes)
        val identity = MissionContentIdentity.compute(source, 1024)
        val partial = File(targetContext.filesDir, ".mission_transfers/${identity.sha256}.partial")
        val sidecar = File(targetContext.filesDir, ".mission_transfers/${identity.sha256}.json")
        val requirement =
            MissionRequirement(
                revision = UUID.randomUUID().toString(),
                game = "d2",
                missionKey = "recovery-fixture",
                displayName = "Recovery fixture",
                kind = MissionRequirement.KIND_WRAPPER,
                wrapperFilename = "recovery-fixture.zip",
                sizeBytes = identity.sizeBytes,
                sha256 = identity.sha256,
                offerAvailable = true,
            )
        val cancelled = CountDownLatch(1)
        val completions = AtomicInteger()
        val lateStatuses = AtomicInteger()
        try {
            ServerSocket(0).use { server ->
                server.soTimeout = 5000
                MissionTransferService.download(
                    targetContext,
                    "127.0.0.1",
                    MissionTransferGrant("phase", server.localPort, requirement.revision),
                    requirement,
                    1,
                    onStatus = {
                        if (cancelled.count == 0L) lateStatuses.incrementAndGet()
                        if (it.status == phase) {
                            MissionTransferService.cancelClient()
                            cancelled.countDown()
                        }
                    },
                    onFinished = { completions.incrementAndGet() },
                )
                server.accept().use { peer ->
                    peer.soTimeout = 5000
                    peer.getInputStream().bufferedReader().readLine()
                    val header =
                        JSONObject()
                            .put("size_bytes", identity.sizeBytes)
                            .put("sha256", identity.sha256)
                            .put("chunk_size", 1024)
                            .put("chunk_sha256", JSONArray(identity.chunkSha256))
                    peer.getOutputStream().write((header.toString() + "\n").toByteArray())
                    peer.getOutputStream().write(bytes)
                    peer.shutdownOutput()
                    check(cancelled.await(5, TimeUnit.SECONDS)) { "Transfer did not reach $phase" }
                    check(peer.getInputStream().read() == -1)
                }
                // Wait for actual file ownership retirement, not just the cancellation callback
                val mutex =
                    MissionTransferService::class.java
                        .getDeclaredField("clientWriter")
                        .apply {
                            isAccessible =
                                true
                        }.get(null) as kotlinx.coroutines.sync.Mutex
                kotlinx.coroutines.runBlocking {
                    kotlinx.coroutines.withTimeout(5000) {
                        mutex.lock()
                        mutex.unlock()
                    }
                }
                check(completions.get() == 0 && lateStatuses.get() == 0) { "Cancelled $phase published a result" }
                check(partial.readBytes().contentEquals(bytes)) { "Cancelled $phase lost verified data" }
            }
        } finally {
            MissionTransferService.cancelClient()
            source.delete()
            partial.delete()
            sidecar.delete()
        }
    }

    private fun missionTransferCancellation() {
        val bytes = ByteArray(2048).also { java.security.SecureRandom().nextBytes(it) }
        val source = File(targetContext.cacheDir, "transfer-recovery-${UUID.randomUUID()}.bin")
        source.writeBytes(bytes)
        val identity = MissionContentIdentity.compute(source, 1024)
        val partialRoot = File(targetContext.filesDir, ".mission_transfers")
        val partial = File(partialRoot, "${identity.sha256}.partial")
        val sidecar = File(partialRoot, "${identity.sha256}.json")
        check(!partial.exists() && !sidecar.exists()) { "Recovery fixture already exists" }
        val requirement =
            MissionRequirement(
                revision = UUID.randomUUID().toString(),
                game = "d2",
                missionKey = "recovery-fixture",
                displayName = "Recovery fixture",
                kind = MissionRequirement.KIND_WRAPPER,
                wrapperFilename = "recovery-fixture.zip",
                sizeBytes = identity.sizeBytes,
                sha256 = identity.sha256,
                offerAvailable = true,
            )
        val oldCallbacks = AtomicInteger()
        val firstChunk = CountDownLatch(1)
        val secondChunk = CountDownLatch(1)
        val peers = mutableListOf<Socket>()
        try {
            ServerSocket(0).use { server ->
                server.soTimeout = 5000

                fun start(attempt: Int) {
                    MissionTransferService.download(
                        targetContext,
                        "127.0.0.1",
                        MissionTransferGrant("attempt-$attempt", server.localPort, requirement.revision),
                        requirement,
                        attempt,
                        onStatus = { report ->
                            if (attempt == 1) oldCallbacks.incrementAndGet()
                            check(
                                report.status == MissionCompatibilityStatus.DOWNLOADING,
                            ) { "Unexpected status $report" }
                            if (report.verifiedBytes == 1024L) firstChunk.countDown()
                            if (report.verifiedBytes == 2048L) secondChunk.countDown()
                        },
                        onFinished = { error("Cancelled transfer must not publish completion") },
                    )
                }

                fun accept(offset: Long): Socket {
                    val peer =
                        server.accept().also {
                            peers.add(it)
                            it.soTimeout = 3000
                        }
                    val request = JSONObject(peer.getInputStream().bufferedReader().readLine())
                    check(request.getLong("offset") == offset) { "Resume offset was not verified" }
                    val header =
                        JSONObject()
                            .put("size_bytes", identity.sizeBytes)
                            .put("sha256", identity.sha256)
                            .put("chunk_size", 1024)
                            .put("chunk_sha256", JSONArray(identity.chunkSha256))
                    peer.getOutputStream().write((header.toString() + "\n").toByteArray())
                    return peer
                }
                start(1)
                val first = accept(0)
                first.getOutputStream().write(bytes, 0, 1024)
                first.getOutputStream().flush()
                check(firstChunk.await(5, TimeUnit.SECONDS)) { "First chunk was not verified" }
                start(2)
                check(first.getInputStream().read() == -1) { "Cancellation did not close old TCP socket" }
                val callbacksAfterCancel = oldCallbacks.get()
                val second = accept(1024)
                second.getOutputStream().write(bytes, 1024, 1024)
                second.getOutputStream().flush()
                check(secondChunk.await(5, TimeUnit.SECONDS)) { "Replacement writer did not resume" }
                MissionTransferService.cancelClient()
                check(second.getInputStream().read() == -1) { "Replacement TCP socket did not close" }
                check(partial.readBytes().contentEquals(bytes)) { "Concurrent writers corrupted resumed content" }
                check(oldCallbacks.get() == callbacksAfterCancel) { "Old transfer published after cancellation" }
            }
        } finally {
            MissionTransferService.cancelClient()
            peers.forEach { it.close() }
            source.delete()
            partial.delete()
            sidecar.delete()
        }
    }
}
