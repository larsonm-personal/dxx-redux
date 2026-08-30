package com.dxxredux.app

import com.dxxredux.app.lobby.LAN_BROADCAST_FAILURE_DIAGNOSTIC
import com.dxxredux.app.lobby.LAN_PEER_TIMEOUT_MS
import com.dxxredux.app.lobby.LAN_RECONNECT_GRACE_MS
import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.LanPlayerLeaseAction
import com.dxxredux.app.lobby.lanDiagnosticAfterBroadcastRecovery
import com.dxxredux.app.lobby.lanLobbyHasClientIdConflict
import com.dxxredux.app.lobby.lanPlayerLeaseAction
import com.dxxredux.app.lobby.lanPlayerMatchesJoinIdentity
import com.dxxredux.app.lobby.lanPlayerMatchesSender
import com.dxxredux.app.lobby.lanTransportRecoveryReason
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
    fun lanTransportRecoveryReason_recoversOnlyAnActiveForegroundTransport() {
        assertEquals(
            "socket unavailable",
            lanTransportRecoveryReason(true, false, socketAvailable = false, receiveLoopActive = true),
        )
        assertEquals(
            "receive loop stopped",
            lanTransportRecoveryReason(true, false, socketAvailable = true, receiveLoopActive = false),
        )
        assertEquals(null, lanTransportRecoveryReason(true, true, false, false))
        assertEquals(null, lanTransportRecoveryReason(false, false, false, false))
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
    fun lanLobbyHasClientIdConflict_rejectsCopiedIdentityButAllowsReconnect() {
        val players =
            listOf(
                LanPlayer("Host", "127.0.0.1", "host-id", ready = true),
                LanPlayer("Wing", "192.168.1.20", "copied-id", ready = true),
            )

        assertTrue(lanLobbyHasClientIdConflict(players, "Other", "copied-id"))
        assertFalse(lanLobbyHasClientIdConflict(players, "wing", "copied-id"))
        assertFalse(lanLobbyHasClientIdConflict(players, "Other", null))
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

    @Test
    fun lanPlayerLeaseAction_marksThenRemovesAfterReconnectGrace() {
        val connected = LanPlayer("Wing", "192.168.1.20", lastSeenMs = 1_000L)
        val reconnecting =
            connected.copy(
                ready = false,
                connected = false,
                disconnectedAtMs = 20_000L,
            )

        assertEquals(LanPlayerLeaseAction.NONE, lanPlayerLeaseAction(connected, 1_000L + LAN_PEER_TIMEOUT_MS))
        assertEquals(
            LanPlayerLeaseAction.MARK_RECONNECTING,
            lanPlayerLeaseAction(connected, 1_001L + LAN_PEER_TIMEOUT_MS),
        )
        assertEquals(
            LanPlayerLeaseAction.NONE,
            lanPlayerLeaseAction(reconnecting, 20_000L + LAN_RECONNECT_GRACE_MS),
        )
        assertEquals(
            LanPlayerLeaseAction.REMOVE,
            lanPlayerLeaseAction(reconnecting, 20_001L + LAN_RECONNECT_GRACE_MS),
        )
    }
}
