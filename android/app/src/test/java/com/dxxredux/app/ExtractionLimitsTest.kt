package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.IOException

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
}
