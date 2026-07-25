package com.dxxredux.app.multiplayer

import android.content.Context
import com.dxxredux.app.lobby.LanLobbyAnnounce
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonPrimitive
import org.json.JSONArray
import org.json.JSONObject

internal const val MULTIPLAYER_RESUME_SCHEMA_VERSION = 1

internal data class MultiplayerResumeRecord(
    val schemaVersion: Int = MULTIPLAYER_RESUME_SCHEMA_VERSION,
    val updatedAtMs: Long,
    val role: String,
    val transport: String,
    val game: String,
    val mission: String,
    val mode: String,
    val difficulty: Int,
    val levelNum: Int,
    val maxPlayers: Int,
    val coopQol: Boolean = true,
    val duplicateEnergyShields: Boolean = false,
    val fullDeathSpew: Boolean = true,
    val playerSpewNoExpire: Boolean = true,
    val localCallsign: String,
    val localClientId: String? = null,
    val hostCallsign: String? = null,
    val hostClientId: String? = null,
    val lanHostAddr: String? = null,
    val lanHostPort: Int = NetworkConstants.ENGINE_PORT,
    val serverUrl: String? = null,
    val lobbyId: String? = null,
    val peerCallsigns: List<String> = emptyList(),
    val peerClientIds: List<String> = emptyList(),
    val coopRestoreSlot: Int? = null,
    val coopRestoreCheckpointId: String? = null,
    val coopRestoreSaveTime: Long? = null,
    val coopRestoreLevel: Int? = null,
    val restoreWasSelected: Boolean = false,
    val hostMigrated: Boolean = false,
    val clientsCanRequestRewind: Boolean = false,
    val restrictNonCoopFovToBase: Boolean = false,
)

internal fun MultiplayerResumeRecord.isHostLanCoop(): Boolean = role == "host" && transport == "lan" && mode == "coop"

internal fun MultiplayerResumeRecord.isClientLanCoop(): Boolean =
    role == "client" && transport == "lan" && mode == "coop" && !lanHostAddr.isNullOrBlank()

internal fun MultiplayerResumeRecord.isHostOnlineCoop(): Boolean =
    role == "host" && transport == "matchmaking" && mode == "coop" && !serverUrl.isNullOrBlank()

internal fun MultiplayerResumeRecord.isClientOnlineCoop(): Boolean =
    role == "client" && transport == "matchmaking" && mode == "coop" && !serverUrl.isNullOrBlank()

internal fun MultiplayerResumeRecord.matchesLanResumeHost(announce: LanLobbyAnnounce): Boolean {
    if (!isClientLanCoop()) return false
    val hostId = hostClientId
    if (!hostId.isNullOrBlank()) {
        val announceHostId = announce.hostClientId
        if (!announceHostId.isNullOrBlank()) return announceHostId == hostId
    }
    val hostName = hostCallsign
    return !hostName.isNullOrBlank() &&
        announce.callsign.equals(hostName, ignoreCase = true) &&
        announce.game == game &&
        announce.mode == mode &&
        announce.mission.equals(mission, ignoreCase = true)
}

internal fun MultiplayerResumeRecord.toHostDefaults(): HostGameDefaults.Defaults =
    HostGameDefaults.Defaults(
        game = game,
        mission = mission,
        mode = mode,
        difficulty = difficulty,
        levelNum = levelNum,
        maxPlayers = maxPlayers,
        coopQol = coopQol,
        duplicateEnergyShields = duplicateEnergyShields,
        fullDeathSpew = fullDeathSpew,
        playerSpewNoExpire = playerSpewNoExpire,
        clientsCanRequestRewind = clientsCanRequestRewind,
        restrictNonCoopFovToBase = restrictNonCoopFovToBase,
    )

internal fun MultiplayerResumeRecord.toGameInfoJson(): JsonObject =
    JsonObject(
        mapOf(
            "game" to JsonPrimitive(game),
            "mission" to JsonPrimitive(mission),
            "mode" to JsonPrimitive(mode),
            "difficulty" to JsonPrimitive(difficulty),
            "level_num" to JsonPrimitive(levelNum),
            "coop_qol" to JsonPrimitive(coopQol),
            "duplicate_energy_shields" to JsonPrimitive(duplicateEnergyShields),
            "full_death_spew" to JsonPrimitive(fullDeathSpew),
            "player_spew_no_expire" to JsonPrimitive(playerSpewNoExpire),
            "clients_can_request_rewind" to JsonPrimitive(clientsCanRequestRewind),
            "restrict_noncoop_fov_to_base" to JsonPrimitive(restrictNonCoopFovToBase),
        ),
    )

