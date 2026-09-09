package com.dxxredux.app.multiplayer

import com.dxxredux.app.lobby.LanLobbyAnnounce
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class MultiplayerResumePrefsTest {
    @get:Rule
    val tempFolder = TemporaryFolder()

    @Test
    fun resumeRecordRoundTripsThroughJson() {
        val record =
            MultiplayerResumeRecord(
                updatedAtMs = 1234L,
                role = "host",
                transport = "lan",
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 2,
                levelNum = 7,
                maxPlayers = 4,
                coopQol = true,
                duplicateEnergyShields = true,
                fullDeathSpew = false,
                playerSpewNoExpire = false,
                localCallsign = "Miner",
                localClientId = "local-id",
                hostCallsign = "Miner",
                hostClientId = "host-id",
                lanHostAddr = "192.168.1.20",
                lanHostPort = 42424,
                serverUrl = null,
                lobbyId = "lobby-1",
                peerCallsigns = listOf("Miner", "Wing"),
                peerClientIds = listOf("local-id", "peer-id"),
                coopRestoreSlot = 7,
                coopRestoreCheckpointId = null,
                coopRestoreSaveTime = 99L,
                coopRestoreLevel = 7,
                restoreWasSelected = true,
                hostMigrated = true,
                clientsCanRequestRewind = true,
            )

        val decoded = decodeMultiplayerResumeRecord(encodeMultiplayerResumeRecord(record))

        assertEquals(record, decoded)
    }

    @Test
    fun coopRestoreSlotFreshSelectionIsExplicitButNotASlot() {
        val filesDir = tempFolder.newFolder()

        writeCoopRestoreSlot(filesDir, "d2", 7)

        assertEquals(7, readCoopRestoreSlot(filesDir, "d2"))
        assertEquals(7, readCoopRestoreSelection(filesDir, "d2")?.slot)

        writeCoopRestoreSlot(filesDir, "d2", null)

        val freshSelection = readCoopRestoreSelection(filesDir, "d2") ?: error("fresh selection missing")
        assertNull(readCoopRestoreSlot(filesDir, "d2"))
        assertNull(freshSelection.slot)
    }

    @Test
    fun retainedLevelStartSelectionRoundTripsAsTypedChoice() {
        val filesDir = tempFolder.newFolder()

        writeCoopRestoreCheckpoint(filesDir, "d2", "d2")

        val selection = readCoopRestoreSelection(filesDir, "d2") ?: error("retained selection missing")
        assertNull(selection.slot)
        assertEquals("d2", selection.checkpointId)
    }

    @Test
    fun resumeRecordRoundTripsRetainedCheckpointChoice() {
        val record = hostResumeRecord(level = 6).copy(
            coopRestoreCheckpointId = "d2",
            coopRestoreLevel = 6,
            restoreWasSelected = true,
        )

        val decoded = decodeMultiplayerResumeRecord(encodeMultiplayerResumeRecord(record))

        assertEquals(record, decoded)
        assertEquals("d2", decoded?.coopRestoreSelection()?.checkpointId)
        assertEquals("Level-start checkpoint", decoded?.let(::multiplayerResumeRestoreLabel))
    }

    @Test
    fun legacyFreshResumeSelectsNewestValidSave() {
        val retained = coopSave(level = 6, timestamp = 100L).copy(
            slot = -1,
            type = "level_start_highest",
            checkpointId = "d2",
        )
        val level7 = coopSave(level = 7, timestamp = 200L)
        val record = hostResumeRecord(level = 6)

        val resolved = resolveCoopHostResumeRecord(record, listOf(retained, level7))

        assertEquals(7, resolved.levelNum)
        assertEquals(level7.slot, resolved.coopRestoreSlot)
        assertNull(resolved.coopRestoreCheckpointId)
        assertEquals("Save slot ${level7.slot}", multiplayerResumeRestoreLabel(resolved))
    }

    @Test
    fun staleRecordedSlotFallsBackToNewestValidSave() {
        val level7 = coopSave(level = 7, timestamp = 200L)
        val stale = hostResumeRecord(level = 6).copy(
            coopRestoreSlot = 5,
            coopRestoreSaveTime = 100L,
            coopRestoreLevel = 6,
            restoreWasSelected = true,
        )

        val resolved = resolveCoopHostResumeRecord(stale, listOf(level7))

        assertEquals(7, resolved.levelNum)
        assertEquals(level7.slot, resolved.coopRestoreSlot)
    }

    @Test
    fun previousCheckpointAndExplicitFreshYieldToNewestProgress() {
        val retained = coopSave(level = 6, timestamp = 100L).copy(
            slot = -1,
            type = "level_start_highest",
            checkpointId = "d2",
        )
        val level7 = coopSave(level = 7, timestamp = 200L)
        val typed = hostResumeRecord(level = 6).copy(
            coopRestoreCheckpointId = "d2",
            restoreWasSelected = true,
        )
        val fresh = hostResumeRecord(level = 6).copy(restoreWasSelected = true)

        assertEquals(level7.slot, resolveCoopHostResumeRecord(typed, listOf(level7, retained)).coopRestoreSlot)
        assertEquals(level7.slot, resolveCoopHostResumeRecord(fresh, listOf(level7, retained)).coopRestoreSlot)
    }

    @Test
    fun newestProgressCanBeOnALowerOrSecretLevel() {
        val oldSave = coopSave(level = 7, timestamp = 100L)
        val latest = coopSave(level = -1, timestamp = 300L).copy(slot = 8)
        val record = hostResumeRecord(level = 7).copy(
            coopRestoreSlot = oldSave.slot,
            coopRestoreSaveTime = oldSave.timestamp,
            coopRestoreLevel = oldSave.level,
            restoreWasSelected = true,
        )

        val resolved = resolveCoopHostResumeRecord(record, listOf(oldSave, latest))

        assertEquals(-1, resolved.levelNum)
        assertEquals(latest.slot, resolved.coopRestoreSlot)
        assertEquals(latest.timestamp, resolved.coopRestoreSaveTime)
    }

    @Test
    fun freshResumeWithoutSavesKeepsItsLevel() {
        val record = hostResumeRecord(level = 6)
        assertEquals(record, resolveCoopHostResumeRecord(record.copy(restoreWasSelected = true), emptyList()))
    }

    @Test
    fun retainedLevelStartOfferRequiresMatchingSaveChecksum() {
        val filesDir = tempFolder.newFolder()
        val gameDir = filesDir.resolve("d2x-redux")
        val savePath = "savesets/coop/d2/level_start_highest.sav"
        val save = gameDir.resolve(savePath)
        val bytes = "level-start-state".toByteArray()
        save.parentFile?.mkdirs()
        save.writeBytes(bytes)
        gameDir.resolve("coop_level_start_d2.json").writeText(
            """{
                "type":"level_start_highest",
                "checkpoint_id":"d2",
                "mission":"d2",
                "level":6,
                "timestamp":123,
                "num_players":2,
                "callsigns":["Miner","Wing"],
                "client_ids":["local-id","peer-id"],
                "save_path":"$savePath",
                "size":${bytes.size},
                "checksum":${coopLevelStartChecksum(bytes)}
            }""".trimIndent(),
        )

        val offers = readCoopLevelStartCheckpointsForClient(filesDir, "d2", "d2", "local-id")
        assertEquals(1, offers.size)
        assertEquals(6, offers.single().level)

        save.appendText("corrupt")
        assertTrue(readCoopLevelStartCheckpointsForClient(filesDir, "d2", "d2", "local-id").isEmpty())
    }

    @Test
    fun initialCoopSaveSelectionDefaultsToNewestFullSave() {
        val save5 =
            CoopSaveEntry(
                slot = 5,
                level = 4,
                timestamp = 100L,
                numPlayers = 2,
                callsigns = listOf("Miner", "Wing"),
            )
        val save6 = save5.copy(slot = 6, level = 5, timestamp = 200L)
        val saves = listOf(save5, save6)

        assertEquals(save6, initialCoopSaveSelection(saves))
    }

    @Test
    fun initialCoopSaveSelectionSupportsRetainedLevelStart() {
        val fullSave =
            CoopSaveEntry(
                slot = 5,
                level = 4,
                timestamp = 100L,
                numPlayers = 2,
                callsigns = listOf("Miner", "Wing"),
            )
        val retained =
            fullSave.copy(
                slot = -1,
                level = 6,
                type = "level_start_highest",
                checkpointId = "d2",
            )

        assertEquals(retained, initialCoopSaveSelection(listOf(retained)))
        assertEquals(fullSave, initialCoopSaveSelection(listOf(fullSave, retained)))
        assertEquals(retained, restoreSaveForHostedLevel(retained, 6))
    }

    @Test
    fun restoreSaveForHostedLevelRequiresMatchingFullSave() {
        val save =
            CoopSaveEntry(
                slot = 5,
                level = 4,
                timestamp = 100L,
                numPlayers = 2,
                callsigns = listOf("Miner", "Wing"),
            )
        val checkpoint = save.copy(slot = -1, type = "checkpoint")

        assertEquals(save, restoreSaveForHostedLevel(save, 4))
        assertNull(restoreSaveForHostedLevel(save, 5))
        assertNull(restoreSaveForHostedLevel(checkpoint, 4))
        assertNull(restoreSaveForHostedLevel(null, 4))
    }

    @Test
    fun startFreshPreservesCurrentLevelText() {
        val save =
            CoopSaveEntry(
                slot = 5,
                level = 4,
                timestamp = 100L,
                numPlayers = 2,
                callsigns = listOf("Miner", "Wing"),
            )

        assertEquals("4", coopLevelTextAfterSaveSelection("9", save))
        assertEquals("9", coopLevelTextAfterSaveSelection("9", null))
        assertEquals("9", coopLevelTextAfterSaveSelection("9", save.copy(level = 0)))
    }

    @Test
    fun invalidSchemaIsIgnored() {
        val json = """{"schema_version":999,"role":"host","transport":"lan","game":"d2"}"""

        assertNull(decodeMultiplayerResumeRecord(json))
    }

    @Test
    fun hostLanCoopPredicateRequiresHostLanCoop() {
        val hostLanCoop =
            MultiplayerResumeRecord(
                updatedAtMs = 1L,
                role = "host",
                transport = "lan",
                game = "d1",
                mission = "",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                localCallsign = "Pilot",
            )
        val clientLanCoop = hostLanCoop.copy(role = "client")
        val hostOnlineCoop = hostLanCoop.copy(transport = "matchmaking")
        val hostLanAnarchy = hostLanCoop.copy(mode = "anarchy")

        assertTrue(hostLanCoop.isHostLanCoop())
        assertFalse(clientLanCoop.isHostLanCoop())
        assertFalse(hostOnlineCoop.isHostLanCoop())
        assertFalse(hostLanAnarchy.isHostLanCoop())
    }

    @Test
    fun clientLanCoopPredicateRequiresHostAddress() {
        val clientLanCoop =
            MultiplayerResumeRecord(
                updatedAtMs = 1L,
                role = "client",
                transport = "lan",
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                localCallsign = "Wing",
                lanHostAddr = "192.168.1.20",
            )

        assertTrue(clientLanCoop.isClientLanCoop())
        assertFalse(clientLanCoop.copy(role = "host").isClientLanCoop())
        assertFalse(clientLanCoop.copy(mode = "anarchy").isClientLanCoop())
        assertFalse(clientLanCoop.copy(lanHostAddr = null).isClientLanCoop())
    }

    @Test
    fun clientLanResumeMatchesStableHostIdBeforeSessionDetails() {
        val record =
            MultiplayerResumeRecord(
                updatedAtMs = 1L,
                role = "client",
                transport = "lan",
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                localCallsign = "Wing",
                hostCallsign = "Host",
                hostClientId = "host-id",
                lanHostAddr = "192.168.1.20",
            )

        assertTrue(record.matchesLanResumeHost(lanAnnounce(hostClientId = "host-id", mission = "other")))
        assertFalse(record.matchesLanResumeHost(lanAnnounce(hostClientId = "other-id")))
    }

    @Test
    fun clientLanResumeFallsBackToHostNameAndSession() {
        val record =
            MultiplayerResumeRecord(
                updatedAtMs = 1L,
                role = "client",
                transport = "lan",
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                localCallsign = "Wing",
                hostCallsign = "Host",
                lanHostAddr = "192.168.1.20",
            )

        assertTrue(record.matchesLanResumeHost(lanAnnounce(hostClientId = null)))
        assertFalse(record.matchesLanResumeHost(lanAnnounce(hostClientId = null, mission = "other")))
    }

    @Test
    fun onlineCoopPredicatesRequireServerUrl() {
        val hostOnlineCoop =
            MultiplayerResumeRecord(
                updatedAtMs = 1L,
                role = "host",
                transport = "matchmaking",
                game = "d2",
                mission = "d2",
                mode = "coop",
                difficulty = 1,
                levelNum = 1,
                maxPlayers = 4,
                localCallsign = "Host",
                serverUrl = "ws://server",
            )
        val clientOnlineCoop = hostOnlineCoop.copy(role = "client")

        assertTrue(hostOnlineCoop.isHostOnlineCoop())
        assertFalse(hostOnlineCoop.copy(serverUrl = null).isHostOnlineCoop())
        assertTrue(clientOnlineCoop.isClientOnlineCoop())
        assertFalse(clientOnlineCoop.copy(mode = "anarchy").isClientOnlineCoop())
    }

    @Test
    fun onlineClientResumeMatchesLobbyIdBeforeSessionDetails() {
        val record = onlineClientRecord(lobbyId = "saved-lobby")

        assertTrue(record.matchesOnlineLobby(onlineLobby(lobbyId = "saved-lobby", mission = "other")))
        assertFalse(record.matchesOnlineLobby(onlineLobby(lobbyId = "other-lobby", hostCallsign = "Other")))
    }

    @Test
    fun onlineClientResumeFallsBackToHostNameAndSession() {
        val record = onlineClientRecord(lobbyId = null)

        assertTrue(record.matchesOnlineLobby(onlineLobby()))
        assertFalse(record.matchesOnlineLobby(onlineLobby(mission = "other")))
        assertFalse(record.matchesOnlineLobby(onlineLobby(hostCallsign = "Other")))
    }

    @Test
    fun resumeRecordBuildsOnlineGameInfo() {
        val gameInfo =
            onlineClientRecord()
                .copy(difficulty = 3, levelNum = 5, clientsCanRequestRewind = true)
                .toGameInfoJson()

        assertEquals("d2", gameInfo["game"]?.jsonPrimitive?.content)
        assertEquals("coop", gameInfo["mode"]?.jsonPrimitive?.content)
        assertEquals(3, gameInfo["difficulty"]?.jsonPrimitive?.intOrNull)
        assertEquals(5, gameInfo["level_num"]?.jsonPrimitive?.intOrNull)
        assertEquals("true", gameInfo["player_spew_no_expire"]?.jsonPrimitive?.content)
        assertEquals("true", gameInfo["clients_can_request_rewind"]?.jsonPrimitive?.content)
    }

    private fun hostResumeRecord(level: Int): MultiplayerResumeRecord =
        MultiplayerResumeRecord(
            updatedAtMs = 1L,
            role = "host",
            transport = "lan",
            game = "d2",
            mission = "d2",
            mode = "coop",
            difficulty = 1,
            levelNum = level,
            maxPlayers = 4,
            localCallsign = "Miner",
        )

    private fun coopSave(
        level: Int,
        timestamp: Long,
    ): CoopSaveEntry =
        CoopSaveEntry(
            slot = 6,
            level = level,
            timestamp = timestamp,
            numPlayers = 2,
            callsigns = listOf("Miner", "Wing"),
        )

    private fun lanAnnounce(
        hostClientId: String?,
        mission: String = "d2",
    ): LanLobbyAnnounce =
        LanLobbyAnnounce(
            lobbyId = "lobby",
            callsign = "Host",
            game = "d2",
            mission = mission,
            mode = "coop",
            playerCount = 2,
            maxPlayers = 4,
            hostAddress = "192.168.1.20",
            hostClientId = hostClientId,
        )

    private fun onlineClientRecord(lobbyId: String? = "lobby"): MultiplayerResumeRecord =
        MultiplayerResumeRecord(
            updatedAtMs = 1L,
            role = "client",
            transport = "matchmaking",
            game = "d2",
            mission = "d2",
            mode = "coop",
            difficulty = 1,
            levelNum = 1,
            maxPlayers = 4,
            localCallsign = "Wing",
            hostCallsign = "Host",
            serverUrl = "ws://server",
            lobbyId = lobbyId,
        )

    private fun onlineLobby(
        lobbyId: String = "lobby",
        hostCallsign: String = "Host",
        mission: String = "d2",
    ): LobbyInfo =
        LobbyInfo(
            lobbyId = lobbyId,
            hostCallsign = hostCallsign,
            game = "d2",
            playerCount = 1,
            maxPlayers = 4,
            joinable = true,
            gameInfo =
                JsonObject(
                    mapOf(
                        "mission" to JsonPrimitive(mission),
                        "mode" to JsonPrimitive("coop"),
                    ),
                ),
        )
}
