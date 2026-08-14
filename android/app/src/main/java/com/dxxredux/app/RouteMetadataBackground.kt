package com.dxxredux.app

import android.content.Context
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay

internal object RouteMetadataBackground {
    private const val RETRY_WINDOW_MS = 10L * 60L * 1_000L
    private const val TOTAL_TIMEOUT_MS = 10L * 60L * 1_000L
    private const val BUSY_RETRY_COUNT = 3
    private val requestedAt = mutableMapOf<String, Long>()

    suspend fun computeActiveLevel(
        context: Context,
        game: String,
        mission: String,
        levelNum: Int,
        levelFile: String,
    ) {
        val key = "$game|$mission|$levelNum|$levelFile"
        val now = SystemClock.elapsedRealtime()
        synchronized(requestedAt) {
            if (now - (requestedAt[key] ?: Long.MIN_VALUE) < RETRY_WINDOW_MS) return
            requestedAt[key] = now
        }
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
            repeat(BUSY_RETRY_COUNT) { attempt ->
                val result =
                    LevelMetadataAnalyzer.analyze(
                        context,
                        target,
                        background = true,
                        totalTimeoutMs = TOTAL_TIMEOUT_MS,
                    )
                val busy = result.problems.any { it.contains("already running", ignoreCase = true) }
                if (!busy) {
                    LauncherDebugLog.log(
                        "Route metadata background result game=$game mission=$mission " +
                            "level=$levelNum status=${result.status} " +
                            "route=${result.levels.singleOrNull()?.routeStatus.orEmpty()}",
                    )
                    return
                }
                if (attempt + 1 < BUSY_RETRY_COUNT) delay((attempt + 1) * 2_000L)
            }
        } catch (e: CancellationException) {
            synchronized(requestedAt) { requestedAt.remove(key) }
            throw e
        }
        synchronized(requestedAt) { requestedAt.remove(key) }
        Log.w("DXX-RouteMetadata", "Background route worker remained busy for $key")
    }
}
