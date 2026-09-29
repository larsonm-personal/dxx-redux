package com.dxxredux.app.multiplayer

import android.content.Context
import com.dxxredux.app.LauncherDebugLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

internal data class CoopSaveOptions(
    val saves: List<CoopSaveEntry> = emptyList(),
    val warnings: Map<CoopSaveEntry, String?> = emptyMap(),
    val resumeLevel: Int? = null,
) {
    companion object {
        suspend fun load(
            context: Context,
            game: String,
            mission: String?,
            mode: String,
            catalog: MissionCatalog,
        ): CoopSaveOptions =
            withContext(Dispatchers.IO) {
                if (mode != "coop") return@withContext CoopSaveOptions()
                val started = System.nanoTime()
                try {
                    val saves =
                        readCoopAutosaveHistory(context.filesDir, game, mission, context) +
                            readCoopLevelStartCheckpoints(context.filesDir, game, mission, context) +
                            listOfNotNull(readCoopProgressAsEntry(context.filesDir, game, mission, context))
                    CoopSaveOptions(
                        saves,
                        saves.associateWith {
                            CoopSaveCompatibility.warning(
                                context.filesDir,
                                game,
                                mission,
                                it,
                                catalog,
                            )
                        },
                        if (saves.none { it.type == "full_save" }) {
                            readCoopProgress(
                                context.filesDir,
                                game,
                                mission,
                            )
                        } else {
                            null
                        },
                    )
                } finally {
                    LauncherDebugLog.log(
                        "Co-op save options game=$game mission=$mission elapsed_ms=${(System.nanoTime() - started) / 1_000_000}",
                    )
                }
            }
    }
}
