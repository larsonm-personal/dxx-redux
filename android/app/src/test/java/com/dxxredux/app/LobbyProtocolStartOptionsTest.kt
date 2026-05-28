package com.dxxredux.app

import com.dxxredux.app.lobby.buildStart
import com.dxxredux.app.lobby.parsePacket
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
            )
        val json = parsePacket(packet, packet.size) ?: error("packet did not parse")

        assertTrue(json.getBoolean("coop_qol"))
        assertFalse(json.getBoolean("full_death_spew"))
    }
}