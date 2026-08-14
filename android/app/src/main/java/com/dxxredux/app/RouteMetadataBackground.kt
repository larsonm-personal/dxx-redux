package com.dxxredux.app

import android.content.Context
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay
import java.io.File

internal object RouteMetadataBackground {
    private const val TOTAL_TIMEOUT_MS = 10L * 60L * 1_000L
    private const val RETRY_DELAY_MS = 2_000L

    suspend fun computeActiveLevel(
        context: Context,
        game: String,
        mission: String,
        levelNum: Int,
        levelFile: String,
    ): Boolean {
        val key = "$game|$mission|$levelNum|$levelFile"
        val fileSets = FileSetManager(context.filesDir)
        val target =
            LevelMetadataTarget(
                displayName = "Active game level",
                game = game,
                sourceType = "active_level",
                dataDir = fileSets.getSetDir(fileSets.getActive()).absolutePath,
                missionName = mission,
                levelFile = levelFile,
                levelNum = levelNum,
                normalLevelFiles = if (levelNum > 0) listOf(levelFile) else emptyList(),
                secretLevelFiles = if (levelNum < 0) listOf(levelFile) else emptyList(),
            )
        try {
            val deadline = SystemClock.elapsedRealtime() + TOTAL_TIMEOUT_MS
            var attempt = 0
            while (SystemClock.elapsedRealtime() < deadline) {
                val result =
                    LevelMetadataAnalyzer.analyze(
                        context,
                        target,
                        background = true,
                        totalTimeoutMs = deadline - SystemClock.elapsedRealtime(),
                    )
                val level = result.levels.singleOrNull()
                val cacheFile = level?.routeCacheFile.orEmpty()
                val gameDir = if (game == GameFileFormats.GAME_D1) "d1x-redux" else "d2x-redux"
                val cachePublished =
                    cacheFile.isNotBlank() && File(File(context.filesDir, gameDir), cacheFile).isFile
                val busy = result.problems.any { it.contains("already running", ignoreCase = true) }
                val retryable = busy || result.status == "crashed"
                if (cachePublished || !retryable) {
                    LauncherDebugLog.log(
                        "Route metadata background result game=$game mission=$mission " +
                            "level=$levelNum status=${result.status} " +
                            "route=${level?.routeStatus.orEmpty()} cache_published=$cachePublished",
                    )
                    return cachePublished
                }
                attempt++
                val remaining = deadline - SystemClock.elapsedRealtime()
                if (remaining > 0L) delay(minOf(attempt * RETRY_DELAY_MS, 10_000L, remaining))
            }
        } catch (e: CancellationException) {
            throw e
        }
        Log.w("DXX-RouteMetadata", "Background route worker did not publish a cache for $key")
        return false
    }
}
