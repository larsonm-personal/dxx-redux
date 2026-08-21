package com.dxxredux.app

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.isActive
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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

internal data class MissionMusicPrecomputeJob(
    val displayName: String,
    val routeSourceIdentity: String,
    val catalog: MissionZipMusicCatalog,
    val outputFile: File,
    val enabled: Boolean,
) {
    val tracks: List<MissionZipMusicTrack> =
        catalog.sources
            .flatMap { it.tracks }
            .filter(MissionZipAudioFingerprintCache::isFingerprintSupported)
            .distinctBy { it.id }
}

internal object MissionMusicPrecomputeScheduling {
    fun isEligible(
        job: MissionMusicPrecomputeJob,
        routeJobs: List<RouteMetadataPrecomputeJob>,
        isRouteTerminal: (RouteMetadataPrecomputeJob) -> Boolean,
    ): Boolean {
        val missionRoutes = routeJobs.filter { it.sourceIdentity == job.routeSourceIdentity }
        return missionRoutes.isNotEmpty() && missionRoutes.all(isRouteTerminal)
    }

    fun shouldRunBeforeRoute(routePriority: RouteMetadataPriority?): Boolean =
        routePriority != RouteMetadataPriority.NEXT
}

private data class DiscoveredPrecomputeJobs(
    val routes: List<RouteMetadataPrecomputeJob>,
    val music: List<MissionMusicPrecomputeJob>,
)

