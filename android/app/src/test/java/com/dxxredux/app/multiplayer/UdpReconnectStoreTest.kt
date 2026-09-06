package com.dxxredux.app.multiplayer

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.security.Signature

class UdpReconnectStoreTest {
    @get:Rule val temporary = TemporaryFolder()

    // Windows File.renameTo cannot replace an existing file; emulate Android's atomic rename
    private fun store(directory: File) =
        UdpReconnectStore(directory) { source, target ->
            Files.move(
                source.toPath(),
                target.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING,
            )
        }

    @Test
    fun restartedClientKeepsIdentityAndAdvancesAcceptedCounter() {
        val directory = temporary.newFolder()
        val original = store(directory)
        val hostKnownKey = original.keyPair().public
        val acceptedCounter = original.nextCounter()
        // Reconstruct all storage state as a fresh game process would
        val restarted = store(directory)
        assertArrayEquals(hostKnownKey.encoded, restarted.keyPair().public.encoded)
        val requestCounter = restarted.nextCounter()
        assertTrue(requestCounter > acceptedCounter)
        val request = "generation=17;counter=$requestCounter;callsign=ace".toByteArray()
        val signature =
            Signature
                .getInstance("SHA256withECDSA")
                .apply {
                    initSign(restarted.keyPair().private)
                    update(request)
                }.sign()
        assertTrue(
            Signature
                .getInstance("SHA256withECDSA")
                .apply {
                    initVerify(hostKnownKey)
                    update(request)
                }.verify(signature),
        )
        assertEquals(requestCounter + 1, store(directory).nextCounter())
    }

    @Test
    fun interruptedWriteDoesNotReplaceCommittedIdentity() {
        val directory = temporary.newFolder()
        val original = store(directory)
        val key = original.keyPair().public.encoded
        val counter = original.nextCounter()
        File(directory, "identity.pending").writeBytes(byteArrayOf(1, 2))
        val restarted = store(directory)
        assertArrayEquals(key, restarted.keyPair().public.encoded)
        assertEquals(counter + 1, restarted.nextCounter())
    }

    @Test(expected = java.io.EOFException::class)
    fun damagedCommittedIdentityIsNotSilentlyReplaced() {
        val directory = temporary.newFolder()
        File(directory, "identity.bin").writeBytes(byteArrayOf(1, 2))
        store(directory).keyPair()
    }
}
