package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class BinarySizeFormatterTest {
    @Test
    fun sizesUseBinaryPowersWithTraditionalLabels() {
        assertEquals("1023 B", formatBinarySize(1023L))
        assertEquals("1 KB", formatBinarySize(1024L))
        assertEquals("1.0 MB", formatBinarySize(1024L * 1024L))
        assertEquals("1.00 GB", formatBinarySize(1024L * 1024L * 1024L))
    }

    @Test
    fun ratesAndRoundedStorageMessagesAvoidIecLabels() {
        assertEquals("1.5 MB/s", formatBinaryRate(1536L * 1024L))
        assertEquals("2 MB", formatBinaryMegabytesRoundedUp(1024L * 1024L + 1L))
    }
}
