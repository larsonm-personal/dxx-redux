package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.util.concurrent.CompletableFuture
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

/** Real control server and relay/proxy producers; this does not substitute for native device qualification */
class OnlineOutageOrderingTest {
    private val loopback = InetAddress.getByName("127.0.0.1")

    private fun awaitState(
        label: String,
        seconds: Int = 8,
        predicate: () -> Boolean,
    ) {
        val deadline = System.nanoTime() + seconds * 1_000_000_000L
        while (!predicate()) {
            assertTrue(label, System.nanoTime() < deadline)
            Thread.sleep(20)
        }
    }

    private class UdpGate(
        private val relay: InetSocketAddress,
    ) : AutoCloseable {
        val front = DatagramSocket(0, InetAddress.getLoopbackAddress())
        val dropped =
            java.util.concurrent.ConcurrentHashMap
                .newKeySet<Int>()
        private val channels = java.util.concurrent.ConcurrentHashMap<InetSocketAddress, DatagramSocket>()
        private val workers = java.util.concurrent.CopyOnWriteArrayList<Thread>()
        val failure = AtomicReference<Throwable?>()
        private val stopped =
            java.util.concurrent.atomic
                .AtomicBoolean()

        init {
            worker {
                while (!stopped.get()) {
                    val packet = DatagramPacket(ByteArray(8192), 8192)
                    front.receive(packet)
                    if (packet.length < RelayTransport.HEADER_SIZE) continue
                    val slot = packet.data[9].toInt() and 255
                    val client = InetSocketAddress(packet.address, packet.port)
                    val upstream =
                        channels.computeIfAbsent(client) {
                            DatagramSocket(0, InetAddress.getLoopbackAddress()).also { socket ->
                                worker {
                                    while (!stopped.get()) {
                                        val response = DatagramPacket(ByteArray(8192), 8192)
                                        socket.receive(response)
                                        if (slot !in dropped) {
                                            val reply = DatagramPacket(response.data, response.length, client)
                                            front.send(reply)
                                        }
                                    }
                                }
                            }
                        }
                    if (slot !in dropped) upstream.send(DatagramPacket(packet.data, packet.length, relay))
                }
            }
        }

        private fun worker(block: () -> Unit) {
            Thread {
                try {
                    block()
                } catch (e: Throwable) {
                    if (!stopped.get()) failure.compareAndSet(null, e)
                }
            }.also {
                workers += it
                it.isDaemon = true
                it.start()
            }
        }

        override fun close() {
            stopped.set(true)
            front.close()
            channels.values.forEach { it.close() }
            workers.forEach {
                it.join(2000)
                check(!it.isAlive)
            }
        }
    }

