package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MultiplayerControllerFocusPolicyTest {
    @Test
    fun disconnectedBrowserStartsOnLan() {
        assertEquals(
            MultiplayerBrowserInitialFocusTarget.LAN,
            multiplayerBrowserInitialFocusTarget(ConnectionStatus.DISCONNECTED),
        )
    }

    @Test
    fun connectingBrowserStartsOnCancel() {
        for (status in listOf(ConnectionStatus.CONNECTING, ConnectionStatus.AUTHENTICATING, ConnectionStatus.RECONNECTING)) {
            assertEquals(
                MultiplayerBrowserInitialFocusTarget.CANCEL_CONNECT,
                multiplayerBrowserInitialFocusTarget(status),
            )
        }
    }

    @Test
    fun connectedBrowserStartsOnRefresh() {
        assertEquals(
            MultiplayerBrowserInitialFocusTarget.REFRESH_LOBBIES,
            multiplayerBrowserInitialFocusTarget(ConnectionStatus.CONNECTED),
        )
    }

    @Test
    fun lanDiscoveryStartsOnPermissionActionWhenPermissionIsMissing() {
        assertEquals(
            LanDiscoveryInitialFocusTarget.PERMISSION_ACTION,
            lanDiscoveryInitialFocusTarget(permissionGranted = false),
        )
    }

    @Test
    fun lanDiscoveryStartsOnPrimaryActionWhenPermissionIsReady() {
        assertEquals(
            LanDiscoveryInitialFocusTarget.PRIMARY_ACTION,
            lanDiscoveryInitialFocusTarget(permissionGranted = true),
        )
    }

    @Test
    fun controllerBackConsumesOnlyActiveTextEntry() {
        assertTrue(controllerBackShouldExitTextEntry(textEntryActive = true))
        assertFalse(controllerBackShouldExitTextEntry(textEntryActive = false))
    }
}