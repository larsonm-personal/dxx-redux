package com.dxxredux.app

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.io.File

internal data class RouteMetadataPrecomputeJob(
    val target: LevelMetadataTarget,
    val sourceIdentity: String,
    val enabled: Boolean,
    val sourceModifiedMs: Long = 0L,
) {
    val id: String =
        listOf(
            target.game,
            target.missionName.orEmpty(),
            target.levelNum.toString(),
            target.levelFile.orEmpty(),
            sourceIdentity,
            ROUTE_METADATA_CACHE_GENERATION.toString(),
        ).joinToString("|")
}

internal object RouteMetadataPrecomputeOrdering {
    fun order(
        jobs: List<RouteMetadataPrecomputeJob>,
        recent: ResumeSaveBridge.ResumeSaveCandidate?,
        focusedSourceIdentity: String? = null,
    ): List<RouteMetadataPrecomputeJob> =
        jobs.sortedWith(
            compareBy<RouteMetadataPrecomputeJob>(
                { if (it.sourceIdentity == focusedSourceIdentity) 0 else 1 },
                { if (matchesRecentLevel(it, recent)) 0 else 1 },
                { if (it.target.game == recent?.game) 0 else 1 },
                { if (it.enabled) 0 else 1 },
                { if (isBaseGame(it)) 0 else 1 },
                { if (it.target.levelNum > 0) 0 else 1 },
                { kotlin.math.abs(it.target.levelNum) },
                { it.target.displayName.lowercase() },
            ),
        )

    fun matchesRecentLevel(
        job: RouteMetadataPrecomputeJob,
        recent: ResumeSaveBridge.ResumeSaveCandidate?,
    ): Boolean =
        recent != null &&
            job.target.game == recent.game &&
            job.target.levelNum == recent.levelNum &&
            (
                recent.missionName.isBlank() ||
                    job.target.missionName
                        .orEmpty()
                        .equals(recent.missionName, ignoreCase = true)
            )

    private fun isBaseGame(job: RouteMetadataPrecomputeJob): Boolean {
        val sourceName =
            job.target.sourcePath
                ?.let(::File)
                ?.name ?: job.target.hogFile.orEmpty()
        return job.target.sourceType == "hog" && LevelMetadataTargets.isBaseHog(sourceName)
    }
}

