package com.dxxredux.app.multiplayer

import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.DataOutputStream
import java.io.File
import java.math.BigInteger
import java.net.ServerSocket
import java.nio.file.Files
import java.security.AlgorithmParameters
import java.security.KeyFactory
import java.security.KeyPair
import java.security.MessageDigest
import java.security.Signature
import java.security.spec.ECGenParameterSpec
import java.security.spec.ECParameterSpec
import java.security.spec.ECPrivateKeySpec
import java.security.spec.ECPublicKeySpec

class OnlineInstallationAuthTest {
    @Test fun workDeadlineIsBoundedAndFailureDiagnosticsExcludeInput() =
        runBlocking {
            var clock = 0L
            val producer =
                OnlineInstallationAuth(fixtureKey(), "Fixture", "https://fixture.invalid:443", "test") {
                    clock.also { clock += 12_000_000_000L }
                }
            var measurement: OnlineInstallationAuth.WorkMeasurement? = null
            try {
                producer.proof("ab".repeat(32), 20) { measurement = it }
                fail("Expired work budget accepted")
            } catch (e: OnlineInstallationAuth.WorkDeadlineExceeded) {
                assertEquals("WORK_DEADLINE", OnlineInstallationAuth.failureCode(e))
                assertEquals(20, measurement!!.difficulty)
                assertEquals(0L, measurement!!.hashes)
                assertEquals(false, measurement!!.solved)
            }
            assertEquals(
                "INVALID_CHALLENGE",
                OnlineInstallationAuth.failureCode(IllegalArgumentException("secret-canary")),
            )
            assertEquals("PROOF_PROVIDER_FAILURE", OnlineInstallationAuth.failureCode(Exception("secret-canary")))
            // Check both a late hash and a signature that finishes after the deadline
            for (expiredRead in listOf(3, 4)) {
                var checks = 0
                val suspended =
                    OnlineInstallationAuth(fixtureKey(), "Fixture", "https://fixture.invalid:443", "test") {
                        if (++checks >= expiredRead) 12_000_000_000L else 0L
                    }
                try {
                    suspended.proof("ab".repeat(32), 0)
                    fail("Solution completed after the work deadline")
                } catch (_: OnlineInstallationAuth.WorkDeadlineExceeded) {
                    // Even the first valid hash cannot bypass an elapsed deadline
                }
            }

            for ((error, expected) in listOf(
                java.security.NoSuchAlgorithmException("secret-canary") to "ALGORITHM_UNAVAILABLE",
                java.security.InvalidKeyException("secret-canary") to "SIGNING_KEY_REJECTED",
                java.security.SignatureException("secret-canary") to "SIGNATURE_FAILURE",
            )) {
                assertEquals(expected, OnlineInstallationAuth.failureCode(error))
            }
        }

    private fun fixtureKey(): KeyPair {
        val params =
            AlgorithmParameters
                .getInstance("EC")
                .apply {
                    init(ECGenParameterSpec("secp256r1"))
                }.getParameterSpec(ECParameterSpec::class.java)
        val factory = KeyFactory.getInstance("EC")
        return KeyPair(
            factory.generatePublic(ECPublicKeySpec(params.generator, params)),
            factory.generatePrivate(ECPrivateKeySpec(BigInteger.ONE, params)),
        )
    }

    @Test fun realProducerSignsBoundTranscriptAndHonorsWorkBounds() =
        runBlocking {
            val pair = fixtureKey()
            val producer = OnlineInstallationAuth(pair, "Fixture", "https://fixture.invalid:443", "test")
            val request = producer.request()
            val challenge = "ab".repeat(32)
            var measurement: OnlineInstallationAuth.WorkMeasurement? = null
            val proof = producer.proof(challenge, 8) { measurement = it }
            assertEquals(proof.solution.toLong() + 1, measurement!!.hashes)
            assertEquals(8, measurement!!.difficulty)
            assertTrue(measurement!!.solved)
            assertTrue(measurement!!.elapsedMs >= 0)
            val transcript =
                "dxx-online-p256-v1\n${NetworkConstants.PROTOCOL_VERSION}\n$challenge\n${request.publicKey}\n" +
                    "Fixture\ntest\nhttps://fixture.invalid:443\n${proof.solution}"
            val signature =
                proof.signature
                    .chunked(2)
                    .map { it.toInt(16).toByte() }
                    .toByteArray()
            assertTrue(
                Signature
                    .getInstance("SHA256withECDSA")
                    .apply {
                        initVerify(pair.public)
                        update(transcript.toByteArray())
                    }.verify(signature),
            )
            assertEquals(
                0,
                MessageDigest.getInstance("SHA-256").digest((challenge + proof.solution).toByteArray())[0].toInt(),
            )
            val cancelled = launch(start = CoroutineStart.UNDISPATCHED) { producer.proof(challenge, 22) }
            cancelled.cancelAndJoin()
            assertTrue(cancelled.isCancelled)
            for (difficulty in listOf(-1, 23, 255)) {
                try {
                    producer.proof(challenge, difficulty)
                    fail("Unbounded challenge accepted")
                } catch (
                    _: IllegalArgumentException,
                ) {
                }
            }
        }

