package com.dxxredux.app.multiplayer

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class RuntimeGameStateBridgeTest {
    @Test
    fun gameStateCodec_preservesCompleteNetworkState() {
        val encoded = RuntimeGameStateCodec.encode(intArrayOf(1, 3, 8, -2, 4))
        val message = JSONObject(encoded!!)

        assertEquals("UPDATE_GAME_STATE", message.getString("type"))
        assertEquals(3, message.getInt("player_count"))
        assertEquals(8, message.getInt("max_players"))
        assertEquals(-2, message.getInt("current_level"))
        assertEquals(1, message.getInt("game_status"))
    }

    @Test
    fun gameStateCodec_rejectsIncompleteAndNonNetworkState() {
        assertNull(RuntimeGameStateCodec.encode(intArrayOf(1, 2, 4, 1)))
        assertNull(RuntimeGameStateCodec.encode(intArrayOf(1, 2, 4, 1, 0)))
    }

    @Test
    fun diagnosticsCodec_roundTripsOwnerSnapshot() {
        val expected =
            RuntimeDiagnosticsSnapshot(
                proxyStats = listOf(PeerProxyStats(2, 11, 12, 1300, 1400)),
                connectionInfo =
                    listOf(
                        PeerConnectionInfoMsg(
                            peerId = "peer-1",
                            peerCallsign = "Wing",
                            method = "relay",
                            detail = null,
                            serverRelay = true,
                            estimatedLatencyMs = 42,
                        ),
                    ),
            )

        assertEquals(
            expected,
            RuntimeDiagnosticsCodec.decode(
                RuntimeDiagnosticsCodec.encode(expected.proxyStats, expected.connectionInfo),
            ),
        )
    }

    @Test
    fun hostedDisconnectPolicy_failsClosed() {
        assertTrue(shouldEndHostedGame(wasHost = true, updatesEnabled = true, lobbyIsHost = false))
        assertTrue(shouldEndHostedGame(wasHost = true, updatesEnabled = false, lobbyIsHost = true))
        assertFalse(shouldEndHostedGame(wasHost = false, updatesEnabled = true, lobbyIsHost = true))
        assertFalse(shouldEndHostedGame(wasHost = true, updatesEnabled = false, lobbyIsHost = false))
    }

    @Test
    fun ipcSession_acceptsOnlyRegisteredHostAndDisconnectsOnce() {
        val session = RuntimeIpcSession()
        assertFalse(session.acceptsGameState())

        session.register(isHost = false)
        assertFalse(session.acceptsGameState())
        assertFalse(session.disconnect())

        session.register(isHost = true)
        assertTrue(session.acceptsGameState())
        assertTrue(session.disconnect())
        assertFalse(session.acceptsGameState())
        assertFalse(session.disconnect())
    }
}
