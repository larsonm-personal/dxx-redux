package com.dxxredux.app.multiplayer

import okhttp3.Request
import okhttp3.WebSocket
import okio.ByteString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Runs the actual service's completed-join consumer; Android storage/GPGS/IPC are not simulated */
class MatchmakingServiceFlowTest {
    @OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
    @Test(timeout = 30000)
    fun repeatedUpgradeWithoutAuthenticationExhaustsReconnectBudget() {
        val scheduler = kotlinx.coroutines.test.TestCoroutineScheduler()
        val owner = kotlinx.coroutines.SupervisorJob()
        val replacement =
            kotlinx.coroutines.CoroutineScope(owner + kotlinx.coroutines.test.StandardTestDispatcher(scheduler))
        val service = MatchmakingService
        val scopeField = service.javaClass.getDeclaredField("scope").apply { isAccessible = true }
        val originalScope = scopeField.get(service)
        val directory =
            java.nio.file.Files
                .createTempDirectory("online-retry-test")
                .toFile()
        val activity =
            object : android.app.Activity() {
                override val noBackupFilesDir = directory
            }
        val accepted =
            java.util.concurrent.atomic
                .AtomicInteger()
        val failure =
            java.util.concurrent.atomic
                .AtomicReference<Throwable?>()
        val stop =
            java.util.concurrent.atomic
                .AtomicBoolean()
        val listener = java.net.ServerSocket(0, 16, java.net.InetAddress.getLoopbackAddress())
        listener.soTimeout = 500
        val server =
            Thread {
                try {
                    while (!stop.get()) {
                        val socket =
                            try {
                                listener.accept()
                            } catch (_: java.net.SocketTimeoutException) {
                                continue
                            }
                        socket.use {
                            it.soTimeout = 2000
                            val input = java.io.DataInputStream(it.getInputStream())
                            val header = StringBuilder()
                            while (!header.endsWith("\r\n\r\n")) {
                                header.append(input.readUnsignedByte().toChar())
                                check(header.length < 16384)
                            }
                            val key =
                                header
                                    .lines()
                                    .first { line ->
                                        line.startsWith("Sec-WebSocket-Key:", true)
                                    }.substringAfter(':')
                                    .trim()
                            val accept =
                                java.util.Base64.getEncoder().encodeToString(
                                    java.security.MessageDigest.getInstance("SHA-1").digest(
                                        (
                                            key +
                                                "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
                                        ).toByteArray(),
                                    ),
                                )
                            it.getOutputStream().write(
                                (
                                    "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n" +
                                        "Connection: Upgrade\r\nSec-WebSocket-Accept: $accept\r\n\r\n"
                                ).toByteArray(),
                            )
                            it.getOutputStream().flush()
                            check(input.readUnsignedByte() and 15 == 1)
                            val lengthByte = input.readUnsignedByte()
                            check(lengthByte and 128 != 0)
                            val shortLength = lengthByte and 127
                            val length = if (shortLength == 126) input.readUnsignedShort() else shortLength
                            check(length in 1..16384)
                            val mask = ByteArray(4).also { bytes -> input.readFully(bytes) }
                            val payload = ByteArray(length).also { bytes -> input.readFully(bytes) }
                            payload.indices.forEach { i ->
                                payload[i] =
                                    (payload[i].toInt() xor mask[i % 4].toInt()).toByte()
                            }
                            check(payload.toString(Charsets.UTF_8).contains("AUTHENTICATE"))
                            accepted.incrementAndGet()
                            // Close after actual onOpen/auth request, without ever issuing AUTH_OK
                        }
                    }
                } catch (error: Throwable) {
                    if (!stop.get()) failure.set(error)
                }
            }.apply {
                isDaemon = true
                start()
            }
        service.disconnect()
        scopeField.set(service, replacement)
        try {
            service.setActivity(activity)
            service.connect("ws://127.0.0.1:${listener.localPort}/ws", "RetryFixture")
            val deadline = System.nanoTime() + 20_000_000_000L
            while (MatchmakingStateHolder.state.value.errorMessage != "Connection lost after 15 attempts") {
                failure.get()?.let { throw AssertionError("Owned WebSocket fixture failed", it) }
                assertTrue(
                    "Unauthenticated upgrades did not reach bounded terminal state",
                    System.nanoTime() < deadline,
                )
                scheduler.advanceTimeBy(30000)
                scheduler.runCurrent()
                Thread.sleep(10)
            }
            assertEquals(16, accepted.get())
            assertEquals(ConnectionStatus.DISCONNECTED, MatchmakingStateHolder.state.value.status)
            scheduler.advanceTimeBy(300000)
            scheduler.runCurrent()
            Thread.sleep(100)
            assertEquals("Terminal retry state opened another connection", 16, accepted.get())
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
        } finally {
            service.disconnect()
            owner.cancel()
            scheduler.runCurrent()
            scopeField.set(service, originalScope)
            stop.set(true)
            listener.close()
            server.join(3000)
            assertTrue("Owned socket worker leaked", !server.isAlive)
            directory.deleteRecursively()
        }
    }

