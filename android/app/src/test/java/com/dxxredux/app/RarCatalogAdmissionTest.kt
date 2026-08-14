package com.dxxredux.app

import kotlinx.coroutines.CancellationException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException

class RarCatalogAdmissionTest {
    @Test
    fun entryLimitAcceptsExactAndRejectsOneOver() {
        val exact = source("one.bin", "two.bin")
        enumerateRarCatalog(exact, policy(maxEntries = 2)).use { assertEquals(2, it.items.size) }

        assertIOException("exceeds 2 entries") {
            enumerateRarCatalog(source("one.bin", "two.bin", "three.bin"), policy(maxEntries = 2))
        }
    }

    @Test
    fun workLimitCountsFilteredNativeItems() {
        enumerateRarCatalog(source(null, ""), policy(maxWorkUnits = 2)).use { assertTrue(it.items.isEmpty()) }

        assertIOException("exceeds 2 work units") {
            enumerateRarCatalog(source(null, "", null), policy(maxWorkUnits = 2))
        }
    }

    @Test
    fun liveMemoryLimitAcceptsExactAndRejectsOneUnder() {
        val paths = arrayOf("common/prefix/alpha.bin", "common/prefix/beta.bin")
        var peak = 0L
        var liveBytes = -1L
        enumerateRarCatalog(
            source(*paths),
            policy(
                onLiveBytesChanged = {
                    peak = maxOf(peak, it)
                    liveBytes = it
                },
            ),
        ).use { assertEquals(2, it.items.size) }
        assertTrue(peak > 0)
        assertEquals(0, liveBytes)

        enumerateRarCatalog(source(*paths), policy(maxLiveBytes = peak)).use { assertEquals(2, it.items.size) }
        assertIOException("live memory bytes") {
            enumerateRarCatalog(source(*paths), policy(maxLiveBytes = peak - 1))
        }
    }

    @Test
    fun longCommonPrefixesCannotAmplifyRetainedMemory() {
        var liveBytes = -1L
        val paths = Array(32) { index -> "common/${"segment/".repeat(12)}item-$index.bin" }
        assertIOException("live memory bytes") {
            enumerateRarCatalog(
                source(*paths),
                policy(maxLiveBytes = 12_000, onLiveBytesChanged = { liveBytes = it }),
            )
        }
        assertEquals(0, liveBytes)
    }

    @Test
    fun projectedPrefixesConsumeTheSameWorkAttempt() {
        val source = source(null, "one/two/three.bin")
        enumerateRarCatalog(source, policy(maxWorkUnits = 9)).use { assertEquals(1, it.items.size) }
        assertIOException("exceeds 8 work units") {
            enumerateRarCatalog(source, policy(maxWorkUnits = 8))
        }
    }

    @Test
    fun cancellationStopsEnumerationAndReleasesPartialCatalog() {
        var checks = 0
        var liveBytes = -1L
        val error =
            runCatching {
                enumerateRarCatalog(
                    source("one.bin", "two.bin"),
                    policy(
                        isCancelled = { ++checks > 3 },
                        onLiveBytesChanged = { liveBytes = it },
                    ),
                )
            }.exceptionOrNull()
        assertTrue(error is CancellationException)
        assertEquals(0, liveBytes)
    }

    @Test
    fun sourceFailureReleasesPartialCatalog() {
        var liveBytes = -1L
        val source =
            object : RarCatalogSource {
                override val itemCount = 2

                override fun path(index: Int) = "item-$index.bin"

                override fun isDirectory(index: Int) = false

                override fun size(index: Int): Long = if (index == 1) throw IOException("source failed") else 1

                override fun packedSize(index: Int) = 1L
            }
        assertIOException("source failed") {
            enumerateRarCatalog(source, policy(onLiveBytesChanged = { liveBytes = it }))
        }
        assertEquals(0, liveBytes)
    }

    @Test
    fun canonicalCollisionsRemainPolicyFailures() {
        val error =
            runCatching {
                enumerateRarCatalog(source("Folder/File.bin", "folder/file.BIN"), policy())
            }.exceptionOrNull()
        assertTrue(error is ArchiveOutputValidationException)
    }

    @Test
    fun traversalRemainsAnOutputPolicyFailure() {
        val error = runCatching { enumerateRarCatalog(source("../outside.bin"), policy()) }.exceptionOrNull()
        assertTrue(error is ArchiveOutputValidationException)
    }

    private fun source(vararg paths: String?): RarCatalogSource =
        object : RarCatalogSource {
            override val itemCount = paths.size

            override fun path(index: Int) = paths[index]

            override fun isDirectory(index: Int) = paths[index]?.endsWith('/') == true

            override fun size(index: Int) = 1L

            override fun packedSize(index: Int) = 1L
        }

    private fun policy(
        maxEntries: Int = 100,
        maxWorkUnits: Long = 1_000,
        maxLiveBytes: Long = 1_000_000,
        isCancelled: () -> Boolean = { false },
        onLiveBytesChanged: (Long) -> Unit = {},
    ) = RarCatalogAdmissionPolicy(
        maxEntries = maxEntries,
        maxWorkUnits = maxWorkUnits,
        maxLiveBytes = maxLiveBytes,
        isCancelled = isCancelled,
        onLiveBytesChanged = onLiveBytesChanged,
    )

    private fun assertIOException(
        message: String,
        action: () -> Unit,
    ) {
        val error = runCatching(action).exceptionOrNull()
        assertTrue(error is IOException)
        assertTrue(error?.message.orEmpty(), error?.message.orEmpty().contains(message))
    }
}
