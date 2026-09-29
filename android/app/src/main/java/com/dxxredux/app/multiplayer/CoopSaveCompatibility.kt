package com.dxxredux.app.multiplayer

import com.dxxredux.app.FileSetManager
import org.json.JSONObject
import java.io.File

internal object CoopSaveCompatibility {
    const val WARNING =
        "This save cannot be used by this build: its co-op data is incompatible, missing, or damaged. " +
            "Choose another save or Start fresh."

    private val nativeAvailable = runCatching { System.loadLibrary("dxx-redux-d2") }.isSuccess

    fun warning(
        filesDir: File,
        game: String,
        mission: String?,
        save: CoopSaveEntry?,
        catalog: MissionCatalog? = null,
    ): String? {
        if (save == null || save.type == "checkpoint") return null
        if (save.slot !in 0..9 && save.checkpointId == null) return WARNING
        return selectionWarning(
            filesDir,
            game,
            mission,
            CoopRestoreSelection(save.slot.takeIf { it >= 0 }, save.checkpointId),
            catalog,
        )
    }

    fun hostWarning(
        filesDir: File,
        game: String,
        mission: String?,
    ): String? = readCoopRestoreSelection(filesDir, game)?.let { selectionWarning(filesDir, game, mission, it) }

    private fun selectionWarning(
        filesDir: File,
        game: String,
        mission: String?,
        selection: CoopRestoreSelection,
        catalog: MissionCatalog? = null,
    ): String? {
        if (selection.slot == null && selection.checkpointId == null) return null
        if (!nativeAvailable) return WARNING
        return runCatching {
            val root = File(filesDir, if (game == "d1") "d1x-redux" else "d2x-redux").canonicalFile
            val path =
                if (selection.checkpointId != null) {
                    root
                        .listFiles { file -> file.name.startsWith("coop_level_start_") && file.extension == "json" }
                        ?.firstNotNullOfOrNull { manifest ->
                            val json = runCatching { JSONObject(manifest.readText()) }.getOrNull()
                            if (json?.optString("checkpoint_id") == selection.checkpointId &&
                                json.optString("mission").equals(mission, ignoreCase = true)
                            ) {
                                File(root, json.getString("save_path")).canonicalFile
                            } else {
                                null
                            }
                        }
                } else {
                    File(nativeSlotPath(root.path, mission.orEmpty(), checkNotNull(selection.slot))).canonicalFile
                }
            if (path == null || !path.path.startsWith(root.path + File.separator) || !path.isFile) {
                return@runCatching WARNING
            }
            val contentGame =
                if (game == "d1") {
                    "d1"
                } else {
                    val snapshot =
                        catalog ?: run {
                            val fileSets = FileSetManager(filesDir)
                            MissionCatalog.scan(filesDir, fileSets.getSetDir(fileSets.getActive()), game)
                        }
                    snapshot.find(mission.orEmpty())?.contentGame ?: return@runCatching WARNING
                }
            val status = nativeCompatibilityStatus(path.path, game == "d1", contentGame == "d1")
            when {
                status == 0 -> null
                status > 0 -> "This D1-in-D2 save uses unsupported version $status. Choose another save or Start fresh."
                else -> WARNING
            }
        }.getOrDefault(WARNING)
    }

    private external fun nativeSlotPath(
        root: String,
        mission: String,
        slot: Int,
    ): String

    // JNI contract: 0 compatible, -1 invalid header/co-op data, positive rejected D1-in-D2 version
    private external fun nativeCompatibilityStatus(
        path: String,
        d1Engine: Boolean,
        d1Mission: Boolean,
    ): Int
}
