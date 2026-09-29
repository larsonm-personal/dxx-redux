package com.dxxredux.app

import android.app.Instrumentation
import com.dxxredux.app.lobby.LanLobbyAnnounce
import com.dxxredux.app.lobby.LanPlayer
import com.dxxredux.app.lobby.LobbyService
import com.dxxredux.app.lobby.buildPlayerList
import com.dxxredux.app.multiplayer.GameLaunchInfo
import com.dxxredux.app.multiplayer.MissionRequirement
import com.dxxredux.app.multiplayer.NetworkConstants
import org.json.JSONObject

/** Exercise the real lobby service across migration and host-to-client role changes */
internal class CoopSessionChecks(private val instrumentation: Instrumentation) {
    private fun field(name: String) = LobbyService::class.java.getDeclaredField(name).apply { isAccessible = true }

    private fun packet(json: JSONObject) {
        LobbyService::class.java.getDeclaredMethod("handlePacket", JSONObject::class.java, String::class.java)
            .apply { isAccessible = true }.invoke(LobbyService, json, "192.0.2.1")
    }

    fun run() {
        LobbyService.stopDiscovery()
        try {
            LobbyService.startDiscovery(instrumentation.targetContext, "FormerHost")
            val requirement = MissionRequirement(
                revision = "migration-test", game = "d2", missionKey = "d2", displayName = "Counterstrike",
                kind = MissionRequirement.KIND_BUILTIN,
            )
            val game = GameLaunchInfo(
                game = "d2", mission = "d2", mode = "coop", difficulty = 3, levelNum = 2,
                maxPlayers = 4, yourSlot = 1, isHost = true, peers = emptyList(), isLan = true,
                missionRequirement = requirement,
            )
            LobbyService.hostLobby("FormerHost", "d2", "d2", "coop", 4)
            LobbyService.startGame(3, 2)
            check(LobbyService.lanLaunchEvent.value == null) { "One-player new game unexpectedly launched" }
            LobbyService.adoptMigratedHost("FormerHost", game, NetworkConstants.HOST_PROXY_PORT)
            check(LobbyService.isHosting.value && field("gameStarted").getBoolean(null))
            check(field("hostedHostPort").getInt(null) == NetworkConstants.HOST_PROXY_PORT)
            check(field("inGameLevelNum").getInt(null) == 2 && field("inGameDifficulty").getInt(null) == 3)
            check(field("hostedMissionRequirement").get(null) == requirement)
            check(LobbyService.lanLaunchEvent.value == null) { "Migration relaunched the host engine" }

            LobbyService.joinLobby("new-host", "192.0.2.1", "FormerHost")
            check(!LobbyService.isHosting.value && field("announceJob").get(null) == null)
            packet(JSONObject().put("type", "JOIN_ACK").put("lobby_id", "new-host").put("game", "d2"))
            val players = listOf(LanPlayer("NewHost", "192.0.2.1"), LanPlayer("FormerHost", "192.0.2.2"))
            packet(JSONObject(String(buildPlayerList("new-host", players), Charsets.UTF_8)))
            check(LobbyService.hostedLobbyPlayers.value.map { it.callsign } == players.map { it.callsign }) {
                "Former host ignored the new host's player list"
            }
            LobbyService.adoptMigratedHost("FormerHost", game, NetworkConstants.HOST_PROXY_PORT)
            check(LobbyService.joinedLobby.value == null && field("joinedLobbyRefreshJob").get(null) == null)
            LobbyService.joinDiscoveredLobby(
                LanLobbyAnnounce(
                    lobbyId = "running", callsign = "NewHost", game = "d2", mission = "d2", mode = "coop",
                    playerCount = 1, maxPlayers = 4, hostAddress = "192.0.2.1", status = "in_game",
                    difficulty = 3, levelNum = 2, hostPort = NetworkConstants.HOST_PROXY_PORT,
                ),
                "FormerHost",
            )
            check(!LobbyService.isHosting.value)
            val join = checkNotNull(LobbyService.lanLaunchEvent.value)
            check(!join.isHost && join.levelNum == 2 && join.lanHostPort == NetworkConstants.HOST_PROXY_PORT)
        } finally {
            LobbyService.stopDiscovery()
        }
    }
}
