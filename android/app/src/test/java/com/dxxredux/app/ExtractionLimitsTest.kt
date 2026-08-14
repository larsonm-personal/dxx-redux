package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.nio.file.Files

class ExtractionLimitsTest {
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
}