    @Test fun acceptedFriendJoinToCodedRoomDoesNotIssueSecondJoin() {
        val sent = mutableListOf<String>()
        val socket =
            object : WebSocket {
                override fun request() = Request.Builder().url("http://127.0.0.1/ws").build()

                override fun queueSize() = 0L

                override fun send(text: String): Boolean {
                    sent.add(text)
                    return true
                }

                override fun send(bytes: ByteString) = error("Unexpected binary message")

                override fun close(
                    code: Int,
                    reason: String?,
                ) = true

                override fun cancel() = Unit
            }
        val service = MatchmakingService
        val type = service.javaClass

        fun field(
            name: String,
            value: Any?,
        ) {
            type.getDeclaredField(name).apply { isAccessible = true }.set(service, value)
        }
        val listenerType = type.declaredClasses.first { it.simpleName == "Listener" }
        val listener =
            listenerType
                .getDeclaredConstructor(String::class.java)
                .apply { isAccessible = true }
                .newInstance("Joiner") as okhttp3.WebSocketListener

        fun receive(body: String) {
            listener.onMessage(socket, body)
        }
        service.disconnect()
        try {
            field("webSocket", socket)
            field("manualDisconnect", false)
            // Keep this consumer test from launching independent STUN/UPnP work
            field("stunCompleted", true)
            MatchmakingStateHolder.update { it.copy(playerId = "joiner", status = ConnectionStatus.CONNECTED) }
            receive(
                """{"type":"LOBBY_LIST","lobbies":[{"lobby_id":"coded-room","host_callsign":"Host",
                "game":"d2","player_count":1,"max_players":3,"joinable":true,"has_code":true}]}""",
            )
            assertTrue(
                MatchmakingStateHolder.state.value.lobbies
                    .single()
                    .hasCode,
            )
            service.joinFriendGame("accepted-friend")
            receive("""{"type":"JOIN_FRIEND_GAME_RESP","success":true,"lobby_id":"coded-room"}""")
            receive(
                """{"type":"LOBBY_UPDATE","lobby_id":"coded-room","players":[
                {"player_id":"accepted-friend","slot":0,"callsign":"Host","ready":false},
                {"player_id":"joiner","slot":2,"callsign":"Joiner","ready":false}]}""",
            )
            assertEquals(
                "coded-room",
                MatchmakingStateHolder.state.value.currentLobby
                    ?.lobbyId,
            )
            assertEquals(
                listOf(0, 2),
                MatchmakingStateHolder.state.value.currentLobby
                    ?.players
                    ?.map { it.slot },
            )
            assertTrue(sent.any { it.contains("JOIN_FRIEND_GAME") })
            assertTrue("Completed friend join sent another code-less join", sent.none { it.contains("JOIN_LOBBY") })
            // Runtime callbacks carry the launch-owned token, never a fresh lookup
            field("runtimeAttempt", "replacement-attempt")
            service.runtimeGameProcessDisconnected("old-attempt", false)
            assertTrue(sent.none { it.contains("LEAVE_LOBBY") })
            assertEquals(
                "coded-room",
                MatchmakingStateHolder.state.value.currentLobby
                    ?.lobbyId,
            )
            service.runtimeGameProcessDisconnected("replacement-attempt", false)
            service.runtimeGameProcessDisconnected("replacement-attempt", false)
            assertEquals(1, sent.count { it.contains("LEAVE_LOBBY") })
            assertEquals(null, MatchmakingStateHolder.state.value.currentLobby)
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
        } finally {
            service.disconnect()
        }
    }
}
