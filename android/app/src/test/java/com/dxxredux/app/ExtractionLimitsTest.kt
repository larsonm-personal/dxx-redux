package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.nio.file.Files

class ExtractionLimitsTest {
    @Test
    fun expansionValidationRejectsMissingCompressedSize() {
        assertThrows(IOException::class.java) {
            ExtractionBudget().validateExpansion(1, 0, "zero")
        }
        assertThrows(IOException::class.java) {
            ExtractionBudget().validateExpansion(1, -1, "unknown")
        }
    }

    @Test
    fun extractionBudgetHonorsThreadCancellation() {
        Thread.currentThread().interrupt()
        try {
            assertThrows(IOException::class.java) {
                ExtractionBudget().registerEntry(1, 1, "cancelled")
            }
        } finally {
            Thread.interrupted()
        }
    }

    @Test
    fun exactReadUsesOneAllocationAtTheLiveMemoryLimit() {
        val liveBytes = mutableListOf<Long>()
        val policy = BoundedReadMemoryPolicy(maxLiveBytes = 8, onLiveBytesChanged = liveBytes::add)

        assertArrayEquals(
            ByteArray(8) { it.toByte() },
            ByteArrayInputStream(ByteArray(8) { it.toByte() }).readBytesBounded(
                maxBytes = 8,
                label = "exact",
                expectedSizeBytes = 8,
                memoryPolicy = policy,
            ),
        )
        assertEquals(listOf(8L), liveBytes)

        val oneOver =
            assertThrows(IOException::class.java) {
                ByteArrayInputStream(ByteArray(9)).readBytesBounded(
                    maxBytes = 9,
                    label = "one-over",
                    expectedSizeBytes = 9,
                    memoryPolicy = policy,
                )
            }
        assertTrue(oneOver.message.orEmpty().contains("8 live memory bytes"))
        assertEquals(ExtractionLimits.MAX_COMBINED_MEMORY_BYTES, 128L * 1024L * 1024L)
    }

    @Test
    fun unknownReadCountsSegmentsAndFinalCopyAtGrowthBoundaries() {
        val liveBytes = mutableListOf<Long>()
        val input = ByteArray(8) { (it + 1).toByte() }

        assertArrayEquals(
            input,
            ByteArrayInputStream(input).readBytesBounded(
                maxBytes = 32,
                label = "segmented",
                memoryPolicy =
                    BoundedReadMemoryPolicy(
                        maxLiveBytes = 16,
                        chunkBytes = 4,
                        onLiveBytesChanged = liveBytes::add,
                    ),
            ),
        )

        assertEquals(16L, liveBytes.maxOrNull())
        assertEquals(8L, liveBytes.last())
        assertEquals(listOf(4L, 8L, 16L, 12L, 8L), liveBytes)
    }

    @Test
    fun unknownReadRejectsBeforePeakMemoryCanGoOneByteOver() {
        val liveBytes = mutableListOf<Long>()
        val error =
            assertThrows(IOException::class.java) {
                ByteArrayInputStream(ByteArray(9)).readBytesBounded(
                    maxBytes = 32,
                    label = "one-over",
                    memoryPolicy =
                        BoundedReadMemoryPolicy(
                            maxLiveBytes = 16,
                            chunkBytes = 4,
                            onLiveBytesChanged = liveBytes::add,
                        ),
                )
            }

        assertTrue(error.message.orEmpty().contains("16 live memory bytes"))
        assertTrue(liveBytes.maxOrNull()!! <= 16)
        assertEquals(0L, liveBytes.last())
    }

    @Test
    fun exactReadHandlesShortAndZeroLengthReads() {
        val expected = ByteArray(11) { it.toByte() }
        assertArrayEquals(
            expected,
            FragmentedInputStream(expected, maxRead = 2).readBytesBounded(
                maxBytes = 11,
                label = "short reads",
                expectedSizeBytes = 11,
                memoryPolicy = BoundedReadMemoryPolicy(maxLiveBytes = 11),
            ),
        )
        assertArrayEquals(
            expected,
            FragmentedInputStream(expected, maxRead = 3, returnZeroBeforeRead = true).readBytesBounded(
                maxBytes = 11,
                label = "zero reads",
                expectedSizeBytes = 11,
                memoryPolicy = BoundedReadMemoryPolicy(maxLiveBytes = 11),
            ),
        )
    }

    @Test
    fun shortDeclaredInputAndAllocationFailureReleaseOwnedMemory() {
        val shortLiveBytes = mutableListOf<Long>()
        assertThrows(IOException::class.java) {
            ByteArrayInputStream(ByteArray(3)).readBytesBounded(
                maxBytes = 4,
                label = "short input",
                expectedSizeBytes = 4,
                memoryPolicy = BoundedReadMemoryPolicy(maxLiveBytes = 4, onLiveBytesChanged = shortLiveBytes::add),
            )
        }
        assertEquals(listOf(4L, 0L), shortLiveBytes)

        val oom = OutOfMemoryError("forced allocation failure")
        val oomLiveBytes = mutableListOf<Long>()
        var allocations = 0
        val error =
            assertThrows(IOException::class.java) {
                ByteArrayInputStream(ByteArray(8)).readBytesBounded(
                    maxBytes = 16,
                    label = "allocation failure",
                    memoryPolicy =
                        BoundedReadMemoryPolicy(
                            maxLiveBytes = 16,
                            chunkBytes = 4,
                            allocate = { size ->
                                allocations++
                                if (allocations == 3) throw oom
                                ByteArray(size)
                            },
                            onLiveBytesChanged = oomLiveBytes::add,
                        ),
                )
            }
        assertSame(oom, error.cause)
        assertEquals(8L, oomLiveBytes.maxOrNull())
        assertEquals(0L, oomLiveBytes.last())
    }

