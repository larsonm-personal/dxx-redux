package com.dxxredux.app

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.File

internal data class RouteMetadataPrecomputeJob(
    val target: LevelMetadataTarget,
    val sourceIdentity: String,
    val enabled: Boolean,
) {
    val id: String =
        listOf(
            target.game,
            target.missionName.orEmpty(),
            target.levelNum.toString(),
            target.levelFile.orEmpty(),
            sourceIdentity,
        ).joinToString("|")
}

internal object RouteMetadataPrecomputeOrdering {
    fun order(
        jobs: List<RouteMetadataPrecomputeJob>,
        recent: ResumeSaveBridge.ResumeSaveCandidate?,
    ): List<RouteMetadataPrecomputeJob> =
        jobs.sortedWith(
            compareBy<RouteMetadataPrecomputeJob>(
                { if (matchesRecentLevel(it, recent)) 0 else 1 },
                { if (it.target.game == recent?.game) 0 else 1 },
                { if (it.enabled) 0 else 1 },
                { if (isBaseGame(it)) 0 else 1 },
                { if (it.target.levelNum > 0) 0 else 1 },
                { kotlin.math.abs(it.target.levelNum) },
                { it.target.displayName.lowercase() },
            ),
        )

    private fun matchesRecentLevel(
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
        val sourceName = job.target.sourcePath?.let(::File)?.name ?: job.target.hogFile.orEmpty()
        return job.target.sourceType == "hog" && LevelMetadataTargets.isBaseHog(sourceName)
    }
}

internal class RouteMetadataPrecomputeCoordinator(
    context: Context,
    private val scope: CoroutineScope,
) {
    private val appContext = context.applicationContext
    private val stateFile = File(appContext.filesDir, "route_metadata_precompute.json")
    private val failureCount = mutableMapOf<String, Int>()
    private val retryAfter = mutableMapOf<String, Long>()
    private var runner: Job? = null

    fun start() {
        if (runner?.isActive == true) return
        runner =
            scope.launch {
                while (isActive) {
                    if (shouldPause()) {
                        delay(2_000L)
                        continue
                    }
                    val completed = loadCompleted()
                    val recent = ResumeSaveBridge.findOptions(appContext.filesDir)?.latestOverall
                    val next =
                        RouteMetadataPrecomputeOrdering
                            .order(discoverJobs(), recent)
                            .firstOrNull {
                                !isCompleted(it, completed) &&
                                    System.currentTimeMillis() >= (retryAfter[it.id] ?: 0L)
                            }
                    if (next == null) {
                        delay(30_000L)
                        continue
                    }
                    try {
                        val result =
                            LevelMetadataAnalyzer.analyze(
                                appContext,
                                next.target,
                                background = true,
                                totalTimeoutMs = 10L * 60L * 1_000L,
                            )
                        val cacheFile =
                            result.levels
                                .singleOrNull()
                                ?.routeCacheFile
                                .orEmpty()
                        val routeComplete =
                            result.levels.singleOrNull()?.routeStatus == "ok"
                        if (result.status == "ok" && routeComplete &&
                            cacheArtifact(next.target.game, cacheFile).isFile
                        ) {
                            completed[next.id] = cacheFile
                            saveCompleted(completed)
                        } else {
                            Log.w(
                                TAG,
                                "Precompute failed ${next.target.displayName} " +
                                    "level=${next.target.levelNum}: ${result.problems.joinToString()}",
                            )
                            val busy = result.problems.any { it.contains("already running", ignoreCase = true) }
                            val attempts = if (busy) 0 else (failureCount[next.id] ?: 0) + 1
                            failureCount[next.id] = attempts
                            if (attempts >= 3) {
                                // Bound repeated work for this launcher session, but do not
                                // persist a permanent failure across app or planner updates.
                                retryAfter[next.id] = Long.MAX_VALUE
                            } else {
                                retryAfter[next.id] =
                                    System.currentTimeMillis() +
                                    if (busy) 2_000L else (1L shl attempts) * 10_000L
                            }
                        }
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        Log.e(TAG, "Precompute worker failed", e)
                        delay(10_000L)
                    }
                    delay(750L)
                }
            }
    }

    fun stop() {
        runner?.cancel()
        runner = null
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
                )
            }
        return normal + secret
    }

    private fun isCompleted(
        job: RouteMetadataPrecomputeJob,
        completed: MutableMap<String, String>,
    ): Boolean {
        val filename = completed[job.id] ?: return false
        if (cacheArtifact(job.target.game, filename).isFile) return true
        completed.remove(job.id)
        return false
    }

    private fun cacheArtifact(
        game: String,
        filename: String,
    ): File {
        val gameDir = if (game == GameFileFormats.GAME_D1) "d1x-redux" else "d2x-redux"
        return File(File(appContext.filesDir, gameDir), filename)
    }

    private fun loadCompleted(): MutableMap<String, String> =
        runCatching {
            val root = JSONObject(stateFile.readText(Charsets.UTF_8))
            val values = root.optJSONObject("completed") ?: JSONObject()
            values
                .keys()
                .asSequence()
                .associateWith { values.getString(it) }
                .toMutableMap()
        }.getOrDefault(mutableMapOf())

    private fun saveCompleted(completed: Map<String, String>) {
        val entries = JSONObject()
        completed.toSortedMap().forEach { (id, filename) -> entries.put(id, filename) }
        val root =
            JSONObject()
                .put("schema", "dxx-route-precompute-v1")
                .put("completed", entries)
        AtomicFilePublication.writeUtf8(stateFile, root.toString(2) + "\n")
    }

    private companion object {
        const val TAG = "DXX-RoutePrecompute"
    }
}