internal class RouteMetadataPrecomputeCoordinator(
    context: Context,
    private val scope: CoroutineScope,
) {
    private val appContext = context.applicationContext
    private val ledger = RouteMetadataLedger(appContext.filesDir)
    private val wakeups = Channel<Unit>(Channel.CONFLATED)
    private var runner: Job? = null

    @Volatile private var focusNewestSource = false

    @Volatile private var runningPriority: RouteMetadataPriority? = null
    private var focusedSourceIdentity: String? = null

    @Synchronized
    fun notifyContentImported() {
        focusNewestSource = true
        wakeups.trySend(Unit)
        val previous = runner
        if (previous == null) {
            start()
        } else if (
            runningPriority == null ||
            RouteMetadataPreemption.shouldPreempt(
                runningPriority,
                RouteMetadataPriority.NEXT,
                replacesFocus = true,
            )
        ) {
            previous.cancel()
            previous.invokeOnCompletion {
                synchronized(this) {
                    if (runner === previous) {
                        runner = null
                        start()
                    }
                }
            }
        }
    }

    fun wake() {
        wakeups.trySend(Unit)
    }

    @Synchronized
    fun start() {
        if (runner?.isActive == true) return
        runner =
            scope.launch {
                while (isActive) {
                    if (shouldPause()) {
                        awaitWake(2_000L)
                        continue
                    }
                    val recent = ResumeSaveBridge.findOptions(appContext.filesDir)?.latestOverall
                    val jobs = discoverJobs()
                    if (focusNewestSource) {
                        focusedSourceIdentity = jobs.maxByOrNull { it.sourceModifiedMs }?.sourceIdentity
                        focusNewestSource = false
                    }
                    val next =
                        RouteMetadataPrecomputeOrdering
                            .order(jobs, recent, focusedSourceIdentity)
                            .firstOrNull {
                                !isCompleted(it) && ledger.read(it.id)?.status != RouteMetadataLedgerStatus.FAILED
                            }
                    if (next == null) {
                        focusedSourceIdentity = null
                        awaitWake(30_000L)
                        continue
                    }
                    try {
                        val priority =
                            if (next.sourceIdentity == focusedSourceIdentity ||
                                RouteMetadataPrecomputeOrdering.matchesRecentLevel(next, recent)
                            ) {
                                RouteMetadataPriority.NEXT
                            } else {
                                RouteMetadataPriority.FILL
                            }
                        runningPriority = priority
                        val result =
                            try {
                                LevelMetadataAnalyzer.analyze(
                                    appContext,
                                    next.target,
                                    background = true,
                                    priority = priority,
                                    totalTimeoutMs = 10L * 60L * 1_000L,
                                )
                            } finally {
                                runningPriority = null
                            }
                        val cacheFile =
                            result.levels
                                .singleOrNull()
                                ?.routeCacheFile
                                .orEmpty()
                        val assessment =
                            RouteMetadataAttemptClassifier.classify(
                                result,
                                cacheFile.isNotBlank() && cacheArtifact(next.target.game, cacheFile).isFile,
                                ledger.read(next.id),
                            )
                        ledger.record(next.id, assessment)
                        RouteMetadataDiagnostics.log(
                            "Route metadata precompute level=${next.target.levelNum} " +
                                "priority=${priority.wireName} status=${assessment.status.wireName} " +
                                "progress=${assessment.progressToken}",
                        )
                        if (assessment.status == RouteMetadataLedgerStatus.FAILED) {
                            Log.w(
                                TAG,
                                "Precompute failed ${next.target.displayName} " +
                                    "level=${next.target.levelNum} kind=${assessment.failureKind}: " +
                                    result.problems.joinToString(),
                            )
                        }
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        Log.e(TAG, "Precompute worker failed", e)
                        delay(10_000L)
                    }
                    awaitWake(750L)
                }
            }
    }

    @Synchronized
    fun stop() {
        runner?.cancel()
        runner = null
        runningPriority = null
    }

    suspend fun stopAndAwait() {
        val previous =
            synchronized(this) {
                runner.also {
                    it?.cancel()
                    runner = null
                    runningPriority = null
                }
            }
        listOfNotNull(previous).joinAll()
    }

    private suspend fun awaitWake(timeoutMs: Long) {
        withTimeoutOrNull(timeoutMs) { wakeups.receive() }
    }

    private fun shouldPause(): Boolean {
        val power = appContext.getSystemService(Context.POWER_SERVICE) as? PowerManager
        val gameRunning = runCatching { MainActivity.nativeIsGameRunning() }.getOrDefault(false)
        val thermalPressure =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
                power != null &&
                power.currentThermalStatus >= PowerManager.THERMAL_STATUS_SEVERE
        return gameRunning || power?.isPowerSaveMode == true || thermalPressure
    }

    private fun discoverJobs(): List<RouteMetadataPrecomputeJob> {
        val fileSets = FileSetManager(appContext.filesDir)
        val setDir = fileSets.getSetDir(fileSets.getActive())
        val targets = mutableListOf<Pair<LevelMetadataTarget, Boolean>>()
        setDir
            .walkTopDown()
            .maxDepth(2)
            .filter { it.isFile && it.extension.equals("hog", ignoreCase = true) }
            .forEach { file ->
                runCatching {
                    LevelMetadataTargets.directFile(
                        file,
                        setDir,
                        GameFileMetadata.summarizeLocalFile(file),
                    )
                }.getOrNull()?.let { targets += it to true }
            }
        val modManager = ModManager(appContext.filesDir, appContext)
        modManager
            .listMods()
            .forEach { mod ->
                val archive = File(File(appContext.filesDir, "mods"), mod.filename)
                val modTargets =
                    if (mod.kind == ModManager.MOD_KIND_MISSION_ZIP) {
                        val scan = runCatching { MissionZip.inspect(archive) }.getOrNull()
                        scan
                            ?.let {
                                runCatching {
                                    LevelMetadataTargets.missionZipTargets(archive.absolutePath, setDir, it)
                                }.getOrDefault(emptyList())
                            }.orEmpty()
                    } else {
                        listOfNotNull(
                            runCatching {
                                LevelMetadataTargets.genericZip(
                                    archive.absolutePath,
                                    setDir,
                                    mod.displayName,
                                    mod.game,
                                )
                            }.getOrNull(),
                        )
                    }
                modTargets.forEach { targets += it to mod.enabled }
            }
        return targets.flatMap { (target, enabled) -> splitTarget(target, enabled) }
    }

    private fun splitTarget(
        target: LevelMetadataTarget,
        enabled: Boolean,
    ): List<RouteMetadataPrecomputeJob> {
        val source = target.archivePath?.let(::File) ?: target.sourcePath?.let(::File)
        val sourceIdentity =
            source?.let { "${it.absolutePath}|${it.length()}|${it.lastModified()}" }
                ?: target.displayName
        val normal =
            target.normalLevelFiles.mapIndexed { index, level ->
                RouteMetadataPrecomputeJob(
                    target.copy(
                        levelFile = level,
                        levelNum = index + 1,
                        normalLevelFiles = listOf(level),
                        secretLevelFiles = emptyList(),
                    ),
                    sourceIdentity,
                    enabled,
                    source?.lastModified() ?: 0L,
                )
            }
        val secret =
            target.secretLevelFiles.mapIndexed { index, level ->
                RouteMetadataPrecomputeJob(
                    target.copy(
                        levelFile = level,
                        levelNum = -(index + 1),
                        normalLevelFiles = emptyList(),
                        secretLevelFiles = listOf(level),
                    ),
                    sourceIdentity,
                    enabled,
                    source?.lastModified() ?: 0L,
                )
            }
        return normal + secret
    }

    private fun isCompleted(job: RouteMetadataPrecomputeJob): Boolean {
        val filename = ledger.completedCache(job.id) ?: return false
        if (cacheArtifact(job.target.game, filename).isFile) return true
        ledger.update(job.id) { null }
        return false
    }

    private fun cacheArtifact(
        game: String,
        filename: String,
    ): File {
        val gameDir = if (game == GameFileFormats.GAME_D1) "d1x-redux" else "d2x-redux"
        return File(File(appContext.filesDir, gameDir), filename)
    }

    private companion object {
        const val TAG = "DXX-RoutePrecompute"
    }
}
