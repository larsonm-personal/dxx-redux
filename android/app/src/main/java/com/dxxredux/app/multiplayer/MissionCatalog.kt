package com.dxxredux.app.multiplayer

import android.content.Context
import androidx.annotation.WorkerThread
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.LauncherDebugLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/** One consistent scan, shared by mission selection and save checks for a loading operation */
internal class MissionCatalog private constructor(
    private val missions: List<MissionScanner.MissionInfo>,
) {
    fun missionsFor(mode: String): List<MissionScanner.MissionInfo> =
        if (mode == "coop") missions.filterNot { it.anarchyOnly } else missions

    fun find(
        mission: String?,
        mode: String = "coop",
    ): MissionScanner.MissionInfo? = resolveMissionSelection(missionsFor(mode), mission)

    companion object {
        suspend fun load(
            context: Context,
            game: String,
        ): MissionCatalog =
            withContext(Dispatchers.IO) {
                val started = System.nanoTime()
                val sets = FileSetManager(context.filesDir)
                try {
                    scan(context.filesDir, sets.getSetDir(sets.getActive()), game)
                } finally {
                    LauncherDebugLog.log(
                        "Mission catalog game=$game elapsed_ms=${(System.nanoTime() - started) / 1_000_000}",
                    )
                }
            }

        // Worker callers may supply an already-resolved file set; UI callers must use load
        @WorkerThread
        fun scan(
            filesDir: File,
            setDir: File,
            game: String,
        ): MissionCatalog = MissionCatalog(MissionScanner.scan(filesDir, setDir, game, "anarchy").toList())
    }
}