    @Test(timeout = 45000)
    fun joinerTimeoutBeforeHostTimeoutPreservesHostOnlyRoom() {
        val binary = System.getProperty("dxx.onlineServerBinary")
        assumeTrue("Requires the owned real matchmaking server", binary != null)
        val directory =
            java.nio.file.Files
                .createTempDirectory("online-outage-order")
                .toFile()
        val wsPort = ServerSocket(0).use { it.localPort }
        val httpPort = ServerSocket(0).use { it.localPort }
        val relayPort = DatagramSocket(0, loopback).use { it.localPort }
        val audience = "http://127.0.0.1:$wsPort"
        val gate = UdpGate(InetSocketAddress(loopback, relayPort))

        fun key() =
            java.security.KeyPairGenerator
                .getInstance("EC")
                .apply {
                    initialize(java.security.spec.ECGenParameterSpec("secp256r1"))
                }.generateKeyPair()
        val joinKey = key()
        val origin =
            java.security.MessageDigest
                .getInstance("SHA-256")
                .digest(audience.toByteArray())
                .joinToString("") { "%02x".format(it.toInt() and 255) }
        val identity = java.io.File(directory, "online_identity/$origin/identity.bin")
        identity.parentFile.mkdirs()
        java.io.DataOutputStream(identity.outputStream()).use { output ->
            output.writeInt(1)
            output.writeLong(System.currentTimeMillis())
            for (part in listOf(joinKey.public.encoded, joinKey.private.encoded)) {
                output.writeInt(part.size)
                output.write(part)
            }
        }
        val config = java.io.File(directory, "config.json")
        config.writeText(
            """{
              "ws_listen_addr":"127.0.0.1:$wsPort",
              "http_listen_addr":"127.0.0.1:$httpPort",
              "relay_listen_addr":"127.0.0.1:$relayPort",
              "relay_public_addr":"127.0.0.1:${gate.front.localPort}",
              "stun_listen_addr":"127.0.0.1:0", "stun_listen_addr_alt":"127.0.0.1:0",
              "db_path":":memory:", "skip_gpgs_verify":true, "force_relay":true,
              "pow_difficulty":8, "installation_auth_audience":"$audience"
            }""",
        )
        val process =
            ProcessBuilder(binary)
                .directory(directory)
                .apply {
                    // Winsock provider initialization requires SystemRoot on Windows
                    val systemRoot = environment()["SystemRoot"]
                    environment().clear()
                    if (systemRoot != null) environment()["SystemRoot"] = systemRoot
                    environment()["CONFIG_FILE"] = config.absolutePath
                    redirectOutput(ProcessBuilder.Redirect.DISCARD)
                    redirectError(ProcessBuilder.Redirect.DISCARD)
                }.start()
        val client = okhttp3.OkHttpClient()
        val messages = LinkedBlockingQueue<JSONObject>()
        val hostProxy = AtomicReference<LocalhostProxy?>()
        val hostId = AtomicReference<String>()
        val roster = AtomicReference<JSONObject>()
        val hostFailure = CompletableFuture<String>()
        val owner = SupervisorJob()
        val scope = CoroutineScope(owner + Dispatchers.IO)
        var socket: okhttp3.WebSocket? = null
        val service = MatchmakingService

        fun receive(kind: String): JSONObject {
            val deadline = System.nanoTime() + 8_000_000_000L
            while (true) {
                val message = messages.poll(1, TimeUnit.SECONDS)
                if (message?.optString("type") == kind) return message
                assertTrue("Missing real server $kind", System.nanoTime() < deadline)
            }
        }

        fun status(): JSONObject =
            JSONObject(
                java.net
                    .URI("http://127.0.0.1:$httpPort/api/v1/ready")
                    .toURL()
                    .openConnection()
                    .apply {
                        connectTimeout = 1000
                        readTimeout = 1000
                    }.getInputStream()
                    .bufferedReader()
                    .use { it.readText() },
            )
        try {
            awaitState("server readiness") {
                check(process.isAlive)
                runCatching { status().getBoolean("ready") }.getOrDefault(false)
            }
            socket =
                client.newWebSocket(
                    okhttp3.Request
                        .Builder()
                        .url("ws://127.0.0.1:$wsPort/ws")
                        .build(),
                    object : okhttp3.WebSocketListener() {
                        override fun onMessage(
                            webSocket: okhttp3.WebSocket,
                            text: String,
                        ) {
                            val msg = JSONObject(text)
                            if (msg.optString("type") == "LOBBY_UPDATE") {
                                roster.set(msg)
                                val players = msg.getJSONArray("players")
                                val peers =
                                    (0 until players.length())
                                        .map { players.getJSONObject(it) }
                                        .filter { it.getString("player_id") != hostId.get() }
                                        .associate { it.getInt("slot") to it.getString("player_id") }
                                hostProxy.get()?.reconcileRoster(peers)
                            }
                            messages.offer(msg)
                        }
                    },
                )
            val producer = OnlineInstallationAuth(key(), "OwnedHost", audience, "test")
            socket.send(protocolJson.encodeToString(AuthenticateMsg.serializer(), producer.request()))
            val challenge = receive("INSTALLATION_CHALLENGE")
            val proof = runBlocking { producer.proof(challenge.getString("challenge"), challenge.getInt("difficulty")) }
            socket.send(protocolJson.encodeToString(InstallationProofMsg.serializer(), proof))
            hostId.set(receive("AUTH_OK").getString("player_id"))
            socket.send("""{"type":"CREATE_LOBBY","game":"d1","max_players":2,"game_info":{"mode":"anarchy"}}""")
            val room = receive("LOBBY_UPDATE").getString("lobby_id")
            service.disconnect()
            service.setActivity(
                object : android.app.Activity() {
                    override val noBackupFilesDir = directory
                },
            )
            service.connect("ws://127.0.0.1:$wsPort/ws", "OwnedJoiner")
            awaitState("joiner authentication") {
                MatchmakingStateHolder.state.value.status ==
                    ConnectionStatus.CONNECTED
            }
            // Force-relay fixture does not require STUN; keep independent discovery out of this ordering test
            service.javaClass
                .getDeclaredField("stunCompleted")
                .apply { isAccessible = true }
                .set(service, true)
            service.joinLobby(room)
            awaitState("joiner membership") {
                MatchmakingStateHolder.state.value.currentLobby
                    ?.players
                    ?.size == 2
            }
            service.setReady(true)
            socket.send("""{"type":"READY","ready":true}""")
            awaitState("real ready roster") {
                roster.get()?.getJSONArray("players")?.let { p ->
                    p.length() == 2 && (0 until p.length()).all { p.getJSONObject(it).getBoolean("ready") }
                } == true
            }
            socket.send("""{"type":"START_GAME"}""")
            val start = protocolJson.decodeFromString(GameStartingMsg.serializer(), receive("GAME_STARTING").toString())
            val peer = start.peers.single()
            val credentials = requireNotNull(peer.relayCredentials)
            val proxy = LocalhostProxy(scope, DatagramSocket(0, loopback), onFailure = { hostFailure.complete(it) })
            hostProxy.set(proxy)
            val localPort = DatagramSocket(0, loopback).use { it.localPort }
            val address = peer.addr.split(':')
            proxy.reconcileRoster(mapOf(1 to MatchmakingStateHolder.state.value.playerId!!))
            proxy.addPeer(
                PeerProxyConfig(
                    1,
                    localPort,
                    InetSocketAddress(address[0], address[1].toInt()),
                    true,
                    peer.relayToken!!.toUInt(),
                    1,
                    credentials.ownSlot,
                    credentials.sendKey,
                    credentials.receiveKey,
                ),
            )
            proxy.start()
            awaitState("both real relay handshakes") {
                proxy.getStats().singleOrNull()?.relayBound == true &&
                    service.getProxyStats().singleOrNull()?.relayBound == true
            }
            gate.dropped.add(1)
            Thread.sleep(4000)
            gate.dropped.add(0)
            awaitState("joiner visible timeout cleanup", 12) {
                val state = MatchmakingStateHolder.state.value
                state.currentLobby == null && state.errorMessage?.contains("within 10 seconds") == true &&
                    service.currentProxy() == null
            }
            awaitState("actual departure update removes host peer") {
                roster.get().getJSONArray("players").length() == 1 && proxy.getStats().isEmpty() &&
                    status().getInt("relay_sessions") == 0
            }
            DatagramSocket(localPort, loopback).use { assertTrue(it.isBound) }
            Thread.sleep(7000) // Past the host's later acknowledgement deadline
            assertFalse("Removed peer watchdog destroyed the host", hostFailure.isDone)
            assertEquals(room, roster.get().getString("lobby_id"))
            assertEquals(1, status().getInt("rooms"))
            assertEquals(0, status().getInt("relay_sessions"))
            assertEquals(2, status().getInt("pending_and_authenticated_sockets"))
            assertTrue(proxy.getStats().isEmpty())
            assertNull("Owned UDP gate failed", gate.failure.get())
            println(
                "PASS: actual joiner timeout/LEAVE_LOBBY and server roster update retire peer routes while the host-only room survives",
            )
        } finally {
            service.disconnect()
            hostProxy.get()?.shutdown()
            scope.cancel()
            socket?.cancel()
            gate.close()
            client.dispatcher.executorService.shutdown()
            client.connectionPool.evictAll()
            process.destroy()
            if (!process.waitFor(3, TimeUnit.SECONDS)) {
                process.destroyForcibly()
                process.waitFor()
            }
            directory.deleteRecursively()
            runBlocking { kotlinx.coroutines.withTimeout(2000) { owner.join() } }
            awaitState("owned forwarding workers released", 2) { LocalhostProxy.activeWorkerCount() == 0 }
        }
    }
}
