package com.dxxredux.app

import android.content.Context
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay
import java.io.File

internal object RouteMetadataBackground {
    private const val TOTAL_TIMEOUT_MS = 10L * 60L * 1_000L
    private const val RETRY_DELAY_MS = 2_000L

    suspend fun computeMission(
        context: Context,
        game: String,
        mission: String,
        currentLevelNum: Int,
        currentLevelFile: String,
        currentRouteReadiness: String,
        normalLevelFiles: List<String>,
        secretLevelFiles: List<String>,
        secretEntryLevels: List<Int>,
        onCurrentReady: (Boolean) -> Unit,
        onCurrentProgress: (Int, RouteMetadataProgressState) -> Unit = { _, _ -> },
    ) {
        val fileSets = FileSetManager(context.filesDir)
        val dataDir = fileSets.getSetDir(fileSets.getActive()).absolutePath
        val ledger = RouteMetadataLedger(context.filesDir)
        val monitor = RouteMetadataPrecomputeMonitor(context.filesDir)
        val sourceIdentity =
            listOf(
                game,
                mission,
                File(dataDir).lastModified(),
                normalLevelFiles.joinToString("/"),
                secretLevelFiles.joinToString("/"),
            ).joinToString("|")
        val levels =
            RouteMetadataMissionOrdering.order(
                currentLevelNum,
                currentLevelFile,
                normalLevelFiles,
                secretLevelFiles,
                secretEntryLevels,
            )
        val currentWork = RouteMetadataCurrentWork.forReadiness(currentRouteReadiness)
        var currentReported = false
        val progressTracker = RouteMetadataCurrentProgressTracker(currentLevelNum)

        fun reportCurrent(ready: Boolean) {
            if (!currentReported) {
                currentReported = true
                onCurrentReady(ready)
            }
        }

        try {
            if (currentWork.ready) reportCurrent(true)
            if (currentWork.ready) {
                progressTracker.onReadiness(currentLevelNum, currentRouteReadiness)?.let {
                    onCurrentProgress(it.permille, it.state)
                }
            }
            for (level in levels) {
                if (level.levelNum == currentLevelNum && currentWork.skip) continue
                val target =
                    LevelMetadataTarget(
                        displayName = "Active game mission",
                        game = game,
                        sourceType = "active_level",
                        dataDir = dataDir,
                        missionName = mission,
                        levelFile = level.levelFile,
                        levelNum = level.levelNum,
                        normalLevelFiles = if (level.levelNum > 0) listOf(level.levelFile) else emptyList(),
                        secretLevelFiles = if (level.levelNum < 0) listOf(level.levelFile) else emptyList(),
                    )
                val jobId =
                    listOf(
                        game,
                        mission,
                        level.levelNum.toString(),
                        level.levelFile,
                        sourceIdentity,
                        ROUTE_METADATA_CACHE_GENERATION.toString(),
                    ).joinToString("|")
                val existing = ledger.read(jobId)
                if (level.levelNum != currentLevelNum &&
                    existing?.status == RouteMetadataLedgerStatus.COMPLETE &&
                    cacheArtifact(context, game, existing.cacheFile).isFile
                ) {
                    continue
                }
                if (existing?.status == RouteMetadataLedgerStatus.FAILED) {
                    if (level.levelNum == currentLevelNum) {
                        progressTracker.onFailure(level.levelNum)?.let {
                            onCurrentProgress(it.permille, it.state)
                        }
                        reportCurrent(false)
                    }
                    continue
                }

                var priority =
                    if (level.levelNum == currentLevelNum) currentWork.priority else level.priority
                var attempt = 0
                while (true) {
                    val analysisStartedAt = System.currentTimeMillis()
                    var lastHeartbeatAt = android.os.SystemClock.elapsedRealtime()
                    val result =
                        LevelMetadataAnalyzer.analyze(
                            context,
                            target,
                            background = true,
                            priority = priority,
                            totalTimeoutMs = TOTAL_TIMEOUT_MS,
                            onProgress = { progress ->
                                val now = android.os.SystemClock.elapsedRealtime()
                                if (now - lastHeartbeatAt >= 30_000L) {
                                    monitor.heartbeat(
                                        RouteMetadataPrecomputeJob(target, sourceIdentity, enabled = true),
                                        priority,
                                        progress.currentLevel?.label ?: progress.overall.label,
                                    )
                                    lastHeartbeatAt = now
                                }
                                progressTracker.onAnalysisProgress(level.levelNum, progress)?.let {
                                    onCurrentProgress(it.permille, it.state)
                                }
                            },
                        )
                    val row = result.levels.singleOrNull()
                    val cacheFile = row?.routeCacheFile.orEmpty()
                    val cachePublished = cacheFile.isNotBlank() && cacheArtifact(context, game, cacheFile).isFile
                    val assessment =
                        RouteMetadataAttemptClassifier.classify(
                            result,
                            cachePublished,
                            ledger.read(jobId),
                        )
                    val recorded = ledger.record(jobId, assessment)
                    monitor.inGameLevelFinished(
                        mission,
                        level.levelNum,
                        level.levelFile,
                        recorded.status,
                        priority,
                        System.currentTimeMillis() - analysisStartedAt,
                    )
                    RouteMetadataDiagnostics.log(
                        "Route metadata in-game level=${level.levelNum} " +
                            "priority=${priority.wireName} status=${recorded.status.wireName} " +
                            "readiness=${row?.routeReadiness.orEmpty()} progress=${recorded.progressToken}",
                    )

                    if (level.levelNum == currentLevelNum &&
                        cachePublished &&
                        row?.routeReadiness in setOf("next_ready", "complete")
                    ) {
                        progressTracker.onReadiness(level.levelNum, row?.routeReadiness.orEmpty())?.let {
                            onCurrentProgress(it.permille, it.state)
                        }
                        reportCurrent(true)
                        priority = RouteMetadataPriority.NEXT
                    }
                    if (assessment.status == RouteMetadataLedgerStatus.COMPLETE) {
                        if (level.levelNum == currentLevelNum) reportCurrent(true)
                        break
                    }
                    if (assessment.status == RouteMetadataLedgerStatus.FAILED || !assessment.shouldRetry) {
                        if (level.levelNum == currentLevelNum) {
                            progressTracker.onFailure(level.levelNum)?.let {
                                onCurrentProgress(it.permille, it.state)
                            }
                            reportCurrent(false)
                        }
                        Log.w(
                            "DXX-RouteMetadata",
                            "Route metadata stopped level=${level.levelNum} kind=${assessment.failureKind}",
                        )
                        break
                    }
                    attempt++
                    delay(minOf(attempt * RETRY_DELAY_MS, 10_000L))
                }
            }
            monitor.inGameMissionFinished(mission, levels.size)
        } catch (e: CancellationException) {
            throw e
        } finally {
            reportCurrent(false)
        }
    }

