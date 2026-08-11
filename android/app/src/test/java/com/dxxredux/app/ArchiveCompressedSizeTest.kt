package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class ArchiveCompressedSizeTest {
    @Test
    fun rendersUnavailableCompressedSizeExplicitly() {
        assertEquals("Unknown", archiveCompressedSizeText(null))
        assertEquals("0 B", archiveCompressedSizeText(0))
        assertEquals("1 KB", archiveCompressedSizeText(1024))
    }
}
