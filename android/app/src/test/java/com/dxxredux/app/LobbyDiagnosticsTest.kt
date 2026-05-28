package com.dxxredux.app

import com.dxxredux.app.lobby.LAN_BROADCAST_FAILURE_DIAGNOSTIC
import com.dxxredux.app.lobby.lanDiagnosticAfterBroadcastRecovery
import com.dxxredux.app.lobby.shouldRefreshLanDiscoveryAfterResume
import com.dxxredux.app.lobby.shouldShowBroadcastFailureWarning
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LobbyDiagnosticsTest {
    @Test
    fun lanDiagnosticAfterBroadcastRecovery_clearsOnlyBroadcastWarning() {
        assertEquals("", lanDiagnosticAfterBroadcastRecovery(LAN_BROADCAST_FAILURE_DIAGNOSTIC))
        assertEquals(
            "Join failed: no response from 192.168.1.20",
            lanDiagnosticAfterBroadcastRecovery("Join failed: no response from 192.168.1.20"),
        )
    }

    @Test
    fun shouldRefreshLanDiscoveryAfterResume_requiresActiveDiscovery() {
        assertTrue(
            shouldRefreshLanDiscoveryAfterResume(
                isDiscovering = true,
                wasBackgrounded = true,
                socketUnavailable = false,
            ),
        )
        assertTrue(
            shouldRefreshLanDiscoveryAfterResume(
                isDiscovering = true,
                wasBackgrounded = false,
                socketUnavailable = true,
            ),
        )
        assertFalse(
            shouldRefreshLanDiscoveryAfterResume(
                isDiscovering = false,
                wasBackgrounded = true,
                socketUnavailable = true,
            ),
        )
    }

    @Test
    fun shouldShowBroadcastFailureWarning_suppressesBackgroundFailures() {
        assertFalse(shouldShowBroadcastFailureWarning(appBackgrounded = true))
        assertTrue(shouldShowBroadcastFailureWarning(appBackgrounded = false))
    }
}