internal fun MultiplayerResumeRecord.matchesOnlineLobby(lobby: LobbyInfo): Boolean {
    if (!isClientOnlineCoop()) return false
    val savedLobbyId = lobbyId
    if (!savedLobbyId.isNullOrBlank() && lobby.lobbyId == savedLobbyId) return true
    val hostName = hostCallsign
    return !hostName.isNullOrBlank() &&
        lobby.hostCallsign.equals(hostName, ignoreCase = true) &&
        lobby.game == game &&
        lobby.gameInfo.stringValue("mission").equals(mission, ignoreCase = true) &&
        lobby.gameInfo.stringValue("mode") == mode
}

internal fun encodeMultiplayerResumeRecord(record: MultiplayerResumeRecord): String =
    JSONObject()
        .put("schema_version", record.schemaVersion)
        .put("updated_at_ms", record.updatedAtMs)
        .put("role", record.role)
        .put("transport", record.transport)
        .put("game", record.game)
        .put("mission", record.mission)
        .put("mode", record.mode)
        .put("difficulty", record.difficulty)
        .put("level_num", record.levelNum)
        .put("max_players", record.maxPlayers)
        .put("coop_qol", record.coopQol)
        .put("duplicate_energy_shields", record.duplicateEnergyShields)
        .put("full_death_spew", record.fullDeathSpew)
        .put("player_spew_no_expire", record.playerSpewNoExpire)
        .put("local_callsign", record.localCallsign)
        .putNullable("local_client_id", record.localClientId)
        .putNullable("host_callsign", record.hostCallsign)
        .putNullable("host_client_id", record.hostClientId)
        .putNullable("lan_host_addr", record.lanHostAddr)
        .put("lan_host_port", record.lanHostPort)
        .putNullable("server_url", record.serverUrl)
        .putNullable("lobby_id", record.lobbyId)
        .put("peer_callsigns", record.peerCallsigns.toJsonArray())
        .put("peer_client_ids", record.peerClientIds.toJsonArray())
        .putNullable("coop_restore_slot", record.coopRestoreSlot)
        .putNullable("coop_restore_checkpoint_id", record.coopRestoreCheckpointId)
        .putNullable("coop_restore_save_time", record.coopRestoreSaveTime)
        .putNullable("coop_restore_level", record.coopRestoreLevel)
        .put("restore_was_selected", record.restoreWasSelected)
        .put("host_migrated", record.hostMigrated)
        .put("clients_can_request_rewind", record.clientsCanRequestRewind)
        .put("restrict_noncoop_fov_to_base", record.restrictNonCoopFovToBase)
        .toString()

internal fun decodeMultiplayerResumeRecord(raw: String?): MultiplayerResumeRecord? {
    if (raw.isNullOrBlank()) return null
    return try {
        val json = JSONObject(raw)
        val schemaVersion = json.optInt("schema_version", 0)
        if (schemaVersion != MULTIPLAYER_RESUME_SCHEMA_VERSION) return null
        val role = json.optString("role", "")
        val transport = json.optString("transport", "")
        val game = json.optString("game", "")
        val mode = json.optString("mode", "")
        val callsign = json.optString("local_callsign", "")
        if (role !in setOf("host", "client") || transport !in setOf("lan", "matchmaking")) return null
        if (game !in setOf("d1", "d2") || mode.isBlank() || callsign.isBlank()) return null
        MultiplayerResumeRecord(
            schemaVersion = schemaVersion,
            updatedAtMs = json.optLong("updated_at_ms", 0L),
            role = role,
            transport = transport,
            game = game,
            mission = json.optString("mission", ""),
            mode = mode,
            difficulty = json.optInt("difficulty", 1),
            levelNum = json.optInt("level_num", 1),
            maxPlayers = json.optInt("max_players", 4),
            coopQol = json.optBoolean("coop_qol", true),
            duplicateEnergyShields = json.optBoolean("duplicate_energy_shields", false),
            fullDeathSpew = json.optBoolean("full_death_spew", true),
            playerSpewNoExpire = json.optBoolean("player_spew_no_expire", true),
            localCallsign = callsign,
            localClientId = json.optNullableString("local_client_id"),
            hostCallsign = json.optNullableString("host_callsign"),
            hostClientId = json.optNullableString("host_client_id"),
            lanHostAddr = json.optNullableString("lan_host_addr"),
            lanHostPort = json.optInt("lan_host_port", NetworkConstants.ENGINE_PORT),
            serverUrl = json.optNullableString("server_url"),
            lobbyId = json.optNullableString("lobby_id"),
            peerCallsigns = json.optStringList("peer_callsigns"),
            peerClientIds = json.optStringList("peer_client_ids"),
            coopRestoreSlot = json.optNullableInt("coop_restore_slot"),
            coopRestoreCheckpointId = json.optNullableString("coop_restore_checkpoint_id"),
            coopRestoreSaveTime = json.optNullableLong("coop_restore_save_time"),
            coopRestoreLevel = json.optNullableInt("coop_restore_level"),
            restoreWasSelected = json.optBoolean("restore_was_selected", false),
            hostMigrated = json.optBoolean("host_migrated", false),
            clientsCanRequestRewind = json.optBoolean("clients_can_request_rewind", false),
            restrictNonCoopFovToBase = json.optBoolean("restrict_noncoop_fov_to_base", false),
        )
    } catch (_: Exception) {
        null
    }
}

