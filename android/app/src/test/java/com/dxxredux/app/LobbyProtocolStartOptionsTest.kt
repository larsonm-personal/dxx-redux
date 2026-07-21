package com.dxxredux.app

import com.dxxredux.app.lobby.buildStart
import com.dxxredux.app.lobby.buildAnnounce
import com.dxxredux.app.lobby.buildJoin
import com.dxxredux.app.lobby.buildJoinAck
import com.dxxredux.app.lobby.buildLeave
import com.dxxredux.app.lobby.buildReady
import com.dxxredux.app.lobby.parsePacket
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LobbyProtocolStartOptionsTest {
    @Test
    fun buildStart_includesDeathSpewOption() {
        val packet =
            buildStart(
                lobbyId = "lobby",
                hostAddress = "127.0.0.1",
                hostPort = 42424,
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                coopQol = true,
                fullDeathSpew = false,
                playerSpewNoExpire = false,
                clientsCanRequestRewind = true,
            )
        val json = parsePacket(packet, packet.size) ?: error("packet did not parse")

        assertTrue(json.getBoolean("coop_qol"))
        assertFalse(json.getBoolean("full_death_spew"))
        assertFalse(json.getBoolean("player_spew_no_expire"))
        assertTrue(json.getBoolean("clients_can_request_rewind"))
    }

    @Test
    fun lanIdentityFieldsRoundTripThroughPackets() {
        val announce = buildAnnounce("lobby", "Host", "d2", "d2", "coop", 2, 4, hostClientId = "host-id")
        val join = buildJoin("lobby", "Wing", clientId = "wing-id")
        val joinAck = buildJoinAck("lobby", "d2", "d2", "coop", 4, "Host", "host-id")
        val ready = buildReady("lobby", "Wing", true, clientId = "wing-id")
        val leave = buildLeave("lobby", "Wing", clientId = "wing-id")

        assertEquals("host-id", parsePacket(announce, announce.size)?.getString("host_client_id"))
        assertEquals("wing-id", parsePacket(join, join.size)?.getString("client_id"))
        assertEquals("Host", parsePacket(joinAck, joinAck.size)?.getString("host_callsign"))
        assertEquals("host-id", parsePacket(joinAck, joinAck.size)?.getString("host_client_id"))
        assertEquals("wing-id", parsePacket(ready, ready.size)?.getString("client_id"))
        assertEquals("wing-id", parsePacket(leave, leave.size)?.getString("client_id"))
    }
}