internal object RouteMetadataPrecomputeOrdering {
    fun order(
        jobs: List<RouteMetadataPrecomputeJob>,
        recent: ResumeSaveBridge.ResumeSaveCandidate?,
        focusedSourceIdentity: String? = null,
        attemptedAtMs: (RouteMetadataPrecomputeJob) -> Long = { 0L },
    ): List<RouteMetadataPrecomputeJob> =
        jobs.sortedWith(
            compareBy<RouteMetadataPrecomputeJob>(
                { if (it.sourceIdentity == focusedSourceIdentity) 0 else 1 },
                { attemptedAtMs(it) },
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

internal object RouteMetadataPrecomputeFocusBroker {
    @Volatile private var handler: ((LevelMetadataTarget?) -> Unit)? = null

    @Synchronized
    fun attach(value: (LevelMetadataTarget?) -> Unit) {
        handler = value
    }

    @Synchronized
    fun detach(value: (LevelMetadataTarget?) -> Unit) {
        if (handler === value) handler = null
    }

    fun focus(target: LevelMetadataTarget?) {
        handler?.invoke(target)
    }
}

internal class RouteMetadataPrecomputeCoordinator(
    context: Context,
    private val scope: CoroutineScope,
) {
    private val appContext = context.applicationContext
    private val ledger = RouteMetadataLedger(appContext.filesDir)
    private val monitor = RouteMetadataPrecomputeMonitor(appContext.filesDir)
    private val wakeups = Channel<Unit>(Channel.CONFLATED)
    private var runner: Job? = null

    @Volatile private var focusNewestSource = false

    @Volatile private var gameLaunchPending = false

    @Volatile private var metadataViewerFocused = false

    @Volatile private var runningPriority: RouteMetadataPriority? = null
    private var focusedSourceIdentity: String? = null
    private var lastSourceIdentity: String? = null
    private val musicFailures = mutableSetOf<String>()
    private val loggedMusicMissions = mutableSetOf<String>()

    @Synchronized
    fun notifyContentImported() {
        monitor.contentImported()
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
            monitor.coordinatorEvent("preempted", "new content imported")
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

    fun setMetadataViewerFocus(target: LevelMetadataTarget?) {
        if (target != null) {
            metadataViewerFocused = true
            stop("metadata viewer opened: ${target.displayName}")
        } else {
            metadataViewerFocused = false
            monitor.coordinatorEvent("resuming", "metadata viewer closed")
            start()
        }
    }

    @Synchronized
    fun start() {
        if (gameLaunchPending || metadataViewerFocused) return
        if (runner?.isActive == true) return
        monitor.coordinatorEvent(
            "started",
            "launcher visible cpu_duty_percent=${RouteMetadataCpuPolicy.LAUNCHER_VISIBLE_DUTY_PERCENT}",
        )
        runner =
            scope.launch {
                val cleanup =
                    withContext(kotlinx.coroutines.Dispatchers.IO) {
                        RouteMetadataCacheMaintenance.prune(appContext.filesDir)
                    }
                if (cleanup.removedFiles > 0 || cleanup.removedDirectories > 0) {
                    monitor.coordinatorEvent(
                        "cache_pruned",
                        "files=${cleanup.removedFiles} directories=${cleanup.removedDirectories}",
                    )
                }
                val fingerprintCache = MissionZipAudioFingerprintCache(appContext.filesDir)
                val musicStageManager = MissionZipMusicStageManager(appContext.cacheDir)
                val fingerprintDbIdentity =
                    withContext(kotlinx.coroutines.Dispatchers.IO) {
                        FingerprintBridge.databaseIdentity(appContext)
                    }
                var discoveryFinished = false
                monitor.discoveryStarted()
                while (isActive) {
                    if (shouldPause()) {
                        awaitWake(2_000L)
                        continue
                    }
                    val discovered =
                        try {
                            ResumeSaveBridge.findOptions(appContext.filesDir)?.latestOverall to discoverJobs()
                        } catch (e: CancellationException) {
                            throw e
                        } catch (e: Exception) {
                            monitor.discoveryFailed(e)
                            Log.e(TAG, "Route metadata job discovery failed", e)
                            awaitWake(10_000L)
                            continue
                        }
                    val recent = discovered.first
                    val jobs = discovered.second.routes
                    val musicJobs = discovered.second.music
                    val entries = ledger.entries()
                    if (!discoveryFinished) {
                        monitor.discoveryFinished(jobs.size)
                        monitor.musicDiscovery(musicJobs.sumOf { it.tracks.size })
                        discoveryFinished = true
                    }
                    if (focusNewestSource) {
                        focusedSourceIdentity = jobs.maxByOrNull { it.sourceModifiedMs }?.sourceIdentity
                        focusNewestSource = false
                    }
                    val nextRoute =
                        RouteMetadataPrecomputeOrdering
                            .order(jobs, recent, focusedSourceIdentity) { entries[it.id]?.updatedAtMs ?: 0L }
                            .firstOrNull {
                                !isCompleted(it) && entries[it.id]?.status != RouteMetadataLedgerStatus.FAILED
                            }
                    val routePriority =
                        nextRoute?.let {
                            if (it.sourceIdentity == focusedSourceIdentity ||
                                RouteMetadataPrecomputeOrdering.matchesRecentLevel(it, recent)
                            ) {
                                RouteMetadataPriority.NEXT
                            } else {
                                RouteMetadataPriority.FILL
                            }
                        }
                    val musicEntries =
                        withContext(kotlinx.coroutines.Dispatchers.IO) {
                            musicJobs.associateWith { fingerprintCache.cachedEntries(it.catalog) }
                        }
                    val currentMusicIds =
                        musicJobs.flatMap { job -> job.tracks.map { musicTrackId(job, it) } }.toSet()
                    musicFailures.retainAll(currentMusicIds)
                    loggedMusicMissions.retainAll(musicJobs.map { it.catalog.sourceIdentity }.toSet())
                    val eligibleMusic =
                        musicJobs.filter { job ->
                            MissionMusicPrecomputeScheduling.isEligible(job, jobs) { route ->
                                isRouteTerminal(route, entries)
                            }
                        }
                    val nextMusic =
                        eligibleMusic
                            .sortedWith(
                                compareByDescending<MissionMusicPrecomputeJob> {
                                    it.routeSourceIdentity == focusedSourceIdentity
                                }.thenByDescending { it.enabled }
                                    .thenBy { it.displayName.lowercase() },
                            ).firstOrNull { job ->
                                job.tracks.any { track ->
                                    !isMusicTrackTerminal(
                                        job,
                                        track,
                                        musicEntries[job].orEmpty(),
                                        fingerprintDbIdentity,
                                    )
                                }
                            }
                    val totalMusicTracks = musicJobs.sumOf { it.tracks.size }
                    val finishedMusicTracks =
                        musicJobs.sumOf { job ->
                            job.tracks.count { track ->
                                isMusicTrackTerminal(
                                    job,
                                    track,
                                    musicEntries[job].orEmpty(),
                                    fingerprintDbIdentity,
                                )
                            }
                        }
                    val failedMusicTracks = musicFailures.size
                    val waitingMusicTracks =
                        musicJobs
                            .filterNot { it in eligibleMusic }
                            .sumOf { job ->
                                job.tracks.count { track ->
                                    !isMusicTrackTerminal(
                                        job,
                                        track,
                                        musicEntries[job].orEmpty(),
                                        fingerprintDbIdentity,
                                    )
                                }
                            }
                    monitor.update(jobs, entries)
                    monitor.updateMusic(
                        MissionMusicPrecomputeProgress(
                            totalTracks = totalMusicTracks,
                            finishedTracks = finishedMusicTracks,
                            failedTracks = failedMusicTracks,
                            waitingTracks = waitingMusicTracks,
                            phase =
                                when {
                                    totalMusicTracks == 0 -> "empty"
                                    finishedMusicTracks == totalMusicTracks -> "complete"
                                    nextMusic == null -> "waiting_for_metadata"
                                    else -> "queued"
                                },
                        ),
                    )
                    if (nextMusic != null && MissionMusicPrecomputeScheduling.shouldRunBeforeRoute(routePriority)) {
                        val track =
                            nextMusic.tracks.first { candidate ->
                                !isMusicTrackTerminal(
                                    nextMusic,
                                    candidate,
                                    musicEntries[nextMusic].orEmpty(),
                                    fingerprintDbIdentity,
                                )
                            }
                        runningPriority = RouteMetadataPriority.FILL
                        try {
                            processMusicTrack(
                                nextMusic,
                                track,
                                fingerprintCache,
                                musicStageManager,
                                fingerprintDbIdentity,
                                totalMusicTracks,
                                finishedMusicTracks,
                                failedMusicTracks,
                                waitingMusicTracks,
                            )
                        } finally {
                            runningPriority = null
                        }
                        awaitWake(750L)
                        continue
                    }
                    if (nextRoute == null) {
                        focusedSourceIdentity = null
                        awaitWake(30_000L)
                        continue
                    }
                    try {
                        val next = nextRoute
                        val priority = checkNotNull(routePriority)
                        runningPriority = priority
                        if (lastSourceIdentity != next.sourceIdentity) {
                            val reason =
                                when {
                                    next.sourceIdentity == focusedSourceIdentity -> "newly imported content"

                                    RouteMetadataPrecomputeOrdering.matchesRecentLevel(
                                        next,
                                        recent,
                                    ) -> "most recent level"

                                    lastSourceIdentity != null -> "next highest priority analysis"

                                    else -> "background analysis started"
                                }
                            monitor.prioritySwitch(
                                next.target.displayName,
                                "${next.target.levelNum}:${next.target.levelFile.orEmpty()}",
                                reason,
                                priority,
                            )
                            lastSourceIdentity = next.sourceIdentity
                        }
                        monitor.update(jobs, ledger.entries(), next, priority)
                        val analysisStartedAt = System.currentTimeMillis()
                        var lastProgressWriteAt = 0L
                        var lastHeartbeatAt = SystemClock.elapsedRealtime()
                        var lastProgressLabel = ""
                        val result =
                            try {
                                LevelMetadataAnalyzer.analyze(
                                    appContext,
                                    next.target,
                                    background = true,
                                    priority = priority,
                                    cpuDutyPercent = RouteMetadataCpuPolicy.LAUNCHER_VISIBLE_DUTY_PERCENT,
                                    totalTimeoutMs = 10L * 60L * 1_000L,
                                    onProgress = { progress ->
                                        val now = SystemClock.elapsedRealtime()
                                        val label = progress.currentLevel?.label ?: progress.overall.label
                                        if (label != lastProgressLabel || now - lastProgressWriteAt >= 1_000L) {
                                            monitor.analysisProgress(next, priority, progress)
                                            lastProgressWriteAt = now
                                            lastProgressLabel = label
                                        }
                                        if (now - lastHeartbeatAt >= 30_000L) {
                                            monitor.heartbeat(next, priority, label)
                                            lastHeartbeatAt = now
                                        }
                                    },
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
                        val recorded = ledger.record(next.id, assessment)
                        val entries = ledger.entries()
                        monitor.levelFinished(
                            next,
                            recorded.status,
                            System.currentTimeMillis() - analysisStartedAt,
                            jobs,
                            entries,
                        )
                        monitor.update(jobs, entries)
                        if (recorded.status == RouteMetadataLedgerStatus.PARTIAL) {
                            monitor.retryDeferred(next)
                        }
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
    fun stop(reason: String = "requested") {
        if (runner != null) monitor.coordinatorEvent("stopped", reason)
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

    suspend fun clearCache(): RouteMetadataCacheCleanupResult {
        stopAndAwait()
        return try {
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                RouteMetadataCacheMaintenance.clear(appContext.filesDir)
            }
        } finally {
            start()
        }
    }

    suspend fun stopForGameLaunch(timeoutMs: Long = 8_000L): Boolean {
        gameLaunchPending = true
        monitor.launchHandoff("started")
        val startedAt = SystemClock.elapsedRealtime()
        val stopped =
            withTimeoutOrNull(timeoutMs) {
                stopAndAwait()
                true
            } ?: false
        val terminatedProcesses = LevelMetadataAnalyzer.terminateWorkerProcessesForGameLaunch(appContext)
        monitor.launchHandoff(
            if (stopped) "complete" else "timeout",
            SystemClock.elapsedRealtime() - startedAt,
            terminatedProcesses,
        )
        return stopped
    }

    @Synchronized
    fun resumeAfterGame() {
        val returningFromGame = gameLaunchPending
        gameLaunchPending = false
        if (returningFromGame) monitor.coordinatorEvent("resuming", "game ended")
        start()
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

    private fun discoverJobs(): DiscoveredPrecomputeJobs {
        val fileSets = FileSetManager(appContext.filesDir)
        val setDir = fileSets.getSetDir(fileSets.getActive())
        val targets = mutableListOf<Pair<LevelMetadataTarget, Boolean>>()
        val musicJobs = mutableListOf<MissionMusicPrecomputeJob>()
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
        val extractionStore = MissionZipExtractionStore(appContext.filesDir)
        modManager
            .listMods()
            .forEach { mod ->
                val archive = File(File(appContext.filesDir, "mods"), mod.filename)
                val modTargets =
                    if (mod.kind == ModManager.MOD_KIND_MISSION_ZIP) {
                        val scan = runCatching { MissionZip.inspect(archive) }.getOrNull()
                        val extractionRecord = extractionStore.freshRecord(mod.filename, archive)
                        val musicCatalog =
                            runCatching {
                                extractionRecord?.let { MissionZipMusic.inspectExtracted(it) }
                                    ?: MissionZipMusic.inspect(archive)
                            }.getOrNull()
                        musicCatalog?.let { catalog ->
                            val outputFile =
                                extractionRecord?.let { File(it.rootDir, MISSION_ZIP_MUSIC_NAMES_FILE) }
                                    ?: MissionZipMusicNames.cacheFile(appContext.filesDir, mod.filename)
                            musicJobs +=
                                MissionMusicPrecomputeJob(
                                    displayName = mod.displayName,
                                    routeSourceIdentity = sourceIdentity(archive),
                                    catalog = catalog,
                                    outputFile = outputFile,
                                    enabled = mod.enabled,
                                )
                        }
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
        return DiscoveredPrecomputeJobs(
            routes = targets.flatMap { (target, enabled) -> splitTarget(target, enabled) },
            music = musicJobs.filter { it.tracks.isNotEmpty() },
        )
    }

    private fun splitTarget(
        target: LevelMetadataTarget,
        enabled: Boolean,
    ): List<RouteMetadataPrecomputeJob> {
        val source = target.archivePath?.let(::File) ?: target.sourcePath?.let(::File)
        val sourceIdentity = source?.let(::sourceIdentity) ?: target.displayName
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

    private fun sourceIdentity(source: File): String =
        "${source.absolutePath}|${source.length()}|${source.lastModified()}"

    private fun isRouteTerminal(
        job: RouteMetadataPrecomputeJob,
        entries: Map<String, RouteMetadataLedgerEntry>,
    ): Boolean = entries[job.id]?.status == RouteMetadataLedgerStatus.FAILED || isCompleted(job)

    private fun musicTrackId(
        job: MissionMusicPrecomputeJob,
        track: MissionZipMusicTrack,
    ): String = "${job.catalog.sourceIdentity}|${track.id}"

    private fun isMusicTrackTerminal(
        job: MissionMusicPrecomputeJob,
        track: MissionZipMusicTrack,
        entries: Map<String, MissionZipAudioFingerprintCache.Entry>,
        fingerprintDbIdentity: String,
    ): Boolean =
        musicTrackId(job, track) in musicFailures ||
            entries[track.id]?.let {
                it.chromaprint.isNotBlank() && it.localMatchDbIdentity == fingerprintDbIdentity
            } == true

    private suspend fun processMusicTrack(
        job: MissionMusicPrecomputeJob,
        track: MissionZipMusicTrack,
        fingerprintCache: MissionZipAudioFingerprintCache,
        stageManager: MissionZipMusicStageManager,
        fingerprintDbIdentity: String,
        totalTracks: Int,
        finishedTracks: Int,
        failedTracks: Int,
        waitingTracks: Int,
    ) {
        monitor.updateMusic(
            MissionMusicPrecomputeProgress(
                totalTracks = totalTracks,
                finishedTracks = finishedTracks,
                failedTracks = failedTracks,
                waitingTracks = waitingTracks,
                currentMission = job.displayName,
                currentTrack = track.displayName,
                phase = "hashing",
            ),
        )
        monitor.musicTrackStarted(job.displayName, track.displayName)
        val startedAt = System.currentTimeMillis()
        val trackId = musicTrackId(job, track)
        try {
            val result =
                withContext(kotlinx.coroutines.Dispatchers.IO) {
                    kotlinx.coroutines.currentCoroutineContext().ensureActive()
                    stageManager.cleanupOldFiles()
                    val staged =
                        stageManager.stageCompressedAudioTrack(job.catalog, track)
                            ?: error("Could not stage audio track")
                    val entry =
                        fingerprintCache.identifyLocal(appContext, job.catalog, track, staged)
                            ?: error("Could not analyze audio track")
                    kotlinx.coroutines.currentCoroutineContext().ensureActive()
                    MissionZipMusicNames.writeSidecar(
                        job.outputFile,
                        job.catalog,
                        fingerprintCache.cachedEntries(job.catalog),
                        allowAcoustIdLookups =
                            appContext
                                .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
                                .getBoolean(PREF_ALLOW_ACOUSTID_WEB_LOOKUPS, false),
                    )
                    entry
                }
            musicFailures.remove(trackId)
            monitor.musicTrackFinished(
                job.displayName,
                track.displayName,
                result.hasLocalMatch,
                System.currentTimeMillis() - startedAt,
            )
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            musicFailures += trackId
            monitor.musicTrackFailed(
                job.displayName,
                track.displayName,
                e.message.orEmpty().ifBlank { e.javaClass.simpleName },
            )
            Log.w(TAG, "Music fingerprint failed ${job.displayName} track=${track.displayName}", e)
        }
        val refreshed =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                fingerprintCache.cachedEntries(job.catalog)
            }
        if (job.tracks.all { isMusicTrackTerminal(job, it, refreshed, fingerprintDbIdentity) } &&
            loggedMusicMissions.add(job.catalog.sourceIdentity)
        ) {
            monitor.musicMissionFinished(
                job.displayName,
                job.tracks.size,
                job.tracks.count { musicTrackId(job, it) in musicFailures },
            )
        }
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
