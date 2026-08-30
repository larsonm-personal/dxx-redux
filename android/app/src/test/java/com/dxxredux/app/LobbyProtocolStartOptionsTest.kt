package com.dxxredux.app

import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.MSG_JOIN
import com.dxxredux.app.lobby.MSG_PING
import com.dxxredux.app.lobby.MSG_READY
import com.dxxredux.app.lobby.buildAnnounce
import com.dxxredux.app.lobby.buildJoin
import com.dxxredux.app.lobby.buildJoinAck
import com.dxxredux.app.lobby.buildJoinedLobbyHeartbeat
import com.dxxredux.app.lobby.buildLeave
import com.dxxredux.app.lobby.buildMissionStatus
import com.dxxredux.app.lobby.buildMissionTransferGrant
import com.dxxredux.app.lobby.buildMissionTransferRequest
import com.dxxredux.app.lobby.buildPlayerList
import com.dxxredux.app.lobby.buildReady
import com.dxxredux.app.lobby.buildStart
import com.dxxredux.app.lobby.missionRequirementFromJson
import com.dxxredux.app.lobby.missionStatusFromJson
import com.dxxredux.app.lobby.parsePacket
import com.dxxredux.app.multiplayer.MissionCompatibilityStatus
import com.dxxredux.app.multiplayer.MissionRequirement
import com.dxxredux.app.multiplayer.MissionStatusReport
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LobbyProtocolStartOptionsTest {
    @Test
    fun buildStart_includesCoopPickupAndDeathSpewOptions() {
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
                duplicateEnergyShields = true,
                fullDeathSpew = false,
                playerSpewNoExpire = false,
                clientsCanRequestRewind = true,
            )
        val json = parsePacket(packet, packet.size) ?: error("packet did not parse")

        assertTrue(json.getBoolean("coop_qol"))
        assertTrue(json.getBoolean("duplicate_energy_shields"))
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

    @Test
    fun joinedLobbyHeartbeatResendsDesiredReadyState() {
        val packets =
            buildJoinedLobbyHeartbeat(
                "lobby",
                "Wing",
                "wing-id",
                true,
                missionStatus = null,
                nowMs = 1234L,
            )
        val json = packets.map { parsePacket(it, it.size) ?: error("heartbeat packet did not parse") }

        assertEquals(listOf(MSG_JOIN, MSG_READY, MSG_PING), json.map { it.getString("type") })
        assertTrue(json[1].getBoolean("ready"))
        assertEquals("wing-id", json.last().getString("client_id"))
        assertEquals(1234L, json.last().getLong("ts"))
    }

    @Test
    fun playerListPublishesReconnectState() {
        val packet =
            buildPlayerList(
                "lobby",
                listOf(
                    LanPlayer(
                        callsign = "Wing",
                        address = "192.0.2.2",
                        ready = false,
                        connected = false,
                    ),
                ),
            )
        val player =
            parsePacket(packet, packet.size)
                ?.getJSONArray("players")
                ?.getJSONObject(0)
                ?: error("player list packet did not parse")

        assertFalse(player.getBoolean("connected"))
    }

    @Test
    fun missionIdentityStatusAndTransferAuthorizationRoundTrip() {
        val hash = "ab".repeat(32)
        val requirement =
            MissionRequirement(
                revision = "$hash:42:d2:castaway",
                game = "d2",
                missionKey = "castaway",
                displayName = "PTMC Castaway Redux",
                kind = MissionRequirement.KIND_WRAPPER,
                descriptorPath = "missions/castaway.mn2",
                wrapperFilename = "castaway_redux.zip",
                sizeBytes = 42L,
                sha256 = hash,
                offerAvailable = true,
            )
        val announce = buildAnnounce("lobby", "Host", "d2", "castaway", "coop", 2, 4, missionRequirement = requirement)
        val status =
            MissionStatusReport(
                revision = requirement.revision,
                status = MissionCompatibilityStatus.DOWNLOADING,
                verifiedBytes = 21L,
                totalBytes = 42L,
                transferId = "attempt-token",
                attempt = 1,
                bytesPerSecond = 12_345L,
            )
        val statusPacket = buildMissionStatus("lobby", "Wing", "wing-id", status)
        val request = buildMissionTransferRequest("lobby", "Wing", "wing-id", requirement.revision, 1)
        val grant = buildMissionTransferGrant("lobby", "secret-token", 42425, requirement.revision, 1)

        val announceJson = parsePacket(announce, announce.size) ?: error("announce did not parse")
        val statusJson = parsePacket(statusPacket, statusPacket.size) ?: error("status did not parse")
        assertEquals(requirement, missionRequirementFromJson(announceJson.getJSONObject("mission_requirement")))
        assertEquals(status, missionStatusFromJson(statusJson.getJSONObject("mission_status")))
        assertEquals(requirement.revision, parsePacket(request, request.size)?.getString("revision"))
        assertEquals("secret-token", parsePacket(grant, grant.size)?.getString("token"))
        assertTrue(announce.size < 8 * 1024)
        assertTrue(statusPacket.size < 8 * 1024)
    }
}
