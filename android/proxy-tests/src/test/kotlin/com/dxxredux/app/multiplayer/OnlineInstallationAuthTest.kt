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
            val proof = producer.proof(challenge, 8)
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

    @Test fun actualServiceAuthenticatesAndReconnectsToProductionServer() {
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
            """{"ws_listen_addr":"127.0.0.1:$wsPort","http_listen_addr":"127.0.0.1:$httpPort","relay_listen_addr":"127.0.0.1:0","db_path":":memory:","skip_gpgs_verify":false,"pow_difficulty":8,"installation_auth_audience":"$audience"}""",
        )
        val process =
            ProcessBuilder(binary)
                .directory(directory)
                .apply {
                    environment().clear()
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