    @Test fun deterministicExpensiveChallengePreservesProductionWorkAndMeasurement() =
        runBlocking {
            val producer = OnlineInstallationAuth(fixtureKey(), "Fixture", "https://fixture.invalid:443", "test")
            var measurement: OnlineInstallationAuth.WorkMeasurement? = null
            // Synthetic fixed vector: first qualifying decimal solution requires over four million hashes
            val proof =
                producer.proof("8c5bab31288aa9d968e3a9103eaccddee759a109b77346b33cbd65b03d3c90a2", 20) {
                    measurement = it
                }
            assertEquals("4243437", proof.solution)
            assertEquals(4_243_438L, measurement!!.hashes)
            assertEquals(20, measurement!!.difficulty)
            assertTrue(measurement!!.solved)
            println("WORK_FIXTURE hashes=${measurement!!.hashes} elapsed_ms=${measurement!!.elapsedMs}")
            com.dxxredux.app.OnlineInstallationWorkChecks.run { line ->
                assertTrue(
                    line.matches(
                        Regex(
                            "ONLINE_WORK_VECTOR difficulty=20 hashes=4243438 elapsed_ms=[0-9]+ solved=true|ONLINE_WORK_(EXHAUSTED|CANCELLED) hashes=512 solved=false",
                        ),
                    ),
                )
                println(line)
            }
        }

