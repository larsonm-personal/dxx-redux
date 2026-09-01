package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class NetworkEventsOverlayTest {
    @Test
    fun shortTextIsUnchanged() {
        assertEquals(listOf("short status"), wrapNetworkEventText("short status", 20))
    }

    @Test
    fun longTextWrapsWithContinuationIndent() {
        val lines = wrapNetworkEventText("  failed to send important packets to peer", 18)

        assertEquals(
            listOf("  failed to send", "    important", "    packets to", "    peer"),
            lines,
        )
        assertTrue(lines.all { it.length <= 18 })
    }

    @Test
    fun longTokenIsHardWrapped() {
        val lines = wrapNetworkEventText("abcdefghijklmnop", 10, "  ")

        assertEquals(listOf("abcdefghij", "  klmnop"), lines)
    }
}
