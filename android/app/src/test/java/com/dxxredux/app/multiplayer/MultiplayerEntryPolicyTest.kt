package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Test

class MultiplayerEntryPolicyTest {
    @Test
    fun ordinaryBuildStartsOnLan() {
        assertEquals(MultiplayerNav.LAN, initialMultiplayerNav(false))
    }

    @Test
    fun onlineQualificationBuildExposesServerBrowser() {
        assertEquals(MultiplayerNav.BROWSER, initialMultiplayerNav(true))
    }
}