internal object MultiplayerResumePrefs {
    private const val PREFS_NAME = "dxx_prefs"
    private const val KEY = "mp_resume_record_v1"

    fun load(context: Context): MultiplayerResumeRecord? =
        decodeMultiplayerResumeRecord(
            context
                .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .getString(KEY, null),
        )

    fun save(
        context: Context,
        record: MultiplayerResumeRecord,
    ) {
        context
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY, encodeMultiplayerResumeRecord(record))
            .apply()
    }

    fun clear(context: Context) {
        context
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .remove(KEY)
            .apply()
    }

    fun saveLaunch(
        context: Context,
        info: GameLaunchInfo,
        localCallsign: String,
        state: MatchmakingState,
    ) {
        val lobby = state.currentLobby
        val hostPlayer = lobby?.players?.firstOrNull { it.playerId == lobby.hostPlayerId }
        val localClientId = ClientIdentity.getInstallationId(context)
        val restoreSelection =
            if (info.isHost &&
                info.mode == "coop"
            ) {
                readCoopRestoreSelection(context.filesDir, info.game)
            } else {
                null
            }
        save(
            context,
            MultiplayerResumeRecord(
                updatedAtMs = System.currentTimeMillis(),
                role = if (info.isHost) "host" else "client",
                transport = if (info.isLan) "lan" else "matchmaking",
                game = info.game,
                mission = info.mission,
                mode = info.mode,
                difficulty = info.difficulty,
                levelNum = info.levelNum,
                maxPlayers = info.maxPlayers,
                coopQol = info.coopQol,
                duplicateEnergyShields = info.duplicateEnergyShields,
                fullDeathSpew = info.fullDeathSpew,
                playerSpewNoExpire = info.playerSpewNoExpire,
                clientsCanRequestRewind = info.clientsCanRequestRewind,
                restrictNonCoopFovToBase = info.restrictNonCoopFovToBase,
                localCallsign = localCallsign,
                localClientId = localClientId,
                hostCallsign = if (info.isHost) localCallsign else info.hostCallsign ?: hostPlayer?.callsign,
                hostClientId = info.hostClientId ?: lobby?.hostPlayerId ?: localClientId.takeIf { info.isHost },
                lanHostAddr = info.lanHostAddr,
                lanHostPort = info.lanHostPort,
                serverUrl = if (info.isLan) null else state.serverUrl,
                lobbyId = lobby?.lobbyId,
                peerCallsigns = lobby?.players?.map { it.callsign }.orEmpty(),
                peerClientIds = lobby?.players?.map { it.playerId }.orEmpty(),
                coopRestoreSlot = restoreSelection?.slot,
                coopRestoreCheckpointId = restoreSelection?.checkpointId,
                coopRestoreLevel = info.levelNum.takeIf { restoreSelection != null },
                restoreWasSelected = restoreSelection != null,
            ),
        )
    }

    fun saveRestoreSelection(
        context: Context,
        game: String,
        selectedSave: CoopSaveEntry?,
        levelNum: Int? = null,
    ) {
        val current = load(context) ?: return
        if (current.game != game || current.mode != "coop") return
        val restoreSlot = selectedSave?.slot?.takeIf { it >= 0 }
        val restoreCheckpointId = selectedSave?.checkpointId
        save(
            context,
            current.copy(
                updatedAtMs = System.currentTimeMillis(),
                levelNum = levelNum ?: selectedSave?.level ?: current.levelNum,
                coopRestoreSlot = restoreSlot,
                coopRestoreCheckpointId = restoreCheckpointId,
                coopRestoreSaveTime =
                    selectedSave?.timestamp?.takeIf {
                        restoreSlot != null || restoreCheckpointId != null
                    },
                coopRestoreLevel =
                    selectedSave?.level?.takeIf {
                        restoreSlot != null || restoreCheckpointId != null
                    },
                restoreWasSelected = true,
            ),
        )
    }
}

