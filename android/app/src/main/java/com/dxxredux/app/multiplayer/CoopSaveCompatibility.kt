package com.dxxredux.app.multiplayer

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
    ): String? {
        if (save == null || save.type == "checkpoint") return null
        if (save.slot !in 0..9 && save.checkpointId == null) return WARNING
        return selectionWarning(
            filesDir,
            game,
            mission,
            CoopRestoreSelection(save.slot.takeIf { it >= 0 }, save.checkpointId),
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
            if (path != null && path.path.startsWith(root.path + File.separator) && path.isFile &&
                nativeIsCompatible(path.path)
            ) {
                null
            } else {
                WARNING
            }
        }.getOrDefault(WARNING)
    }

    private external fun nativeSlotPath(
        root: String,
        mission: String,
        slot: Int,
    ): String

    private external fun nativeIsCompatible(path: String): Boolean
}
