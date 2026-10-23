package com.dxxredux.app.multiplayer

import android.app.Activity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.DataOutputStream
import java.io.File
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.security.KeyPairGenerator
import java.security.MessageDigest
import java.security.spec.ECGenParameterSpec
import java.util.UUID
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicReference

/** The actual service job and Rust listeners, with native process launch deliberately outside this fixture */
class OnlineCandidateDiscoveryTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    private fun awaitState(
        label: String,
        predicate: () -> Boolean,
    ) {
        val deadline = System.nanoTime() + 15_000_000_000L
        while (!predicate()) {
            assertTrue(label, System.nanoTime() < deadline)
            Thread.sleep(20)
        }
    }

    @Test(timeout = 120000)
    fun actualServiceRedeemsReceiptsAndHandsTheProvenSocketToDirectAndRelayProxies() {
        val binary = System.getProperty("dxx.onlineServerBinary")
        assumeTrue("Requires the owned real matchmaking server", binary != null)
        for (relay in listOf(false, true)) qualify(binary, relay)
    }

    private fun qualify(
        binary: String,
        relay: Boolean,
    ) {
        val directory = temporaryFolder.newFolder(if (relay) "relay" else "direct")
        val loopback = InetAddress.getByName("127.0.0.1")
        val ports =
            ServerSocket(0).use { first ->
                ServerSocket(0).use { second -> first.localPort to second.localPort }
            }
        val udpPorts =
            DatagramSocket(0, loopback).use { first ->
                DatagramSocket(0, loopback).use { second ->
                    DatagramSocket(0, loopback).use { third ->
                        Triple(first.localPort, second.localPort, third.localPort)
                    }
                }
            }
        val audience = "http://127.0.0.1:${ports.first}"
        // Seed the existing host-JVM identity format; Android Keystore and real GPGS are not simulated
        val origin =
            MessageDigest.getInstance("SHA-256").digest(audience.toByteArray()).joinToString("") {
                "%02x".format(it.toInt() and 255)
            }
        val key =
            KeyPairGenerator
                .getInstance(
                    "EC",
                ).apply { initialize(ECGenParameterSpec("secp256r1")) }
                .generateKeyPair()
        val identity = File(directory, "online_identity/$origin/identity.bin")
        identity.parentFile!!.mkdirs()
        DataOutputStream(identity.outputStream()).use { output ->
            output.writeInt(1)
            output.writeLong(0)
            for (part in listOf(key.public.encoded, key.private.encoded)) {
                output.writeInt(part.size)
                output.write(part)
            }
        }
        val config = File(directory, "server.json")
        config.writeText(
            """{
            "ws_listen_addr":"127.0.0.1:${ports.first}","http_listen_addr":"127.0.0.1:${ports.second}",
            "relay_listen_addr":"127.0.0.1:${udpPorts.third}","relay_public_addr":"127.0.0.1:${udpPorts.third}",
            "db_path":":memory:","skip_gpgs_verify":true,
            "max_relay_sessions":1,"force_relay":$relay,"pow_difficulty":8,
            "relay_policy":{"allocation_lifetime_seconds":${if (relay) 40 else 7200}},
            "installation_auth_audience":"$audience","stun_listen_addr":"127.0.0.1:${udpPorts.first}",
            "stun_listen_addr_alt":"127.0.0.1:${udpPorts.second}",
            "stun_public_addrs":"127.0.0.1:${udpPorts.first},127.0.0.1:${udpPorts.second}"
        }""",
        )
        val process =
            ProcessBuilder(binary)
                .directory(directory)
                .apply {
                    val systemRoot = environment()["SystemRoot"]
                    environment().clear()
                    if (systemRoot != null) environment()["SystemRoot"] = systemRoot
                    environment()["CONFIG_FILE"] = config.absolutePath
                    redirectOutput(ProcessBuilder.Redirect.DISCARD)
                    redirectError(ProcessBuilder.Redirect.DISCARD)
                }.start()
        val http = OkHttpClient()
        val messages = LinkedBlockingQueue<JSONObject>(128)
        val backlog = mutableListOf<JSONObject>()
        val controlFailure = AtomicReference<Throwable?>()
        val udpFailure = AtomicReference<Throwable?>()
        val stopped = AtomicBoolean(false)
        val echoStopped = AtomicBoolean(false)
        val relayProxy = AtomicReference<LocalhostProxy?>()
        val relayOwner = CoroutineScope(SupervisorJob() + Dispatchers.IO)
        val commits = AtomicInteger()
        val echoed = AtomicInteger(0)
        val provenClient = AtomicReference<InetSocketAddress?>()
        val hostUdp = DatagramSocket(0, loopback).apply { soTimeout = 100 }
        val service = MatchmakingService
        var host: WebSocket? = null
        var echoWorker: Thread? = null

        fun receive(kind: String): JSONObject {
            val saved = backlog.indexOfFirst { it.optString("type") == kind }
            if (saved >= 0) return backlog.removeAt(saved)
            val deadline = System.nanoTime() + 15_000_000_000L
            while (true) {
                controlFailure.get()?.let { throw AssertionError("Owned control failed", it) }
                assertTrue("Missing real $kind", System.nanoTime() < deadline)
                val message = messages.poll(100, TimeUnit.MILLISECONDS) ?: continue
                if (message.optString("type") == kind) return message
                assertTrue("Unexpected owned server error", message.optString("type") != "ERROR")
                assertTrue(backlog.size < 128)
                backlog.add(message)
            }
        }
        try {
            service.disconnect()
            awaitState("Owned server did not start") {
                check(process.isAlive) { "Owned server exited" }
                runCatching { java.net.Socket(loopback, ports.first).use { true } }.getOrDefault(false)
            }
            host =
                http.newWebSocket(
                    Request.Builder().url("ws://127.0.0.1:${ports.first}/ws").build(),
                    object : WebSocketListener() {
                        override fun onMessage(
                            webSocket: WebSocket,
                            text: String,
                        ) {
                            val json = JSONObject(text)
                            if (json.optString("type").startsWith("RELAY_REKEY_")) {
                                try {
                                    val proxy = checkNotNull(relayProxy.get())
                                    val message = protocolJson.decodeFromString<RelayRekeyMsg>(text)
                                    when (json.getString("type")) {
                                        "RELAY_REKEY_OFFER" -> {
                                            check(
                                                proxy.stageRelayRekey(message) {
                                                    if (!stopped.get() && relayProxy.get() === proxy) {
                                                        val ready =
                                                            RelayRekeyReadyMsg(
                                                                message.lobbyId,
                                                                message.gameEpoch,
                                                                message.oldToken,
                                                                message.newToken,
                                                                message.newEpoch,
                                                            )
                                                        webSocket.send(
                                                            protocolJson.encodeToString(
                                                                RelayRekeyReadyMsg.serializer(),
                                                                ready,
                                                            ),
                                                        )
                                                    }
                                                },
                                            )
                                        }

                                        "RELAY_REKEY_COMMIT" -> {
                                            check(proxy.commitRelayRekey(message.scope(), message.retireAfterMs))
                                            commits.incrementAndGet()
                                        }

                                        "RELAY_REKEY_ABORT" -> {
                                            error("Healthy renewal was aborted")
                                        }
                                    }
                                } catch (error: Throwable) {
                                    controlFailure.compareAndSet(null, error)
                                }
                                return
                            }
                            if (!messages.offer(
                                    json,
                                )
                            ) {
                                controlFailure.compareAndSet(null, IllegalStateException("Owned queue full"))
                            }
                        }

                        override fun onFailure(
                            webSocket: WebSocket,
                            t: Throwable,
                            response: Response?,
                        ) {
                            if (!stopped.get()) controlFailure.compareAndSet(null, t)
                        }
                    },
                )
            host.send(
                """{"type":"AUTHENTICATE","protocol_version":${NetworkConstants.PROTOCOL_VERSION},
                "client_version":"owned-fixture","callsign":"Host","platform":"test",
                "auth_method":"dev","play_games_token":"owned-host"}""",
            )
            val hostId = receive("AUTH_OK").getString("player_id")
            host.send("""{"type":"CREATE_LOBBY","game":"d2","max_players":2,"game_info":{}}""")
            val room = receive("LOBBY_UPDATE").getString("lobby_id")
            val activity =
                object : Activity() {
                    override val noBackupFilesDir = directory
                }
            service.setActivity(activity)
            service.connect("ws://127.0.0.1:${ports.first}/ws", "Client")
            awaitState("Actual service authentication") {
                MatchmakingStateHolder.state.value.status ==
                    ConnectionStatus.CONNECTED
            }
            java.lang.ref.Reference
                .reachabilityFence(activity)
            val clientId = MatchmakingStateHolder.state.value.playerId!!
            service.joinLobby(room)
            val roster = receive("LOBBY_UPDATE").getJSONArray("players")
            val admission =
                (0 until roster.length())
                    .map { roster.getJSONObject(it) }
                    .single {
                        it.getString("player_id") ==
                            hostId
                    }.getString("candidate_admission_id")
            val advertised = receive("PEER_CANDIDATES")
            assertEquals(clientId, advertised.getString("peer_id"))
            val candidate = advertised.getJSONArray("candidates").getJSONObject(0)
            assertEquals(1, advertised.getJSONArray("candidates").length())
            assertEquals("srflx", candidate.getString("candidate_type"))
            val clientEndpoint = requireNotNull(parseProbeEndpoint(candidate.getString("addr")))
            provenClient.set(clientEndpoint)
            host.send(
                JSONObject()
                    .put("type", "GATHER_CANDIDATES")
                    .put("lobby_id", room)
                    .put("admission_id", admission)
                    .put("request_id", UUID.randomUUID().toString())
                    .toString(),
            )
            val challenge = receive("CANDIDATE_CHALLENGE")
            val observations =
                StunProofClient.discover(
                    listOf(InetSocketAddress(loopback, udpPorts.first), InetSocketAddress(loopback, udpPorts.second)),
                    UUID.fromString(challenge.getString("credential")),
                    hostUdp,
                )
            assertEquals(2, observations.size)
            assertTrue(observations.all { it.endpoint == hostUdp.localSocketAddress })
            echoWorker =
                Thread {
                    try {
                        while (!stopped.get() && !echoStopped.get()) {
                            val packet = DatagramPacket(ByteArray(13), 13)
                            try {
                                hostUdp.receive(packet)
                            } catch (_: SocketTimeoutException) {
                                continue
                            }
                            if (packet.length != 12 || packet.socketAddress != provenClient.get()) continue
                            val frame = ByteBuffer.wrap(packet.data)
                            if (frame.int != 0x44585043 || frame.getShort(10).toInt() != 1) continue
                            frame.putShort(10, 2)
                            hostUdp.send(packet)
                            echoed.incrementAndGet()
                        }
                    } catch (error: Throwable) {
                        if (!stopped.get()) udpFailure.set(error)
                    }
                }.apply {
                    isDaemon = true
                    start()
                }
            host.send(
                JSONObject()
                    .put("type", "STUN_RESULT")
                    .put("attempt_id", challenge.getString("attempt_id"))
                    .put("receipts", JSONArray(observations.map { it.receipt.toString() }))
                    .toString(),
            )
            val checks = receive("CONNECTIVITY_CHECK_GO").getJSONArray("peer_addrs")
            val check = checks.getJSONObject(0)
            host.send(
                JSONObject()
                    .put("type", "CONNECTIVITY_OK")
                    .put("peer_id", clientId)
                    .put(
                        "check_id",
                        check.getString("check_id"),
                    ).put("winning_candidate_type", "srflx")
                    .put("rtt_ms", 1)
                    .toString(),
            )
            awaitState("Actual service direct proof result") {
                val diagnostic = service.resourceDiagnostics()["connectivity"] as Map<*, *>
                (diagnostic["results"] as List<*>).isNotEmpty()
            }
            assertTrue(echoed.get() > 0)
            val discoverySocket =
                service.javaClass
                    .getDeclaredField(
                        "candidateSocket",
                    ).apply { isAccessible = true }
                    .get(service)
            service.setReady(true)
            host.send("""{"type":"READY","ready":true}""")
            var ready = false
            repeat(8) {
                if (!ready) {
                    val players = receive("LOBBY_UPDATE").getJSONArray("players")
                    ready =
                        players.length() == 2 &&
                        (0 until players.length()).all { players.getJSONObject(it).getBoolean("ready") }
                }
            }
            assertTrue("Ready acknowledgements must follow the service's connectivity report", ready)
            host.send("""{"type":"START_GAME"}""")
            val started = receive("GAME_STARTING")
            assertEquals(relay, started.getJSONArray("peers").getJSONObject(0).getBoolean("is_relay"))
            awaitState("Actual socket handoff") {
                MatchmakingStateHolder.state.value.gameLaunchInfo != null &&
                    service.currentProxy() != null
            }
            val proxy = requireNotNull(service.currentProxy())
            assertSame(
                discoverySocket,
                proxy.javaClass
                    .getDeclaredField("sharedRealSocket")
                    .apply {
                        isAccessible = true
                    }.get(proxy),
            )
            assertEquals(
                relay,
                MatchmakingStateHolder.state.value.gameLaunchInfo!!
                    .peers
                    .single()
                    .isRelay,
            )
            assertFalse((discoverySocket as DatagramSocket).isClosed)
            assertEquals(clientEndpoint.port, discoverySocket.localPort)
            if (relay) {
                echoStopped.set(true)
                echoWorker.join(2000)
                assertFalse(echoWorker.isAlive)
                hostUdp.soTimeout = 0
                val hostPort = DatagramSocket(0, loopback).use { it.localPort }
                val peer = started.getJSONArray("peers").getJSONObject(0)
                val credentials = peer.getJSONObject("relay_credentials")
                val hostProxy =
                    LocalhostProxy(
                        relayOwner,
                        hostUdp,
                        onFailure = { controlFailure.compareAndSet(null, IllegalStateException(it)) },
                    )
                hostProxy.reconcileRoster(mapOf(1 to clientId))
                hostProxy.addPeer(
                    PeerProxyConfig(
                        1,
                        hostPort,
                        InetSocketAddress(loopback, udpPorts.third),
                        true,
                        peer.getLong("relay_token").toUInt(),
                        1,
                        0,
                        credentials.getString("send_key"),
                        credentials.getString("receive_key"),
                        relayEpoch = credentials.getString("allocation_epoch"),
                    ),
                )
                relayProxy.set(hostProxy)
                hostProxy.start()
                awaitState("Both actual proxy bindings") {
                    hostProxy.getStats().all { it.relayBound } &&
                        proxy.getStats().all { it.relayBound }
                }
                DatagramSocket(42424, loopback).use { engine ->
                    engine.soTimeout = 3000
                    val until = System.nanoTime() + 53_000_000_000L
                    var sequence = 0
                    while (System.nanoTime() < until) {
                        controlFailure.get()?.let { throw AssertionError("Owned renewal failed", it) }
                        val payload = byteArrayOf(16, (sequence++).toByte(), 42)
                        engine.send(DatagramPacket(payload, payload.size, loopback, NetworkConstants.PROXY_PORT_BASE))
                        val toHost = DatagramPacket(ByteArray(64), 64)
                        engine.receive(toHost)
                        assertEquals(hostPort, toHost.port)
                        assertTrue(payload.contentEquals(toHost.data.copyOf(toHost.length)))
                        engine.send(DatagramPacket(payload, payload.size, loopback, hostPort))
                        val toClient = DatagramPacket(ByteArray(64), 64)
                        engine.receive(toClient)
                        assertEquals(NetworkConstants.PROXY_PORT_BASE, toClient.port)
                        assertTrue(payload.contentEquals(toClient.data.copyOf(toClient.length)))
                        assertSame(proxy, service.currentProxy())
                        Thread.sleep(150)
                    }
                }
                assertTrue("Multiple real key epochs must commit", commits.get() >= 2)
                assertSame(
                    discoverySocket,
                    proxy.javaClass
                        .getDeclaredField("sharedRealSocket")
                        .apply {
                            isAccessible =
                                true
                        }.get(proxy),
                )
                assertTrue(proxy.getStats().single().packetsSent > 100)
            }
            udpFailure.get()?.let { throw AssertionError("Owned UDP echo failed", it) }
        } finally {
            stopped.set(true)
            echoStopped.set(true)
            relayProxy.getAndSet(null)?.shutdown()
            relayOwner.cancel()
            service.disconnect()
            host?.cancel()
            hostUdp.close()
            echoWorker?.join(2000)
            assertTrue(echoWorker?.isAlive != true)
            http.connectionPool.evictAll()
            http.dispatcher.executorService.shutdown()
            process.destroy()
            if (!process.waitFor(3, TimeUnit.SECONDS)) {
                process.destroyForcibly()
                process.waitFor()
            }
            awaitState("Owned proxy workers released") { LocalhostProxy.activeWorkerCount() == 0 }
        }
    }
}
