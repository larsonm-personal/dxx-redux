package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class LanInvitationTest {
    @Test
    fun hostRoundTripsWithoutGameOrPort() {
        listOf("192.168.1.42", "10.1.2.3", "172.16.0.4", "169.254.1.2", "123.123.123.123").forEach { host ->
            assertEquals(host, LanInvitation.parse(LanInvitation.encode(host)))
            assertEquals(host, LanInvitation.parse("descent://$host/"))
        }
    }

    @Test
    fun rejectsAmbiguousOrNonUnicastInvitations() {
        listOf(
            "https://192.168.1.42",
            "descent://host.local",
            "descent://[::1]",
            "descent://user@192.168.1.42",
            "descent://192.168.1.42:42424",
            "descent://192.168.1.42?game=d1",
            "descent://192.168.1.42#join",
            "descent://192.168.1.42/join",
            "descent://192.168.001.42",
            "descent://127.0.0.1",
            "descent://0.0.0.0",
            "descent://224.0.0.1",
            "descent://255.255.255.255",
            "descent://256.1.2.3",
            "descent://1.2.3",
            "descent://192.168.1.42%2F",
            "descent://192.168.1.42\n",
            "descent://" + "1".repeat(4096),
        ).forEach { assertNull(it, LanInvitation.parse(it)) }
        assertNull(LanInvitation.parse("descent://192.168.1.255", setOf("192.168.1.255")))
    }

    @Test
    fun onlyExplicitRevealSurvivesUnrelatedRefreshes() {
        val state = LanQrRevealState()
        assertFalse(state.reveal())
        state.setAddress("192.168.1.42")
        assertFalse(state.revealed)
        assertTrue(state.reveal())
        state.setAddress("192.168.1.42")
        assertTrue(state.revealed)
        state.setAddress("192.168.1.43")
        assertFalse(state.revealed)
        state.reveal()
        state.conceal()
        state.setAddress("192.168.1.43")
        assertFalse(state.revealed)
        state.reveal()
        state.setAddress(null)
        assertFalse(state.revealed)
    }
}
