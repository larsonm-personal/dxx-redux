package com.dxxredux.app

import kotlinx.coroutines.CancellationException
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException
import java.nio.file.Files

class RarFallbackPolicyTest {
    @Test
    fun unavailableNativeBackendAllowsCapabilityFallback() {
        assertCapabilityFallback(RarNativeBackendUnavailableException("unavailable"))
    }

    @Test
    fun unsupportedNativeFormatAllowsCapabilityFallback() {
        assertCapabilityFallback(RarNativeFormatUnsupportedException("unsupported"))
    }

    @Test
    fun missingNativeLibraryAllowsCapabilityFallback() {
        assertCapabilityFallback(UnsatisfiedLinkError("missing native library"))
    }

    @Test
    fun outputPolicyRejectionRemainsTerminal() {
        assertTerminal(ArchiveOutputValidationException("colliding outputs"))
    }

    @Test
    fun extractionLimitRejectionRemainsTerminal() {
        assertTerminal(IOException("Archive exceeds 4096 entries"))
    }

    @Test
    fun catalogEnumerationRejectionRemainsTerminal() {
        val source =
            object : RarCatalogSource {
                override val itemCount = 1

                override fun path(index: Int) = "entry.bin"

                override fun isDirectory(index: Int) = false

                override fun size(index: Int) = 1L

                override fun packedSize(index: Int) = 1L
            }
        val failure =
            runCatching {
                enumerateRarCatalog(source, RarCatalogAdmissionPolicy(maxEntries = 0))
            }.exceptionOrNull()
        assertTrue(failure is IOException)
        assertTerminal(failure!!)
    }

    @Test
    fun cancellationRemainsTerminal() {
        assertTerminal(CancellationException("cancelled"))
    }

    @Test
    fun integrityFailureRemainsTerminal() {
        assertTerminal(IllegalArgumentException("7-Zip extraction returned CRCERROR"))
    }

    @Test
    fun unknownNativeFailureFailsClosed() {
        assertTerminal(IllegalStateException("unexpected native state"))
    }

    private fun assertCapabilityFallback(nativeFailure: Throwable) {
        withTempDirectory { root ->
            val archive = File(root, "input.rar").apply { writeBytes(byteArrayOf(1)) }
            val target = File(root, "output")
            var fallbackCalled = false
            extractRarArchiveToDirectory(
                archive,
                target,
                nativeExtractor = { _, output ->
                    File(output, "partial.bin").writeBytes(byteArrayOf(2))
                    throw nativeFailure
                },
                capabilityFallback = { _, output ->
                    fallbackCalled = true
                    assertFalse(File(output, "partial.bin").exists())
                    File(output, "safe.bin").writeBytes(byteArrayOf(3))
                },
            )
            assertTrue(fallbackCalled)
            assertTrue(File(target, "safe.bin").isFile)
        }
    }

    private fun assertTerminal(nativeFailure: Throwable) {
        withTempDirectory { root ->
            val archive = File(root, "input.rar").apply { writeBytes(byteArrayOf(1)) }
            val target = File(root, "output")
            var fallbackCalled = false
            val thrown =
                runCatching {
                    extractRarArchiveToDirectory(
                        archive,
                        target,
                        nativeExtractor = { _, _ -> throw nativeFailure },
                        capabilityFallback = { _, _ -> fallbackCalled = true },
                    )
                }.exceptionOrNull()
            assertSame(nativeFailure, thrown)
            assertFalse(fallbackCalled)
        }
    }

    private fun withTempDirectory(action: (File) -> Unit) {
        val root = Files.createTempDirectory("rar-fallback-policy-").toFile()
        try {
            action(root)
        } finally {
            assertTrue(root.deleteRecursively())
            assertFalse(root.exists())
        }
    }
}