    @Test fun actualServiceAuthenticatesAndReconnectsToLoopbackDevServer() {
        val binary = System.getProperty("dxx.onlineServerBinary")
        assumeTrue("Set onlineServerBinary to run the real server/service integration", binary != null)
        val directory = Files.createTempDirectory("online-installation-test").toFile()
        val wsPort = ServerSocket(0).use { it.localPort }
        val httpPort = ServerSocket(0).use { it.localPort }
        val audience = "http://127.0.0.1:$wsPort"
        val key = fixtureKey()
        val origin =
            MessageDigest.getInstance("SHA-256").digest(audience.toByteArray()).joinToString("") {
                "%02x".format(it.toInt() and 255)
            }
        val identityFile = File(directory, "online_identity/$origin/identity.bin")
        identityFile.parentFile.mkdirs()
        DataOutputStream(identityFile.outputStream()).use { output ->
            output.writeInt(1)
            output.writeLong(0)
            for (part in listOf(key.public.encoded, key.private.encoded)) {
                output.writeInt(part.size)
                output.write(part)
            }
        }
        val configuration = File(directory, "config.json")
        configuration.writeText(
            """{"ws_listen_addr":"127.0.0.1:$wsPort","http_listen_addr":"127.0.0.1:$httpPort","relay_listen_addr":"127.0.0.1:0","db_path":":memory:","skip_gpgs_verify":true,"pow_difficulty":8,"installation_auth_audience":"$audience"}""",
        )
        val process =
            ProcessBuilder(binary)
                .directory(directory)
                .apply {
                    // Winsock provider initialization requires SystemRoot on Windows
                    val systemRoot = environment()["SystemRoot"]
                    environment().clear()
                    if (systemRoot != null) environment()["SystemRoot"] = systemRoot
                    environment()["CONFIG_FILE"] = configuration.absolutePath
                    redirectOutput(ProcessBuilder.Redirect.DISCARD)
                    redirectError(ProcessBuilder.Redirect.DISCARD)
                }.start()
        val activity =
            object : android.app.Activity() {
                override val noBackupFilesDir = directory
            }

        fun waitUntil(predicate: () -> Boolean) {
            val deadline = System.nanoTime() + 8_000_000_000L
            while (!predicate()) {
                assertTrue("Owned service deadline exceeded", System.nanoTime() < deadline)
                Thread.sleep(20)
            }
        }
        try {
            waitUntil {
                check(process.isAlive) { "Owned server exited" }
                runCatching { java.net.Socket("127.0.0.1", wsPort).use { true } }.getOrDefault(false)
            }
            val service = MatchmakingService
            service.setActivity(activity)
            var previous: String? = null
            repeat(2) {
                service.connect("ws://127.0.0.1:$wsPort/ws", "Fixture")
                waitUntil { MatchmakingStateHolder.state.value.status == ConnectionStatus.CONNECTED }
                val id = MatchmakingStateHolder.state.value.playerId
                assertNotNull(id)
                if (previous != null) assertEquals(previous, id)
                previous = id
                service.disconnect()
                waitUntil {
                    val status =
                        java.net
                            .URI("http://127.0.0.1:$httpPort/api/v1/ready")
                            .toURL()
                            .openConnection()
                            .apply {
                                connectTimeout =
                                    1000
                                ; readTimeout = 1000
                            }.getInputStream()
                            .bufferedReader()
                            .use { it.readText() }
                    status.contains("\"pending_and_authenticated_sockets\":0") && status.contains("\"rooms\":0") &&
                        status.contains("\"relay_sessions\":0")
                }
            }
            val client = okhttp3.OkHttpClient()
            val owners = mutableListOf<okhttp3.WebSocket>()
            try {
                repeat(17) { index ->
                    val messages = java.util.concurrent.LinkedBlockingQueue<String>()
                    val socket =
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
                                    messages.offer(text)
                                }
                            },
                        )
                    owners += socket

                    fun receive(): org.json.JSONObject =
                        org.json.JSONObject(checkNotNull(messages.poll(8, java.util.concurrent.TimeUnit.SECONDS)))
                    val pair =
                        java.security.KeyPairGenerator
                            .getInstance("EC")
                            .apply {
                                initialize(ECGenParameterSpec("secp256r1"))
                            }.generateKeyPair()
                    val producer = OnlineInstallationAuth(pair, "Page$index", audience, "test")
                    socket.send(protocolJson.encodeToString(AuthenticateMsg.serializer(), producer.request()))
                    val challenge = receive()
                    assertEquals("INSTALLATION_CHALLENGE", challenge.getString("type"))
                    val proof =
                        runBlocking { producer.proof(challenge.getString("challenge"), challenge.getInt("difficulty")) }
                    socket.send(protocolJson.encodeToString(InstallationProofMsg.serializer(), proof))
                    for (kind in listOf(
                        "AUTH_OK",
                        "SERVER_STATUS",
                        "LOBBY_LIST",
                        "FRIEND_LIST_RESP",
                    )) {
                        assertEquals(kind, receive().getString("type"))
                    }
                    socket.send(
                        org.json
                            .JSONObject()
                            .put(
                                "type",
                                "CREATE_LOBBY",
                            ).put(
                                "game",
                                "d1",
                            ).put(
                                "max_players",
                                8,
                            ).put("game_info", org.json.JSONObject().put("padding", "x".repeat(5000)))
                            .toString(),
                    )
                    assertEquals("LOBBY_UPDATE", receive().getString("type"))
                }
                service.connect("ws://127.0.0.1:$wsPort/ws", "Fixture")
                waitUntil {
                    MatchmakingStateHolder.state.value.status == ConnectionStatus.CONNECTED &&
                        MatchmakingStateHolder.state.value.lobbies.size == 17
                }
                assertEquals(
                    17,
                    MatchmakingStateHolder.state.value.lobbies
                        .map { it.lobbyId }
                        .toSet()
                        .size,
                )
                assertTrue(
                    MatchmakingStateHolder.state.value.lobbies
                        .all { it.gameInfo.toString().contains("x".repeat(5000)) },
                )
            } finally {
                service.disconnect()
                owners.forEach { it.cancel() }
                client.dispatcher.executorService.shutdown()
                client.connectionPool.evictAll()
            }
            waitUntil {
                val status =
                    java.net
                        .URI("http://127.0.0.1:$httpPort/api/v1/ready")
                        .toURL()
                        .openConnection()
                        .apply {
                            connectTimeout = 1000
                            readTimeout = 1000
                        }.getInputStream()
                        .bufferedReader()
                        .use { it.readText() }
                status.contains("\"pending_and_authenticated_sockets\":0") &&
                    status.contains("\"rooms\":0") && status.contains("\"relay_sessions\":0")
            }
            waitUntil {
                val resources = service.resourceDiagnostics()
                resources["background_jobs"] == 0 && resources["proxy_workers"] == 0 &&
                    resources["candidate_socket"] == false
            }
        } finally {
            MatchmakingService.disconnect()
            process.destroy()
            if (!process.waitFor(3, java.util.concurrent.TimeUnit.SECONDS)) {
                process.destroyForcibly()
                process.waitFor()
            }
            directory.deleteRecursively()
        }
    }
}
