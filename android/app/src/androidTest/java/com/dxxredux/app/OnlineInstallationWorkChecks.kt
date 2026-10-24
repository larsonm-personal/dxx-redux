package com.dxxredux.app

import com.dxxredux.app.multiplayer.NetworkConstants
import com.dxxredux.app.multiplayer.OnlineInstallationAuth
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.security.KeyPairGenerator
import java.security.Signature
import java.security.spec.ECGenParameterSpec

/** Synthetic work vector on the actual Android provider, without network or persisted identity */
internal object OnlineInstallationWorkChecks {
    fun run(report: (String) -> Unit) =
        runBlocking {
            val pair =
                KeyPairGenerator
                    .getInstance("EC")
                    .apply {
                        initialize(ECGenParameterSpec("secp256r1"))
                    }.generateKeyPair()
            val producer = OnlineInstallationAuth(pair, "Fixture", "https://fixture.invalid:443", "test")
            val challenge = "8c5bab31288aa9d968e3a9103eaccddee759a109b77346b33cbd65b03d3c90a2"
            val proof =
                try {
                    producer.proof(challenge, 20) { work ->
                        report(
                            "ONLINE_WORK_VECTOR difficulty=${work.difficulty} hashes=${work.hashes} " +
                                "elapsed_ms=${work.elapsedMs} solved=${work.solved}",
                        )
                    }
                } catch (e: Exception) {
                    error("Installation work fixture failed: ${OnlineInstallationAuth.failureCode(e)}")
                }
            check(proof.solution == "4243437") { "Installation work fixture solution mismatch" }
            val transcript =
                "dxx-online-p256-v1\n${NetworkConstants.PROTOCOL_VERSION}\n$challenge\n" +
                    "${producer.request().publicKey}\n" +
                    "Fixture\ntest\nhttps://fixture.invalid:443\n${proof.solution}"
            val signature =
                proof.signature
                    .chunked(2)
                    .map { it.toInt(16).toByte() }
                    .toByteArray()
            check(
                Signature
                    .getInstance("SHA256withECDSA")
                    .apply {
                        initVerify(pair.public)
                        update(transcript.toByteArray(Charsets.UTF_8))
                    }.verify(signature),
            ) { "Installation work fixture signature mismatch" }
            // Deterministic exhaustion after real hashes, independent of device speed or random luck
            var clockReads = 0
            var exhaustedWork: OnlineInstallationAuth.WorkMeasurement? = null
            val exhausted =
                OnlineInstallationAuth(pair, "Fixture", "https://fixture.invalid:443", "test") {
                    if (++clockReads >= 3) 12_000_000_000L else 0L
                }
            try {
                exhausted.proof(challenge, 20) { exhaustedWork = it }
                error("Unsolved installation work was accepted")
            } catch (_: OnlineInstallationAuth.WorkDeadlineExceeded) {
                check(exhaustedWork?.hashes == 512L && exhaustedWork.solved == false)
                report("ONLINE_WORK_EXHAUSTED hashes=512 solved=false")
            }
            var cancellationReads = 0
            var cancelledWork: OnlineInstallationAuth.WorkMeasurement? = null
            lateinit var active: kotlinx.coroutines.Job
            val interrupted =
                OnlineInstallationAuth(pair, "Fixture", "https://fixture.invalid:443", "test") {
                    if (++cancellationReads == 3) active.cancel()
                    0L
                }
            active =
                launch(start = CoroutineStart.LAZY) {
                    interrupted.proof(challenge, 20) { cancelledWork = it }
                    error("Cancelled installation proof returned")
                }
            active.start()
            active.join()
            check(active.isCancelled && cancelledWork?.hashes == 512L && cancelledWork.solved == false)
            report("ONLINE_WORK_CANCELLED hashes=512 solved=false")
            val cancelled = launch(start = CoroutineStart.UNDISPATCHED) { producer.proof(challenge, 22) }
            cancelled.cancelAndJoin()
            check(cancelled.isCancelled) { "Installation work ignored cancellation" }
        }
}
