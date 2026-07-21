package com.dxxredux.app

import com.dxxredux.app.lobby.LAN_BROADCAST_FAILURE_DIAGNOSTIC
import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.lanDiagnosticAfterBroadcastRecovery
import com.dxxredux.app.lobby.lanPlayerMatchesJoinIdentity
import com.dxxredux.app.lobby.lanPlayerMatchesSender
import com.dxxredux.app.lobby.refreshLanPlayerLeasesAfterResume
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

    @Test
    fun lanPlayerMatchesSender_protectsHostAndUsesStableClientIdentity() {
        val host = LanPlayer("Host", "127.0.0.1", "host-id", ready = true)
        val joiner = LanPlayer("Wing", "192.168.1.20", "wing-id", ready = true)

        assertFalse(lanPlayerMatchesSender(host, "Host", "host-id", "192.168.1.20"))
        assertFalse(lanPlayerMatchesSender(joiner, "Wing", "wing-id", "192.168.1.21"))
        assertTrue(lanPlayerMatchesJoinIdentity(joiner, "Wing", "wing-id", "192.168.1.21"))
        assertFalse(lanPlayerMatchesSender(joiner, "Host", "host-id", "192.168.1.20"))
    }

    @Test
    fun refreshLanPlayerLeasesAfterResume_preservesReadyState() {
        val host = LanPlayer("Host", "127.0.0.1", "host-id", ready = true, lastSeenMs = 10)
        val joiner = LanPlayer("Wing", "192.168.1.20", "wing-id", ready = true, lastSeenMs = 20)

        val refreshed = refreshLanPlayerLeasesAfterResume(listOf(host, joiner), nowMs = 1000)

        assertEquals(host, refreshed[0])
        assertTrue(refreshed[1].ready)
        assertEquals(1000L, refreshed[1].lastSeenMs)
    }
}
