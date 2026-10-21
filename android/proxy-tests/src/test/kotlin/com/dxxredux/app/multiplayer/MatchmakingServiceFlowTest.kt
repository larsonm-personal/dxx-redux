package com.dxxredux.app.multiplayer

import okhttp3.Request
import okhttp3.WebSocket
import okio.ByteString
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Runs the actual service's completed-join consumer; Android storage/GPGS/IPC are not simulated */
class MatchmakingServiceFlowTest {
    @Test(timeout = 10000)
    fun staleInstallationCallbacksCannotSubmitOrRetryOnReplacement() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        val sent = java.util.concurrent.CopyOnWriteArrayList<String>()

        fun socket() =
            object : WebSocket {
                override fun request() = Request.Builder().url("http://127.0.0.1/ws").build()

                override fun queueSize() = 0L

                override fun send(text: String): Boolean {
                    sent += text
                    return true
                }

                override fun send(bytes: ByteString) = false

                override fun close(
                    code: Int,
                    reason: String?,
                ) = true

                override fun cancel() = Unit
            }
        val oldSocket = socket()
        val freshSocket = socket()
        val listener =
            service.javaClass.declaredClasses
                .first { it.simpleName == "Listener" }
                .getDeclaredConstructor(String::class.java)
                .apply { isAccessible = true }
                .newInstance("Fixture") as okhttp3.WebSocketListener
        val entered = java.util.concurrent.CountDownLatch(1)
        val release = java.util.concurrent.CountDownLatch(1)
        val pair =
            java.security.KeyPairGenerator
                .getInstance("EC")
                .apply {
                    initialize(java.security.spec.ECGenParameterSpec("secp256r1"))
                }.generateKeyPair()
        service.disconnect()
        try {
            field("webSocket").set(service, oldSocket)
            field("manualDisconnect").set(service, false)
            var reads = 0
            field("installationAuth").set(
                service,
                OnlineInstallationAuth(pair, "Fixture", "http://127.0.0.1:80", "test") {
                    if (++reads == 3) {
                        entered.countDown()
                        check(release.await(3, java.util.concurrent.TimeUnit.SECONDS))
                    }
                    0L
                },
            )
            listener.onMessage(
                oldSocket,
                """{"type":"INSTALLATION_CHALLENGE","challenge":"${"ab".repeat(32)}","difficulty":0}""",
            )
            assertTrue(entered.await(3, java.util.concurrent.TimeUnit.SECONDS))
            val oldJob = field("installationAuthJob").get(service) as kotlinx.coroutines.Job
            service.disconnect()
            field("webSocket").set(service, freshSocket)
            field("manualDisconnect").set(service, false)
            field(
                "installationAuth",
            ).set(service, OnlineInstallationAuth(pair, "Fixture", "http://127.0.0.1:80", "test"))
            release.countDown()
            kotlinx.coroutines.runBlocking { oldJob.join() }
            assertTrue(sent.isEmpty())
            val freshChallenge = "cd".repeat(32)
            val message = """{"type":"INSTALLATION_CHALLENGE","challenge":"$freshChallenge","difficulty":0}"""
            listener.onMessage(oldSocket, message)
            listener.onFailure(oldSocket, java.io.IOException("stale"), null)
            assertEquals(0, field("installationAttempts").get(service))
            listener.onMessage(freshSocket, message)
            val deadline = System.nanoTime() + 3_000_000_000L
            while (sent.isEmpty()) {
                check(System.nanoTime() < deadline)
                Thread.sleep(5)
            }
            assertEquals(1, sent.size)
            assertTrue(sent.single().contains(freshChallenge))
            assertEquals(1, field("installationAttempts").get(service))
        } finally {
            release.countDown()
            service.disconnect()
        }
    }

    @OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
    @Test(timeout = 15000)
    fun exhaustedProofsRetryFreshSocketsAndStopAtThree() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        val scheduler = kotlinx.coroutines.test.TestCoroutineScheduler()
        val owner = kotlinx.coroutines.SupervisorJob()
        val originalScope = field("scope").get(service)
        service.disconnect()
        field("scope").set(
            service,
            kotlinx.coroutines.CoroutineScope(owner + kotlinx.coroutines.test.StandardTestDispatcher(scheduler)),
        )
        val consume =
            service.javaClass.getDeclaredMethod("handleMessage", String::class.java).apply {
                isAccessible =
                    true
            }
        val pair =
            java.security.KeyPairGenerator
                .getInstance("EC")
                .apply {
                    initialize(java.security.spec.ECGenParameterSpec("secp256r1"))
                }.generateKeyPair()
        try {
            repeat(3) { attempt ->
                var cancelled = false
                val sent = mutableListOf<String>()
                val socket =
                    object : WebSocket {
                        override fun request() = Request.Builder().url("http://127.0.0.1/ws").build()

                        override fun queueSize() = 0L

                        override fun send(text: String): Boolean {
                            sent += text
                            return true
                        }

                        override fun send(bytes: ByteString) = false

                        override fun close(
                            code: Int,
                            reason: String?,
                        ) = true

                        override fun cancel() {
                            cancelled = true
                        }
                    }
                // Simulate the next socket opening without resetting the real service budget
                (field("reconnectJob").get(service) as? kotlinx.coroutines.Job)?.cancel()
                field("webSocket").set(service, socket)
                field("manualDisconnect").set(service, false)
                var clock = 0L
                field("installationAuth").set(
                    service,
                    OnlineInstallationAuth(pair, "Fixture", "http://127.0.0.1:80", "test") {
                        clock.also { clock += 12_000_000_000L }
                    },
                )
                consume.invoke(
                    service,
                    """{"type":"INSTALLATION_CHALLENGE","challenge":"${("a" + attempt).repeat(32)}","difficulty":20}""",
                )
                val deadline = System.nanoTime() + 3_000_000_000L
                while (synchronized(service) { field("webSocket").get(service) != null }) {
                    check(System.nanoTime() < deadline) { "Expired producer did not retire socket" }
                    Thread.sleep(5)
                }
                scheduler.runCurrent()
                assertTrue(cancelled)
                assertTrue("Unsolved proof was submitted", sent.isEmpty())
                assertEquals(attempt + 1, (service.resourceDiagnostics()["installation_work"] as Map<*, *>)["attempt"])
                assertEquals(
                    if (attempt < 2) ConnectionStatus.RECONNECTING else ConnectionStatus.DISCONNECTED,
                    MatchmakingStateHolder.state.value.status,
                )
            }
            assertTrue(
                MatchmakingStateHolder.state.value.errorMessage!!
                    .contains("WORK_DEADLINE"),
            )
            scheduler.advanceTimeBy(300000)
            scheduler.runCurrent()
            assertEquals(null, field("webSocket").get(service))
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
        } finally {
            service.disconnect()
            owner.cancel()
            scheduler.runCurrent()
            field("scope").set(service, originalScope)
        }
    }

    @OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
    @Test
    fun installationOverallDeadlineAndCancellationSurviveSocketReplacement() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        val scheduler = kotlinx.coroutines.test.TestCoroutineScheduler()
        val owner = kotlinx.coroutines.SupervisorJob()
        val originalScope = field("scope").get(service)
        val begin = service.javaClass.getDeclaredMethod("beginInstallationWork").apply { isAccessible = true }
        service.disconnect()
        field("scope").set(
            service,
            kotlinx.coroutines.CoroutineScope(owner + kotlinx.coroutines.test.StandardTestDispatcher(scheduler)),
        )
        try {
            assertEquals(true, begin.invoke(service))
            scheduler.runCurrent()
            scheduler.advanceTimeBy(44000)
            assertEquals(true, begin.invoke(service))
            scheduler.advanceTimeBy(1000)
            scheduler.runCurrent()
            assertTrue(
                MatchmakingStateHolder.state.value.errorMessage!!
                    .contains("TOTAL_DEADLINE"),
            )
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
            assertEquals(true, begin.invoke(service))
            scheduler.runCurrent()
            service.disconnect()
            scheduler.advanceTimeBy(45000)
            scheduler.runCurrent()
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
            assertEquals(0, field("installationAttempts").get(service))
        } finally {
            service.disconnect()
            owner.cancel()
            scheduler.runCurrent()
            field("scope").set(service, originalScope)
        }
    }

    @OptIn(kotlinx.coroutines.ExperimentalCoroutinesApi::class)
    @Test
    fun authOkCannotClearExpiredBudgetBeforeDelayedTimerRuns() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        val scheduler = kotlinx.coroutines.test.TestCoroutineScheduler()
        val owner = kotlinx.coroutines.SupervisorJob()
        val originalScope = field("scope").get(service)
        val begin = service.javaClass.getDeclaredMethod("beginInstallationWork").apply { isAccessible = true }
        val consume =
            service.javaClass.getDeclaredMethod("handleMessage", String::class.java).apply {
                isAccessible =
                    true
            }
        val authOk = """{"type":"AUTH_OK","player_id":"fixture-player","session_token":"fixture-session"}"""
        service.disconnect()
        field("scope").set(
            service,
            kotlinx.coroutines.CoroutineScope(owner + kotlinx.coroutines.test.StandardTestDispatcher(scheduler)),
        )
        try {
            assertEquals(true, begin.invoke(service))
            // Leave the deadline coroutine queued while monotonic time reaches its boundary
            field("installationDeadlineNanos").setLong(service, System.nanoTime())
            consume.invoke(service, authOk)
            assertEquals(ConnectionStatus.DISCONNECTED, MatchmakingStateHolder.state.value.status)
            assertEquals(null, MatchmakingStateHolder.state.value.playerId)
            assertEquals(null, MatchmakingStateHolder.state.value.sessionToken)
            assertTrue(
                MatchmakingStateHolder.state.value.errorMessage!!
                    .contains("TOTAL_DEADLINE"),
            )
            assertEquals(null, field("installationDeadlineJob").get(service))
            scheduler.runCurrent()
            assertEquals(0, service.resourceDiagnostics()["background_jobs"])
            scheduler.advanceTimeBy(45000)
            scheduler.runCurrent()
            assertEquals(ConnectionStatus.DISCONNECTED, MatchmakingStateHolder.state.value.status)

            // A live budget still accepts the same response and cancels its pending timer
            service.disconnect()
            assertEquals(true, begin.invoke(service))
            consume.invoke(service, authOk)
            assertEquals(ConnectionStatus.CONNECTED, MatchmakingStateHolder.state.value.status)
            assertEquals("fixture-player", MatchmakingStateHolder.state.value.playerId)
            assertEquals("fixture-session", MatchmakingStateHolder.state.value.sessionToken)
            assertEquals(0, field("installationAttempts").get(service))
            assertEquals(null, field("installationDeadlineJob").get(service))
            scheduler.advanceTimeBy(45000)
            scheduler.runCurrent()
            assertEquals(ConnectionStatus.CONNECTED, MatchmakingStateHolder.state.value.status)
        } finally {
            service.disconnect()
            owner.cancel()
            scheduler.runCurrent()
            field("scope").set(service, originalScope)
        }
    }

    @Test(timeout = 30000)
    fun actualSocketsRecoverAfterFirstProofExhaustionWithFreshChallenge() {
        val service = MatchmakingService

        fun field(name: String) = service.javaClass.getDeclaredField(name).apply { isAccessible = true }
        val directory =
            java.nio.file.Files
                .createTempDirectory("online-proof-retry")
                .toFile()
        val activity =
            object : android.app.Activity() {
                override val noBackupFilesDir = directory
            }
        val listener = java.net.ServerSocket(0, 2, java.net.InetAddress.getLoopbackAddress())
        listener.soTimeout = 15000
        val failure =
            java.util.concurrent.atomic
                .AtomicReference<Throwable?>()
        val authenticated = java.util.concurrent.CountDownLatch(1)
        val release = java.util.concurrent.CountDownLatch(1)
        val requests = mutableListOf<org.json.JSONObject>()
        val connections = mutableListOf<Any>()
        val challenges = listOf("8c5bab31288aa9d968e3a9103eaccddee759a109b77346b33cbd65b03d3c90a2", "ab".repeat(32))
        val server =
            Thread {
                try {
                    repeat(2) { attempt ->
                        listener.accept().use { socket ->
                            socket.soTimeout = 15000
                            val input = java.io.DataInputStream(socket.getInputStream())
                            val output = java.io.DataOutputStream(socket.getOutputStream())
                            val header = StringBuilder()
                            while (!header.endsWith("\r\n\r\n")) {
                                header.append(input.readUnsignedByte().toChar())
                                check(header.length < 16384)
                            }
                            val key =
                                header
                                    .lines()
                                    .first {
                                        it.startsWith(
                                            "Sec-WebSocket-Key:",
                                            true,
                                        )
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
                            output.write(
                                (
                                    "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n" +
                                        "Connection: Upgrade\r\nSec-WebSocket-Accept: $accept\r\n\r\n"
                                ).toByteArray(),
                            )
                            output.flush()

                            fun receive(): org.json.JSONObject {
                                check(input.readUnsignedByte() and 15 == 1)
                                val lengthByte = input.readUnsignedByte()
                                check(lengthByte and 128 != 0)
                                val length =
                                    if (lengthByte and 127 ==
                                        126
                                    ) {
                                        input.readUnsignedShort()
                                    } else {
                                        lengthByte and 127
                                    }
                                check(length in 1..16384)
                                val mask = ByteArray(4).also(input::readFully)
                                val payload = ByteArray(length).also(input::readFully)
                                payload.indices.forEach {
                                    payload[it] =
                                        (payload[it].toInt() xor mask[it % 4].toInt()).toByte()
                                }
                                return org.json.JSONObject(payload.toString(Charsets.UTF_8))
                            }

                            fun send(text: String) {
                                val bytes = text.toByteArray()
                                output.writeByte(129)
                                if (bytes.size < 126) {
                                    output.writeByte(bytes.size)
                                } else {
                                    output.writeByte(126)
                                    output.writeShort(bytes.size)
                                }
                                output.write(bytes)
                                output.flush()
                            }
                            val request = receive()
                            assertEquals("AUTHENTICATE", request.getString("type"))
                            requests += request
                            synchronized(service) {
                                connections += checkNotNull(field("webSocket").get(service))
                                if (attempt == 0) {
                                    // Alter only this real producer's clock, after its persisted identity request
                                    val producer = checkNotNull(field("installationAuth").get(service))
                                    var reads = 0
                                    producer.javaClass.getDeclaredField("nanoTime").apply { isAccessible = true }.set(
                                        producer,
                                        { if (++reads >= 3) 12_000_000_000L else 0L },
                                    )
                                }
                            }
                            send(
                                """{"type":"INSTALLATION_CHALLENGE","challenge":"${challenges[attempt]}",""" +
                                    """"difficulty":20}""",
                            )
                            if (attempt == 0) {
                                // No manual socket substitution: the actual failure must cancel and reconnect
                                assertEquals(
                                    "Exhausted proof sent data instead of retiring its socket",
                                    -1,
                                    input.read(),
                                )
                                val work = service.resourceDiagnostics()["installation_work"] as Map<*, *>
                                assertEquals(512L, work["hashes"])
                                assertEquals(false, work["solved"])
                                assertEquals(1, work["attempt"])
                            } else {
                                assertTrue(connections[0] !== connections[1])
                                assertEquals(requests[0].getString("public_key"), request.getString("public_key"))
                                assertEquals(requests[0].getString("auth_audience"), request.getString("auth_audience"))
                                val proof = receive()
                                assertEquals("INSTALLATION_PROOF", proof.getString("type"))
                                assertEquals(challenges[1], proof.getString("challenge"))
                                val solution = proof.getString("solution")
                                val hash =
                                    java.security.MessageDigest.getInstance("SHA-256").digest(
                                        (
                                            challenges[1] +
                                                solution
                                        ).toByteArray(),
                                    )
                                assertEquals(0, hash[0].toInt())
                                assertEquals(0, hash[1].toInt())
                                assertEquals(0, hash[2].toInt() and 240)

                                fun hex(value: String) = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                                val publicBytes = hex(request.getString("public_key"))
                                val params =
                                    java.security.AlgorithmParameters
                                        .getInstance("EC")
                                        .apply {
                                            init(java.security.spec.ECGenParameterSpec("secp256r1"))
                                        }.getParameterSpec(java.security.spec.ECParameterSpec::class.java)
                                val publicKey =
                                    java.security.KeyFactory.getInstance("EC").generatePublic(
                                        java.security.spec.ECPublicKeySpec(
                                            java.security.spec.ECPoint(
                                                java.math.BigInteger(1, publicBytes.copyOfRange(1, 33)),
                                                java.math.BigInteger(1, publicBytes.copyOfRange(33, 65)),
                                            ),
                                            params,
                                        ),
                                    )
                                val transcript =
                                    listOf(
                                        "dxx-online-p256-v1",
                                        NetworkConstants.PROTOCOL_VERSION.toString(),
                                        challenges[1],
                                        request.getString("public_key"),
                                        request.getString("callsign"),
                                        request.getString("platform"),
                                        request.getString("auth_audience"),
                                        solution,
                                    ).joinToString("\n")
                                assertTrue(
                                    java.security.Signature
                                        .getInstance("SHA256withECDSA")
                                        .apply {
                                            initVerify(publicKey)
                                            update(transcript.toByteArray())
                                        }.verify(hex(proof.getString("signature"))),
                                )
                                val work = service.resourceDiagnostics()["installation_work"] as Map<*, *>
                                assertEquals(2, work["attempt"])
                                assertEquals(20, work["difficulty"])
                                assertEquals(true, work["solved"])
                                send(
                                    """{"type":"AUTH_OK","player_id":"retry-fixture",""" +
                                        """"session_token":"retry-session"}""",
                                )
                                authenticated.countDown()
                                check(release.await(10, java.util.concurrent.TimeUnit.SECONDS))
                            }
                        }
                    }
                } catch (error: Throwable) {
                    failure.set(error)
                    authenticated.countDown()
                }
            }.apply { isDaemon = true }
        service.disconnect()
        try {
            service.setActivity(activity)
            server.start()
            service.connect("ws://127.0.0.1:${listener.localPort}/ws", "RetryFixture")
            assertTrue(
                "Real reconnect did not authenticate",
                authenticated.await(20, java.util.concurrent.TimeUnit.SECONDS),
            )
            failure.get()?.let { throw AssertionError("Owned proof fixture failed", it) }
            val deadline = System.nanoTime() + 3_000_000_000L
            while (MatchmakingStateHolder.state.value.status != ConnectionStatus.CONNECTED) {
                check(System.nanoTime() < deadline)
                Thread.sleep(5)
            }
            assertEquals("retry-fixture", MatchmakingStateHolder.state.value.playerId)
            assertEquals("retry-session", MatchmakingStateHolder.state.value.sessionToken)
            assertEquals(0, field("installationAttempts").get(service))
            assertEquals(null, field("installationDeadlineJob").get(service))
        } finally {
            service.disconnect()
            release.countDown()
            listener.close()
            server.join(2000)
            directory.deleteRecursively()
        }
        failure.get()?.let { throw AssertionError("Owned proof fixture failed", it) }
        assertEquals(0, service.resourceDiagnostics()["background_jobs"])
    }

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
