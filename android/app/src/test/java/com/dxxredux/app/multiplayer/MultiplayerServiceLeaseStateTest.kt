package com.dxxredux.app.multiplayer

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MultiplayerServiceLeaseStateTest {
    @Test
    fun `stopping game lease preserves active LAN lobby`() {
        val state = MultiplayerServiceLeaseState()

        state.setLanActive(true)
        state.setGameActive(true)
        state.setGameActive(false)

        assertTrue(state.active)
        assertTrue(state.lanActive)
        assertFalse(state.gameActive)
    }

    @Test
    fun `service becomes idle only after both leases stop`() {
        val state = MultiplayerServiceLeaseState()

        state.setGameActive(true)
        state.setLanActive(true)
        state.setGameActive(false)
        state.setLanActive(false)

        assertFalse(state.active)
    }
}