internal fun MultiplayerResumeRecord.coopRestoreSelection(): CoopRestoreSelection? =
    when {
        !coopRestoreCheckpointId.isNullOrBlank() -> CoopRestoreSelection(null, coopRestoreCheckpointId)
        coopRestoreSlot != null -> CoopRestoreSelection(coopRestoreSlot)
        restoreWasSelected -> CoopRestoreSelection(null)
        else -> null
    }

internal fun resolveCoopHostResumeRecord(
    record: MultiplayerResumeRecord,
    coopSaves: List<CoopSaveEntry>,
): MultiplayerResumeRecord {
    if (record.role != "host" || record.mode != "coop") return record
    val restorable = coopSaves.filter { it.type == "full_save" || it.type == "level_start_highest" }
    val requested =
        when {
            !record.coopRestoreCheckpointId.isNullOrBlank() -> {
                restorable.firstOrNull { it.checkpointId == record.coopRestoreCheckpointId }
            }

            record.coopRestoreSlot != null -> {
                restorable.firstOrNull {
                    it.slot == record.coopRestoreSlot &&
                        (record.coopRestoreSaveTime == null || it.timestamp == record.coopRestoreSaveTime) &&
                        it.level == (record.coopRestoreLevel ?: record.levelNum)
                }
            }

            record.restoreWasSelected -> {
                return record
            }

            else -> {
                null
            }
        }
    val selected =
        requested ?: restorable.maxWithOrNull(compareBy<CoopSaveEntry> { it.level }.thenBy { it.timestamp })
            ?: return record.copy(
                coopRestoreSlot = null,
                coopRestoreCheckpointId = null,
                coopRestoreSaveTime = null,
                coopRestoreLevel = null,
                restoreWasSelected = false,
            )
    return record.copy(
        levelNum = selected.level,
        coopRestoreSlot = selected.slot.takeIf { it >= 0 },
        coopRestoreCheckpointId = selected.checkpointId,
        coopRestoreSaveTime = selected.timestamp,
        coopRestoreLevel = selected.level,
        restoreWasSelected = true,
    )
}

internal fun resolveCoopHostResumeRecord(
    context: Context,
    record: MultiplayerResumeRecord,
): MultiplayerResumeRecord {
    if (record.role != "host" || record.mode != "coop") return record
    val saves = readCoopAutosaveHistory(context.filesDir, record.game, record.mission, context)
    val retained = readCoopLevelStartCheckpoints(context.filesDir, record.game, record.mission, context)
    return resolveCoopHostResumeRecord(record, saves + retained)
}

private fun JSONObject.putNullable(
    key: String,
    value: Any?,
): JSONObject = if (value == null) put(key, JSONObject.NULL) else put(key, value)

private fun List<String>.toJsonArray(): JSONArray = JSONArray().also { array -> forEach { array.put(it) } }

private fun JSONObject.optNullableString(key: String): String? = if (has(key) && !isNull(key)) optString(key) else null

private fun JSONObject.optNullableInt(key: String): Int? = if (has(key) && !isNull(key)) optInt(key) else null

private fun JSONObject.optNullableLong(key: String): Long? = if (has(key) && !isNull(key)) optLong(key) else null

private fun JSONObject.optStringList(key: String): List<String> {
    val array = optJSONArray(key) ?: return emptyList()
    val values = ArrayList<String>(array.length())
    for (index in 0 until array.length()) values.add(array.optString(index))
    return values
}

private fun JsonObject.stringValue(key: String): String = this[key]?.jsonPrimitive?.contentOrNull ?: ""