    private fun cacheArtifact(
        context: Context,
        game: String,
        filename: String,
    ): File {
        val gameDir = if (game == GameFileFormats.GAME_D1) "d1x-redux" else "d2x-redux"
        return File(File(context.filesDir, gameDir), filename)
    }
}

internal enum class RouteMetadataProgressState(
    val wireValue: Int,
) {
    CALCULATING(0),
    USEFUL(1),
    COMPLETE(2),
    FAILED(3),
}

internal data class RouteMetadataProgressEvent(
    val permille: Int,
    val state: RouteMetadataProgressState,
)

internal class RouteMetadataCurrentProgressTracker(
    private val currentLevelNum: Int,
) {
    private var permille = 0

    fun onAnalysisProgress(
        levelNum: Int,
        progress: LevelMetadataAnalysisProgress,
    ): RouteMetadataProgressEvent? {
        if (levelNum != currentLevelNum) return null
        permille = maxOf(permille, progress.estimatedLevel?.completed ?: 0).coerceAtMost(999)
        return RouteMetadataProgressEvent(permille, RouteMetadataProgressState.CALCULATING)
    }

    fun onReadiness(
        levelNum: Int,
        readiness: String,
    ): RouteMetadataProgressEvent? {
        if (levelNum != currentLevelNum) return null
        return when (readiness) {
            "complete" -> {
                permille = LevelMetadataLevelProgressEstimator.TOTAL
                RouteMetadataProgressEvent(permille, RouteMetadataProgressState.COMPLETE)
            }

            "next_ready" -> {
                permille = permille.coerceAtMost(999)
                RouteMetadataProgressEvent(permille, RouteMetadataProgressState.USEFUL)
            }

            else -> {
                null
            }
        }
    }

    fun onFailure(levelNum: Int): RouteMetadataProgressEvent? =
        if (levelNum == currentLevelNum) {
            RouteMetadataProgressEvent(permille, RouteMetadataProgressState.FAILED)
        } else {
            null
        }
}