    @Test
    fun boundedReadAcceptsLimitAndRejectsLimitPlusOne() {
        assertArrayEquals(
            byteArrayOf(1, 2, 3),
            ByteArrayInputStream(byteArrayOf(1, 2, 3)).readBytesBounded(3, "descriptor"),
        )
        assertThrows(IOException::class.java) {
            ByteArrayInputStream(byteArrayOf(1, 2, 3, 4)).readBytesBounded(3, "descriptor")
        }
    }

    @Test
    fun declaredEntryRejectsOversizeAndExcessiveRatio() {
        assertThrows(IOException::class.java) {
            ExtractionBudget().registerEntry(ExtractionLimits.MAX_ENTRY_BYTES + 1, 1, "entry")
        }
        assertThrows(IOException::class.java) {
            ExtractionBudget().registerEntry(ExtractionLimits.MAX_RATIO + 1, 1, "entry")
        }
    }

    @Test
    fun budgetRejectsExcessiveEntryCountAndActualTotal() {
        val entryBudget = ExtractionBudget(maxEntries = 1)
        entryBudget.registerEntry(0, 0, "first")
        assertThrows(IOException::class.java) {
            entryBudget.registerEntry(0, 0, "second")
        }

        val byteBudget = ExtractionBudget(maxEntryBytes = 4, maxTotalBytes = 4)
        byteBudget.accountActual(3, 3, -1, "first")
        assertThrows(IOException::class.java) {
            byteBudget.accountActual(2, 2, -1, "second")
        }
    }

    @Test
    fun directEntryLimitsAcceptExactValuesAndRejectOneOver() {
        val exact = ExtractionBudget(maxEntryBytes = 4, maxTotalBytes = 4, maxEntries = 1)
        exact.registerEntry(4, 4, "exact")
        ByteArrayInputStream(ByteArray(4)).copyToBounded(ByteArrayOutputStream(), exact, 4, "exact")

        assertThrows(IOException::class.java) {
            ExtractionBudget(maxEntryBytes = 4).registerEntry(5, 5, "one-over")
        }
        assertThrows(IOException::class.java) {
            exact.registerEntry(0, 0, "second")
        }
        assertThrows(IOException::class.java) {
            val oneOver = ExtractionBudget(maxEntryBytes = 4, maxTotalBytes = 4)
            oneOver.registerEntry(0, 0, "unknown-size")
            ByteArrayInputStream(ByteArray(5)).copyToBounded(
                ByteArrayOutputStream(),
                oneOver,
                -1,
                "unknown-size",
            )
        }
    }

    @Test
    fun nestedAttemptContinuesTheDirectZipBudget() {
        val budget = ExtractionBudget(maxEntryBytes = 4, maxTotalBytes = 8, maxEntries = 2)
        budget.registerEntry(4, 4, "outer")
        ByteArrayInputStream(ByteArray(4)).copyToBounded(ByteArrayOutputStream(), budget, 4, "outer")

        val exactNested = budget.newDiscExtractionAttempt()
        exactNested.state[0] += 4
        exactNested.state[1] += 1
        budget.acceptDiscExtractionAttempt(exactNested)

        val oneOverBytes = budget.newDiscExtractionAttempt()
        oneOverBytes.state[0] += 1
        assertThrows(IOException::class.java) { budget.acceptDiscExtractionAttempt(oneOverBytes) }

        val oneOverEntries = budget.newDiscExtractionAttempt()
        oneOverEntries.state[1] += 1
        assertThrows(IOException::class.java) { budget.acceptDiscExtractionAttempt(oneOverEntries) }

        val unchanged = budget.newDiscExtractionAttempt()
        assertArrayEquals(longArrayOf(8, 2, 0), unchanged.state)
        unchanged.state[2] = 1
        assertThrows(IOException::class.java) { budget.acceptDiscExtractionAttempt(unchanged) }
    }

    @Test
    fun failedOrCancelledAttemptRemovesDirectAndNestedStaging() {
        val root = Files.createTempDirectory("setup-zip-budget-test").toFile()
        try {
            val direct = File(root, "direct.tmp").also { it.writeBytes(byteArrayOf(1)) }
            val nested = File(root, "nested").also { it.mkdirs() }
            File(nested, "nested.tmp").writeBytes(byteArrayOf(2))

            cleanupZipExtractionAttempt(listOf(direct, nested))

            org.junit.Assert.assertFalse(direct.exists())
            org.junit.Assert.assertFalse(nested.exists())
        } finally {
            root.deleteRecursively()
        }
    }

    private class FragmentedInputStream(
        private val bytes: ByteArray,
        private val maxRead: Int,
        private val returnZeroBeforeRead: Boolean = false,
    ) : java.io.InputStream() {
        private var offset = 0
        private var zeroPending = returnZeroBeforeRead

        override fun read(): Int = if (offset < bytes.size) bytes[offset++].toInt() and 0xff else -1

        override fun read(
            output: ByteArray,
            outputOffset: Int,
            length: Int,
        ): Int {
            if (zeroPending) {
                zeroPending = false
                return 0
            }
            if (offset >= bytes.size) return -1
            val count = minOf(length, maxRead, bytes.size - offset)
            bytes.copyInto(output, outputOffset, offset, offset + count)
            offset += count
            zeroPending = returnZeroBeforeRead
            return count
        }
    }
}
