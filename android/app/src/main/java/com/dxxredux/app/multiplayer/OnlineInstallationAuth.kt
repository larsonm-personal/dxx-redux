package com.dxxredux.app.multiplayer

import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.yield
import java.io.File
import java.security.InvalidKeyException
import java.security.KeyPair
import java.security.MessageDigest
import java.security.NoSuchAlgorithmException
import java.security.Signature
import java.security.SignatureException
import java.security.interfaces.ECPublicKey
import kotlin.coroutines.coroutineContext

/** Per-origin installation identity; keys remain in private, non-backed-up app storage */
internal class OnlineInstallationAuth(
    private val pair: KeyPair,
    private val callsign: String,
    private val audience: String,
    private val platform: String = NetworkConstants.PLATFORM,
    private val nanoTime: () -> Long = System::nanoTime,
) {
    internal class WorkDeadlineExceeded : IllegalStateException("Installation work deadline exceeded")

    private val publicKey: String =
        (pair.public as ECPublicKey).let { key ->
            fun coordinate(value: java.math.BigInteger): ByteArray {
                val bytes = value.toByteArray()
                require(bytes.size <= 33)
                return if (bytes.size > 32) bytes.copyOfRange(1, 33) else ByteArray(32 - bytes.size) + bytes
            }
            (byteArrayOf(4) + coordinate(key.w.affineX) + coordinate(key.w.affineY)).hex()
        }

    fun request() =
        AuthenticateMsg(
            callsign = callsign,
            platform = platform,
            authMethod = "p256_challenge",
            publicKey = publicKey,
            authAudience = audience,
        )

    data class WorkMeasurement(
        val difficulty: Int,
        val hashes: Long,
        val elapsedMs: Long,
        val solved: Boolean,
    )

    suspend fun proof(
        challenge: String,
        difficulty: Int,
        measure: (WorkMeasurement) -> Unit = {},
    ): InstallationProofMsg {
        require(challenge.matches(Regex("[0-9a-f]{64}")) && difficulty in 0..22) { "Invalid installation challenge" }
        val started = nanoTime()
        val digest = MessageDigest.getInstance("SHA-256")
        val prefix = challenge.toByteArray(Charsets.UTF_8)
        var nonce = 0L
        var hashes = 0L
        var solved = false
        try {
            while (true) {
                if (nonce % 512 == 0L) {
                    coroutineContext.ensureActive()
                    if (nanoTime() - started >= 12_000_000_000L) throw WorkDeadlineExceeded()
                    yield()
                }
                val solution = nonce.toString()
                digest.update(prefix)
                val hash = digest.digest(solution.toByteArray(Charsets.UTF_8))
                hashes++
                var bits = 0
                for (byte in hash) {
                    val value = byte.toInt() and 255
                    if (value == 0) {
                        bits += 8
                    } else {
                        bits += Integer.numberOfLeadingZeros(value) - 24
                        break
                    }
                }
                if (bits >= difficulty) {
                    solved = true
                    // Keep this canonical transcript synchronized with server/src/pow.rs
                    val transcript =
                        "dxx-online-p256-v1\n${NetworkConstants.PROTOCOL_VERSION}\n$challenge\n$publicKey\n" +
                            "$callsign\n$platform\n$audience\n$solution"
                    val signature =
                        Signature
                            .getInstance("SHA256withECDSA")
                            .apply {
                                initSign(pair.private)
                                update(transcript.toByteArray(Charsets.UTF_8))
                            }.sign()
                    return InstallationProofMsg(challenge = challenge, solution = solution, signature = signature.hex())
                }
                nonce++
            }
        } finally {
            measure(WorkMeasurement(difficulty, hashes, (nanoTime() - started) / 1_000_000L, solved))
        }
    }

    companion object {
        // Fixed categories only: provider exceptions may contain sensitive input
        fun failureCode(error: Exception): String =
            when (error) {
                is WorkDeadlineExceeded -> "WORK_DEADLINE"
                is IllegalArgumentException -> "INVALID_CHALLENGE"
                is NoSuchAlgorithmException -> "ALGORITHM_UNAVAILABLE"
                is InvalidKeyException -> "SIGNING_KEY_REJECTED"
                is SignatureException -> "SIGNATURE_FAILURE"
                else -> "PROOF_PROVIDER_FAILURE"
            }

        fun fromStorage(
            directory: File,
            callsign: String,
            audience: String,
        ): OnlineInstallationAuth {
            val originId = MessageDigest.getInstance("SHA-256").digest(audience.toByteArray(Charsets.UTF_8)).hex()
            val pair = UdpReconnectStore(File(directory, "online_identity/$originId")).keyPair()
            return OnlineInstallationAuth(pair, callsign.trim(), audience)
        }

        private fun ByteArray.hex() = joinToString("") { "%02x".format(it.toInt() and 255) }
    }
}
