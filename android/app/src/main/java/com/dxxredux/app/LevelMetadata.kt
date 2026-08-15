package com.dxxredux.app

import android.app.ActivityManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.os.Process
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.RandomAccessFile
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

private const val LEVEL_METADATA_TIMEOUT_MS = 120_000L
private const val LEVEL_METADATA_POLL_MS = 200L
private const val LEVEL_METADATA_WORKER_START_TIMEOUT_MS = 5_000L
private const val LEVEL_METADATA_CANCELLATION_GRACE_MS = 2_000L
private const val LEVEL_METADATA_PROGRESS_EXTENSION_MS = 120_000L
private const val LEVEL_METADATA_MAX_ZIP_FILES = 240
private const val LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES = 256L * 1024L * 1024L
private const val LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES = 64L * 1024L * 1024L
private const val LEVEL_METADATA_D1_PROCESS_SUFFIX = ":levelmeta_d1"
private const val LEVEL_METADATA_D2_PROCESS_SUFFIX = ":levelmeta_d2"
private const val LEVEL_METADATA_CACHE_MAX_AGE_MS = 24L * 60L * 60L * 1_000L
private const val LEVEL_METADATA_WORKER_FILE = "worker.json"
private const val LEVEL_METADATA_QUEUED_FILE = "queued.json"
private const val LEVEL_METADATA_CANCELLATION_FILE = "cancel"
private const val LEVEL_METADATA_WORKER_OWNER_D1_FILE = "worker-owner-d1.json"
private const val LEVEL_METADATA_WORKER_OWNER_D2_FILE = "worker-owner-d2.json"
private const val LEVEL_METADATA_WORKER_IDENTITY_MAX_BYTES = 4_096L
private const val LEVEL_METADATA_GLOBAL_LOCK_FILE = "worker-global.lock"

internal data class LevelMetadataTarget(
    val displayName: String,
    val game: String,
    val sourceType: String,
    val sourcePath: String? = null,
    val dataDir: String? = null,
    val missionName: String? = null,
    val missionDisplayName: String? = null,
    val missionFilename: String? = null,
    val missionType: String? = null,
    val levelFile: String? = null,
    val levelNum: Int = 1,
    val hogFile: String? = null,
    val hogFiles: List<String> = emptyList(),
    val normalLevelFiles: List<String> = emptyList(),
    val secretLevelFiles: List<String> = emptyList(),
    val archivePath: String? = null,
    val archiveEntries: List<String> = emptyList(),
)

internal data class LevelMetadataCheckpointUpdate(
    val progress: MetadataLoadProgress,
    val activityId: String,
    val stage: String = "level_progress",
    val phase: String = "",
    val taskId: Int = 0,
    val levelIdentity: String = "",
    val analysisProgress: LevelMetadataAnalysisProgress =
        LevelMetadataAnalysisProgress(overall = progress),
)

internal data class LevelMetadataAnalysisProgress(
    val overall: MetadataLoadProgress,
    val currentLevel: MetadataLoadProgress? = null,
    val estimatedLevel: MetadataLoadProgress? = null,
)

internal class LevelMetadataLevelProgressEstimator {
    companion object {
        const val TOTAL = 1_000
        private const val SECRET_END = 100
        private const val TOPOLOGY_END = 200
        private const val SUMMARY_END = 300
        private const val ROUTE_SPAN = TOTAL - SUMMARY_END
        private const val FIRST_ROUTE_INNER_TASK_ID = 5
        private const val UNFINISHED_MAX = TOTAL - 1
    }

    private var levelIdentity = ""
    private var maximum = 0

    fun observe(update: LevelMetadataCheckpointUpdate): MetadataLoadProgress {
        if (update.levelIdentity.isNotBlank() && update.levelIdentity != levelIdentity) {
            levelIdentity = update.levelIdentity
            maximum = 0
        }
        val rawFraction =
            update.progress.fraction
                ?.toDouble()
                ?.coerceIn(0.0, 1.0) ?: 0.0
        val candidate =
            when {
                update.stage == "level_done" -> {
                    TOTAL
                }

                update.stage != "level_progress" -> {
                    maximum
                }

                update.phase == "secret_areas" -> {
                    (rawFraction * SECRET_END).toInt()
                }

                update.phase == "level_topology" -> {
                    SECRET_END + (rawFraction * (TOPOLOGY_END - SECRET_END)).toInt()
                }

                update.phase == "level_summary" -> {
                    TOPOLOGY_END + (rawFraction * (SUMMARY_END - TOPOLOGY_END)).toInt()
                }

                update.phase == "route_planning" && update.progress.completed >= update.progress.total -> {
                    TOTAL
                }

                else -> {
                    val completedInnerTasks =
                        (update.taskId - FIRST_ROUTE_INNER_TASK_ID).coerceAtLeast(0)
                    val estimatedInner = 1.0 - Math.pow(0.9, completedInnerTasks + rawFraction)
                    SUMMARY_END + (ROUTE_SPAN * estimatedInner).toInt()
                }
            }
        maximum = maxOf(maximum, candidate.coerceIn(0, if (candidate >= TOTAL) TOTAL else UNFINISHED_MAX))
        return MetadataLoadProgress("Estimated level progress", maximum, TOTAL)
    }
}

internal class LevelMetadataProgressDeadline(
    startedAtMs: Long,
    private val initialTimeoutMs: Long = LEVEL_METADATA_TIMEOUT_MS,
    private val progressExtensionMs: Long = LEVEL_METADATA_PROGRESS_EXTENSION_MS,
) {
    private var deadlineMs = startedAtMs + initialTimeoutMs
    private var activityId: String? = null
    private var creditedCompleted = 0

    fun observe(
        update: LevelMetadataCheckpointUpdate,
        nowMs: Long,
    ) {
        val progress = update.progress
        if (progress.total <= 0) return
        val completed = progress.completed.coerceIn(0, progress.total)
        if (update.activityId != activityId) {
            activityId = update.activityId
            creditedCompleted = completed
            deadlineMs = maxOf(deadlineMs, nowMs + progressExtensionMs)
            return
        }
        if (completed <= creditedCompleted) return
        creditedCompleted = completed
        deadlineMs = maxOf(deadlineMs, nowMs + progressExtensionMs)
    }

    fun isExpired(nowMs: Long): Boolean = nowMs >= deadlineMs
}

internal data class LevelMetadataWorkerIdentity(
    val requestId: String,
    val pid: Int,
    val processStartTicks: Long,
) {
    fun matches(
        expectedRequestId: String,
        runningPid: Int,
        runningStartTicks: Long?,
    ): Boolean =
        requestId == expectedRequestId &&
            pid == runningPid &&
            runningStartTicks == processStartTicks

    fun toJson(): String =
        JSONObject()
            .put("request_id", requestId)
            .put("pid", pid)
            .put("process_start_ticks", processStartTicks)
            .toString(2) + "\n"

    companion object {
        fun fromJson(text: String): LevelMetadataWorkerIdentity? =
            runCatching {
                val json = JSONObject(text)
                val requestId = json.optString("request_id")
                val pid = json.optInt("pid", -1)
                val processStartTicks = json.optLong("process_start_ticks", -1L)
                if (requestId.isBlank() || pid <= 0 || processStartTicks < 0L) return@runCatching null
                LevelMetadataWorkerIdentity(requestId, pid, processStartTicks)
            }.getOrNull()
    }
}

internal object LevelMetadataWorkerOwnerStore {
    @Synchronized
    fun publish(
        file: File,
        identity: LevelMetadataWorkerIdentity,
    ) {
        file.parentFile?.mkdirs()
        RandomAccessFile(file, "rw").use { owner ->
            owner.channel.lock().use {
                owner.setLength(0)
                owner.write(identity.toJson().toByteArray(Charsets.UTF_8))
                owner.fd.sync()
            }
        }
    }

    @Synchronized
    fun read(file: File): LevelMetadataWorkerIdentity? {
        if (!file.isFile) return null
        return runCatching {
            RandomAccessFile(file, "rw").use { owner ->
                owner.channel.lock().use { readLocked(owner) }
            }
        }.getOrNull()
    }

    @Synchronized
    fun clearIfOwned(
        file: File,
        identity: LevelMetadataWorkerIdentity,
    ) {
        if (!file.isFile) return
        runCatching {
            RandomAccessFile(file, "rw").use { owner ->
                owner.channel.lock().use {
                    if (readLocked(owner) == identity) {
                        owner.setLength(0)
                        owner.fd.sync()
                    }
                }
            }
        }
    }

    private fun readLocked(owner: RandomAccessFile): LevelMetadataWorkerIdentity? {
        val length = owner.length()
        if (length <= 0L || length > LEVEL_METADATA_WORKER_IDENTITY_MAX_BYTES) return null
        val bytes = ByteArray(length.toInt())
        owner.seek(0)
        owner.readFully(bytes)
        return LevelMetadataWorkerIdentity.fromJson(bytes.toString(Charsets.UTF_8))
    }
}

internal fun levelMetadataBackgroundRequestPriority(
    cacheRoot: File,
    identity: LevelMetadataWorkerIdentity,
): RouteMetadataPriority? =
    runCatching {
        val request = JSONObject(File(File(cacheRoot, identity.requestId), "request.json").readText(Charsets.UTF_8))
        if (request.optString("request_id") != identity.requestId || !request.optBoolean("background")) {
            return@runCatching null
        }
        RouteMetadataPriority.entries.firstOrNull { it.wireName == request.optString("priority") }
    }.getOrNull()

internal fun parseProcessStartTicks(stat: String): Long? {
    val commandEnd = stat.lastIndexOf(')')
    if (commandEnd < 0 || commandEnd + 1 >= stat.length) return null
    val fields = stat.substring(commandEnd + 1).trim().split(Regex("\\s+"))
    return fields.getOrNull(19)?.toLongOrNull()?.takeIf { it >= 0L }
}

private fun processStartTicks(pid: Int): Long? =
    runCatching { parseProcessStartTicks(File("/proc/$pid/stat").readText(Charsets.US_ASCII)) }.getOrNull()

private fun workerOwnerFile(
    cacheRoot: File,
    game: String,
): File =
    File(
        cacheRoot,
        if (game == GameFileFormats.GAME_D1) {
            LEVEL_METADATA_WORKER_OWNER_D1_FILE
        } else {
            LEVEL_METADATA_WORKER_OWNER_D2_FILE
        },
    )

internal data class LevelMetadataRouteOpenLink(
    val seg: Int = -1,
    val side: Int = -1,
    val wall: Int = -1,
)

internal data class LevelMetadataPosition(
    val x: Double,
    val y: Double,
    val z: Double,
)

internal data class LevelMetadataRouteStep(
    val index: Int,
    val kind: String,
    val activationKind: String = "",
    val calculated: Boolean = true,
    val label: String,
    val seg: Int = -1,
    val side: Int = -1,
    val wall: Int = -1,
    val labelPosition: LevelMetadataPosition? = null,
    val distance: Double = 0.0,
    val key: String = "",
    val canBeBypassed: Boolean = false,
    val keyCarrierObjnum: Int = -1,
    val trigger: Int = -1,
    val triggerType: String = "",
    val triggerTypeId: Int = -1,
    val opens: List<LevelMetadataRouteOpenLink> = emptyList(),
)

internal data class LevelMetadataLevelRow(
    val levelNum: Int,
    val secret: Boolean,
    val levelName: String,
    val levelFile: String,
    val robots: Int,
    val hostages: Int,
    val secrets: Int,
    val matcens: Int,
    val energyCenters: Int,
    val mineVolume: Double,
    val mineVolumeNormalized: Double,
    val mineVolumeText: String,
    val travelDistance: Double,
    val travelTimeSeconds: Int,
    val travelTimeText: String,
    val guidebotCount: Int,
    val guidebotPlaced: Boolean,
    val guidebotAccessible: Boolean,
    val guidebotPlacementNote: String,
    val guidebotNote: String,
    val routeStatus: String,
    val routeProblem: String,
    val routeNote: String,
    val routeCacheFile: String = "",
    val routeReadiness: String = "",
    val failureKind: String = "",
    val visibilityEntries: Int = 0,
    val visibilityCheckpointSequence: Int = 0,
    val routeSteps: List<LevelMetadataRouteStep>,
    val status: String,
    val problems: List<String>,
    val notes: List<String>,
)

internal data class LevelMetadataResult(
    val status: String,
    val source: String,
    val game: String,
    val missionName: String,
    val missionFilename: String,
    val coopStarts: String,
    val levels: List<LevelMetadataLevelRow>,
    val problems: List<String>,
    val diagnostics: List<String> = emptyList(),
    val failureKind: String = "",
) {
    companion object {
        fun fromJson(text: String): LevelMetadataResult {
            val obj = JSONObject(text)
            val levels = obj.optJSONArray("levels") ?: JSONArray()
            val rows =
                buildList {
                    for (index in 0 until levels.length()) {
                        val row = levels.optJSONObject(index) ?: continue
                        add(
                            LevelMetadataLevelRow(
                                levelNum = row.optInt("level_num"),
                                secret = row.optBoolean("secret"),
                                levelName = row.optString("level_name"),
                                levelFile = row.optString("level_file"),
                                robots = row.optInt("robots"),
                                hostages = row.optInt("hostages"),
                                secrets = row.optInt("secrets"),
                                matcens = row.optInt("matcens"),
                                energyCenters = row.optInt("energy_centers"),
                                mineVolume = row.optDouble("mine_volume"),
                                mineVolumeNormalized = row.optDouble("mine_volume_normalized"),
                                mineVolumeText = row.optString("mine_volume_text"),
                                travelDistance = row.optDouble("travel_distance"),
                                travelTimeSeconds = row.optInt("travel_time_seconds"),
                                travelTimeText = row.optString("travel_time_text"),
                                guidebotCount = row.optInt("guidebot_count"),
                                guidebotPlaced = row.optBoolean("guidebot_placed"),
                                guidebotAccessible = row.optBoolean("guidebot_accessible"),
                                guidebotPlacementNote = row.optString("guidebot_placement_note"),
                                guidebotNote = row.optString("guidebot_note"),
                                routeStatus = row.optString("route_status"),
                                routeProblem = row.optString("route_problem"),
                                routeNote = row.optString("route_note"),
                                routeCacheFile = row.optString("route_cache_file"),
                                routeReadiness = row.optString("route_readiness"),
                                failureKind = row.optString("failure_kind"),
                                visibilityEntries = row.optInt("visibility_entries"),
                                visibilityCheckpointSequence = row.optInt("visibility_checkpoint_sequence"),
                                routeSteps = row.optRouteSteps("route_steps"),
                                status = row.optString("status", "ok"),
                                problems = row.optStringList("problems"),
                                notes = row.optStringList("notes"),
                            ),
                        )
                    }
                }
            return LevelMetadataResult(
                status = obj.optString("status", "failed"),
                source = obj.optString("source"),
                game = obj.optString("game"),
                missionName = obj.optString("mission_name"),
                missionFilename = obj.optString("mission_filename"),
                coopStarts = obj.optString("coop_starts"),
                levels = rows,
                problems = obj.optStringList("problems"),
                diagnostics = obj.optStringList("diagnostics"),
                failureKind = obj.optString("failure_kind"),
            )
        }

        fun failed(
            source: String,
            game: String,
            problem: String,
            diagnostics: List<String> = emptyList(),
            status: String = "failed",
            failureKind: String = "analysis_failed",
        ): LevelMetadataResult =
            LevelMetadataResult(
                status = status,
                source = source,
                game = game,
                missionName = "",
                missionFilename = "",
                coopStarts = "",
                levels = emptyList(),
                problems = listOf(problem),
                diagnostics = diagnostics,
                failureKind = failureKind,
            )
    }
}

internal object LevelMetadataTargets {
    private val directExtensions = setOf("hog", "msn", "mn2", "rdl", "rl2", "sdl", "sl2")

    fun canAnalyzeFile(name: String): Boolean = GameFileFormats.extensionOf(name) in directExtensions

    fun canAnalyzeZipConstituent(name: String): Boolean = canAnalyzeFile(name)

    fun canAnalyzeMissionZip(scan: MissionZip.ScanResult): Boolean =
        scan.constituents.any { canAnalyzeZipConstituent(it.name) }

    fun missionZipTargets(
        archivePath: String,
        setDir: File,
        scan: MissionZip.ScanResult,
    ): List<LevelMetadataTarget> {
        val sets = scan.missionSets.ifEmpty { listOf(MissionZip.MissionSet(scan.mission, scan.constituents)) }
        return sets.mapNotNull { missionZipTarget(archivePath, setDir, scan, it) }
    }

    fun directFile(
        file: File,
        setDir: File,
        metadata: GameFileMetadata.Summary?,
    ): LevelMetadataTarget? {
        if (!file.isFile || !canAnalyzeFile(file.name)) return null
        val game = gameForFile(file.name, metadata?.contents?.map { it.name }.orEmpty()) ?: return null
        val ext = GameFileFormats.extensionOf(file.name)
        if (ext == "hog") {
            if (!isBaseHog(file.name)) {
                descriptorForHog(file, game)?.let { mission ->
                    return LevelMetadataTarget(
                        displayName = file.name,
                        game = game,
                        sourceType = "hog",
                        sourcePath = file.absolutePath,
                        dataDir = setDir.absolutePath,
                        missionName = file.name.substringBeforeLast('.'),
                        missionDisplayName = mission.displayName,
                        missionFilename = descriptorFileNameForHog(file, game),
                        missionType = mission.type,
                        hogFile = file.name,
                        normalLevelFiles = mission.levelNames,
                        secretLevelFiles = mission.secretLevelNames,
                    )
                }
            }
            hogLevelFiles(
                metadata?.contents.orEmpty(),
                baseGame = isBaseHog(file.name),
            ).takeIf { it.first.isNotEmpty() || it.second.isNotEmpty() }?.let {
                return LevelMetadataTarget(
                    displayName = file.name,
                    game = game,
                    sourceType = "hog",
                    sourcePath = file.absolutePath,
                    dataDir = setDir.absolutePath,
                    missionName = missionNameFor(file.name, game),
                    hogFile = file.name,
                    normalLevelFiles = it.first,
                    secretLevelFiles = it.second,
                )
            }
        }
        if (GameFileFormats.isMissionDescriptor(file.name)) {
            descriptorTargetForFile(file, setDir, game)?.let { return it }
        }
        return LevelMetadataTarget(
            displayName = file.name,
            game = game,
            sourceType = if (GameFileFormats.isLevelFile(file.name)) "level" else "mission",
            sourcePath = file.absolutePath,
            dataDir = setDir.absolutePath,
            missionName = missionNameFor(file.name, game),
            levelFile = if (GameFileFormats.isLevelFile(file.name)) file.name else null,
            levelNum = if (ext == "sdl" || ext == "sl2") -1 else 1,
        )
    }

    fun missionZip(
        archivePath: String,
        setDir: File,
        scan: MissionZip.ScanResult,
    ): LevelMetadataTarget? = missionZipTargets(archivePath, setDir, scan).firstOrNull()

    private fun missionZipTarget(
        archivePath: String,
        setDir: File,
        scan: MissionZip.ScanResult,
        missionSet: MissionZip.MissionSet,
    ): LevelMetadataTarget? {
        if (!canAnalyzeMissionZip(scan)) return null
        missionZipExtractedStoreForArchivePath(archivePath)
            ?.extractedTarget(archivePath, setDir, scan, missionSet)
            ?.let { return it }
        val entries = missionSet.constituents.map { it.path }
        val mission = missionSet.mission
        return LevelMetadataTarget(
            displayName = mission.displayName,
            game =
                mission.game.takeIf {
                    it == GameFileFormats.GAME_D1 || it == GameFileFormats.GAME_D2
                } ?: return null,
            sourceType = "mission_files",
            dataDir = setDir.absolutePath,
            archivePath = archivePath,
            archiveEntries = entries,
            missionName =
                mission.path
                    .substringAfterLast('/')
                    .substringBeforeLast('.'),
            missionDisplayName = mission.displayName,
            missionFilename = mission.path.substringAfterLast('/').substringAfterLast('\\'),
            missionType = mission.type,
            hogFiles =
                missionSet.constituents
                    .filter { GameFileFormats.extensionOf(it.name) == "hog" }
                    .map { it.name },
            normalLevelFiles = mission.levelNames,
            secretLevelFiles = mission.secretLevelNames,
        )
    }

    fun genericZip(
        archivePath: String,
        setDir: File,
        displayName: String,
        gameHint: String,
    ): LevelMetadataTarget? {
        val archive = File(archivePath)
        if (!archive.isFile) return null
        val archiveEntries = mutableListOf<String>()
        val hogFiles = mutableListOf<String>()
        val normalLevels = mutableListOf<String>()
        val secretLevels = mutableListOf<String>()
        val gameHints = mutableListOf<String>()
        ArchiveFiles.open(archive).use { source ->
            val entries =
                source
                    .entries
                    .asSequence()
                    .filterNot { it.isDirectory }
                    .take(LEVEL_METADATA_MAX_ZIP_FILES + 1)
                    .toList()
            if (entries.size > LEVEL_METADATA_MAX_ZIP_FILES) return null
            entries.forEach { entry ->
                val name = entry.name
                when {
                    GameFileFormats.isLevelFile(name) -> {
                        archiveEntries += entry.path
                        gameHints += GameFileFormats.gameForLevel(name).orEmpty()
                        if (GameFileFormats.extensionOf(name) in setOf("sdl", "sl2")) {
                            secretLevels += name
                        } else {
                            normalLevels += name
                        }
                    }

                    GameFileFormats.extensionOf(name) == "hog" -> {
                        val summary =
                            runCatching {
                                GameFileMetadata.summarizeZipConstituent(archive, entry.path, name)
                            }.getOrNull()
                        val hogLevels = hogLevelFiles(summary?.contents.orEmpty())
                        if (hogLevels.first.isNotEmpty() || hogLevels.second.isNotEmpty()) {
                            archiveEntries += entry.path
                            hogFiles += name
                            normalLevels += hogLevels.first
                            secretLevels += hogLevels.second
                            gameForFile(name, summary?.contents?.map { it.name }.orEmpty())?.let { gameHints += it }
                        }
                    }
                }
            }
        }
        if (archiveEntries.isEmpty() || (normalLevels.isEmpty() && secretLevels.isEmpty())) return null
        val game =
            gameHint
                .takeIf { it == GameFileFormats.GAME_D1 || it == GameFileFormats.GAME_D2 }
                ?: gameHints
                    .filter { it == GameFileFormats.GAME_D1 || it == GameFileFormats.GAME_D2 }
                    .distinct()
                    .singleOrNull()
                ?: return null
        return LevelMetadataTarget(
            displayName = displayName,
            game = game,
            sourceType = "mission_files",
            dataDir = setDir.absolutePath,
            archivePath = archivePath,
            archiveEntries = archiveEntries,
            missionName = displayName.substringBeforeLast('.'),
            hogFiles = hogFiles,
            normalLevelFiles = normalLevels,
            secretLevelFiles = secretLevels,
        )
    }

    fun zipConstituent(
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        metadata: GameFileMetadata.Summary? = null,
    ): LevelMetadataTarget? {
        if (!canAnalyzeZipConstituent(constituent.name)) return null
        extractedConstituentTarget(archivePath, setDir, constituent, metadata)?.let { return it }
        val game = gameForFile(constituent.name, metadata?.contents?.map { it.name }.orEmpty()) ?: return null
        val ext = GameFileFormats.extensionOf(constituent.name)
        if (GameFileFormats.isMissionDescriptor(constituent.name)) {
            descriptorTargetForZipConstituent(archivePath, setDir, constituent, game)?.let { return it }
        }
        if (ext == "hog") {
            if (!isBaseHog(constituent.name)) {
                descriptorTargetForZipHog(archivePath, setDir, constituent, game)?.let { return it }
            }
            hogLevelFiles(
                metadata?.contents.orEmpty(),
                baseGame = isBaseHog(constituent.name),
            ).takeIf { it.first.isNotEmpty() || it.second.isNotEmpty() }?.let {
                return LevelMetadataTarget(
                    displayName = constituent.name,
                    game = game,
                    sourceType = "hog",
                    dataDir = setDir.absolutePath,
                    archivePath = archivePath,
                    archiveEntries = listOf(constituent.path),
                    missionName = missionNameFor(constituent.name, game),
                    hogFile = constituent.name,
                    normalLevelFiles = it.first,
                    secretLevelFiles = it.second,
                )
            }
        }
        return LevelMetadataTarget(
            displayName = constituent.name,
            game = game,
            sourceType = if (GameFileFormats.isLevelFile(constituent.name)) "level" else "mission",
            dataDir = setDir.absolutePath,
            archivePath = archivePath,
            archiveEntries = listOf(constituent.path),
            missionName = missionNameFor(constituent.name, game),
            levelFile = if (GameFileFormats.isLevelFile(constituent.name)) constituent.name else null,
            levelNum = if (ext == "sdl" || ext == "sl2") -1 else 1,
        )
    }

    private fun extractedConstituentTarget(
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        metadata: GameFileMetadata.Summary?,
    ): LevelMetadataTarget? {
        val store = missionZipExtractedStoreForArchivePath(archivePath) ?: return null
        val extracted = store.extractedEntryForArchiveEntry(archivePath, constituent.path) ?: return null
        val ext = GameFileFormats.extensionOf(constituent.name)
        val game = gameForFile(constituent.name, metadata?.contents?.map { it.name }.orEmpty()) ?: return null
        if (GameFileFormats.isMissionDescriptor(constituent.name)) {
            extractedDescriptorTarget(store, archivePath, setDir, constituent, extracted, game)?.let { return it }
        }
        if (ext == "hog" && !isBaseHog(constituent.name)) {
            extractedHogTarget(store, archivePath, setDir, constituent, extracted, game)?.let { return it }
            return directFile(extracted.file, setDir, metadata)
        }
        return null
    }

    private fun extractedDescriptorTarget(
        store: MissionZipExtractionStore,
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        extracted: MissionZipExtractedEntry,
        game: String,
    ): LevelMetadataTarget? {
        val mission =
            runCatching {
                MissionZip.parseMissionDescriptor(constituent.path, extracted.file.readBytes())
            }.getOrNull() ?: return null
        val hog = store.findExtractedSameStemEntry(archivePath, constituent.path, "hog") ?: return null
        val sourceLayout =
            extractedMissionFileSourceLayout(extracted.rootDir, listOf(extracted.relativePath, hog.relativePath))
        return LevelMetadataTarget(
            displayName = constituent.name,
            game = game,
            sourceType = "mission_files",
            sourcePath = sourceLayout.root.absolutePath,
            dataDir = setDir.absolutePath,
            missionName = constituent.name.substringBeforeLast('.'),
            missionDisplayName = mission.displayName,
            missionFilename = sourceLayout.relativeToRoot(extracted.relativePath),
            missionType = mission.type,
            hogFiles = listOf(sourceLayout.relativeToRoot(hog.relativePath)),
            normalLevelFiles = mission.levelNames,
            secretLevelFiles = mission.secretLevelNames,
        )
    }

    private fun extractedHogTarget(
        store: MissionZipExtractionStore,
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        extracted: MissionZipExtractedEntry,
        game: String,
    ): LevelMetadataTarget? {
        val descriptorExt = if (game == GameFileFormats.GAME_D1) "msn" else "mn2"
        val descriptor = store.findExtractedSameStemEntry(archivePath, constituent.path, descriptorExt) ?: return null
        val mission =
            runCatching {
                MissionZip.parseMissionDescriptor(
                    descriptor.relativePath,
                    descriptor.file.readBytes(),
                )
            }.getOrNull() ?: return null
        val sourceLayout =
            extractedMissionFileSourceLayout(extracted.rootDir, listOf(extracted.relativePath, descriptor.relativePath))
        return LevelMetadataTarget(
            displayName = constituent.name,
            game = game,
            sourceType = "mission_files",
            sourcePath = sourceLayout.root.absolutePath,
            dataDir = setDir.absolutePath,
            missionName = constituent.name.substringBeforeLast('.'),
            missionDisplayName = mission.displayName,
            missionFilename = sourceLayout.relativeToRoot(descriptor.relativePath),
            missionType = mission.type,
            hogFiles = listOf(sourceLayout.relativeToRoot(extracted.relativePath)),
            normalLevelFiles = mission.levelNames,
            secretLevelFiles = mission.secretLevelNames,
        )
    }

    private data class ExtractedMissionFileSourceLayout(
        val root: File,
        val prefix: String,
    ) {
        fun relativeToRoot(path: String): String = if (prefix.isEmpty()) path else path.removePrefix(prefix)
    }

    private fun extractedMissionFileSourceLayout(
        extractedRoot: File,
        relativePaths: List<String>,
    ): ExtractedMissionFileSourceLayout {
        val dirs =
            relativePaths
                .map { it.replace('\\', '/').trim('/').substringBeforeLast('/', "") }
                .filter { it.isNotBlank() }
                .distinctBy { it.lowercase(Locale.US) }
        if (dirs.size != 1) return ExtractedMissionFileSourceLayout(extractedRoot, "")
        val prefix = "${dirs.single().trim('/')}/"
        val root = File(extractedRoot, prefix.replace('/', File.separatorChar))
        return if (root.isDirectory) {
            ExtractedMissionFileSourceLayout(root, prefix)
        } else {
            ExtractedMissionFileSourceLayout(extractedRoot, "")
        }
    }

    private fun gameForFile(
        name: String,
        children: List<String>,
    ): String? {
        GameFileFormats.gameForDescriptor(name)?.let { return it }
        GameFileFormats.gameForLevel(name)?.let { return it }
        val lower = name.lowercase(Locale.US)
        if (lower == "descent.hog") return GameFileFormats.GAME_D1
        if (lower == "descent2.hog" || lower == "d2demo.hog") return GameFileFormats.GAME_D2
        val hints = children.mapNotNull { GameFileFormats.gameForLevel(it) }.distinct()
        return hints.singleOrNull()
    }

    private fun missionNameFor(
        name: String,
        game: String,
    ): String {
        val lower = name.lowercase(Locale.US)
        return when {
            game == GameFileFormats.GAME_D1 && lower == "descent.hog" -> ""
            game == GameFileFormats.GAME_D2 && lower == "descent2.hog" -> "d2"
            game == GameFileFormats.GAME_D2 && lower == "d2demo.hog" -> "d2demo"
            else -> name.substringAfterLast('/').substringBeforeLast('.')
        }
    }

    internal fun isBaseHog(name: String): Boolean =
        when (name.lowercase(Locale.US)) {
            "descent.hog", "descent2.hog", "d2demo.hog" -> true
            else -> false
        }

    private fun hogLevelFiles(
        entries: List<GameFileMetadata.EntrySummary>,
        baseGame: Boolean = false,
    ): Pair<List<String>, List<String>> {
        val normal = mutableListOf<String>()
        val secret = mutableListOf<String>()
        entries.forEach { entry ->
            when (GameFileFormats.extensionOf(entry.name)) {
                "rdl", "rl2" -> {
                    if (baseGame && isBaseSecretLevel(entry.name)) {
                        secret += entry.name
                    } else {
                        normal += entry.name
                    }
                }

                "sdl", "sl2" -> {
                    secret += entry.name
                }
            }
        }
        return normal to secret
    }

    private fun isBaseSecretLevel(name: String): Boolean {
        val stem = name.substringAfterLast('/').substringBeforeLast('.').lowercase(Locale.US)
        return stem.matches(Regex("levels[0-9]+")) || stem.endsWith("-s")
    }

    private fun descriptorTargetForFile(
        descriptor: File,
        setDir: File,
        game: String,
    ): LevelMetadataTarget? {
        val mission =
            runCatching {
                MissionZip.parseMissionDescriptor(descriptor.name, descriptor.readBytes())
            }.getOrNull() ?: return null
        val hog = File(descriptor.parentFile, "${descriptor.name.substringBeforeLast('.')}.hog")
        if (!hog.isFile) return null
        return LevelMetadataTarget(
            displayName = descriptor.name,
            game = game,
            sourceType = "hog",
            sourcePath = hog.absolutePath,
            dataDir = setDir.absolutePath,
            missionName = descriptor.name.substringBeforeLast('.'),
            missionDisplayName = mission.displayName,
            missionFilename = descriptor.name,
            missionType = mission.type,
            hogFile = hog.name,
            normalLevelFiles = mission.levelNames,
            secretLevelFiles = mission.secretLevelNames,
        )
    }

    private fun descriptorTargetForZipConstituent(
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        game: String,
    ): LevelMetadataTarget? {
        val archive = File(archivePath)
        if (!archive.isFile) return null
        ArchiveFiles.open(archive).use { source ->
            val descriptorEntry = source.findEntry(constituent.path) ?: return null
            val mission =
                runCatching {
                    source.openInputStream(descriptorEntry).use {
                        MissionZip.parseMissionDescriptor(constituent.path, it.readBytes())
                    }
                }.getOrNull() ?: return null
            val hogEntry = findSameStemArchiveEntry(source, constituent.path, "hog") ?: return null
            val hogName = hogEntry.name
            return LevelMetadataTarget(
                displayName = constituent.name,
                game = game,
                sourceType = "mission_files",
                dataDir = setDir.absolutePath,
                archivePath = archivePath,
                archiveEntries = listOf(constituent.path, hogEntry.path),
                missionName = constituent.name.substringBeforeLast('.'),
                missionDisplayName = mission.displayName,
                missionFilename = constituent.name,
                missionType = mission.type,
                hogFiles = listOf(hogName),
                normalLevelFiles = mission.levelNames,
                secretLevelFiles = mission.secretLevelNames,
            )
        }
    }

    private fun descriptorTargetForZipHog(
        archivePath: String,
        setDir: File,
        constituent: MissionZip.Constituent,
        game: String,
    ): LevelMetadataTarget? {
        val archive = File(archivePath)
        if (!archive.isFile) return null
        val descriptorExt = if (game == GameFileFormats.GAME_D1) "msn" else "mn2"
        ArchiveFiles.open(archive).use { source ->
            val descriptorEntry = findSameStemArchiveEntry(source, constituent.path, descriptorExt) ?: return null
            val mission =
                runCatching {
                    source.openInputStream(descriptorEntry).use {
                        MissionZip.parseMissionDescriptor(descriptorEntry.path, it.readBytes())
                    }
                }.getOrNull() ?: return null
            return LevelMetadataTarget(
                displayName = constituent.name,
                game = game,
                sourceType = "mission_files",
                dataDir = setDir.absolutePath,
                archivePath = archivePath,
                archiveEntries = listOf(constituent.path, descriptorEntry.path),
                missionName = constituent.name.substringBeforeLast('.'),
                missionDisplayName = mission.displayName,
                missionFilename = descriptorEntry.name.substringAfterLast('/').substringAfterLast('\\'),
                missionType = mission.type,
                hogFiles = listOf(constituent.name),
                normalLevelFiles = mission.levelNames,
                secretLevelFiles = mission.secretLevelNames,
            )
        }
    }

    private fun findSameStemArchiveEntry(
        archive: ReadableArchiveFile,
        path: String,
        extension: String,
    ): ArchiveFileEntry? {
        val normalized = path.replace('\\', '/').trim('/')
        val dir = normalized.substringBeforeLast('/', "")
        val stem = normalized.substringAfterLast('/').substringBeforeLast('.')
        val sibling = if (dir.isBlank()) "$stem.$extension" else "$dir/$stem.$extension"
        archive.findEntry(sibling)?.let { return it }
        val wanted = "$stem.$extension".lowercase(Locale.US)
        return archive.entries.firstOrNull {
            !it.isDirectory &&
                it.name.lowercase(Locale.US) == wanted
        }
    }

    private fun descriptorForHog(
        hogFile: File,
        game: String,
    ): GameFileFormats.MissionDescriptor? {
        val descriptorExt = if (game == GameFileFormats.GAME_D1) "msn" else "mn2"
        val descriptor = File(hogFile.parentFile, "${hogFile.name.substringBeforeLast('.')}.$descriptorExt")
        if (!descriptor.isFile) return null
        return runCatching {
            MissionZip.parseMissionDescriptor(descriptor.name, descriptor.readBytes())
        }.getOrNull()
    }

    private fun descriptorFileNameForHog(
        hogFile: File,
        game: String,
    ): String {
        val descriptorExt = if (game == GameFileFormats.GAME_D1) "msn" else "mn2"
        return "${hogFile.name.substringBeforeLast('.')}.$descriptorExt"
    }
}

internal object LevelMetadataAnalyzer {
    suspend fun analyze(
        context: Context,
        target: LevelMetadataTarget,
        background: Boolean = false,
        priority: RouteMetadataPriority = if (background) RouteMetadataPriority.FILL else RouteMetadataPriority.ACTIVE,
        totalTimeoutMs: Long = Long.MAX_VALUE,
        onProgress: suspend (LevelMetadataAnalysisProgress) -> Unit = {},
    ): LevelMetadataResult =
        withContext(Dispatchers.IO) {
            val stepCount = 5
            val expectedLevelCount =
                (target.normalLevelFiles.size + target.secretLevelFiles.size).coerceAtLeast(0)
            var completedLevelCount = 0
            val levelProgressEstimator = LevelMetadataLevelProgressEstimator()

            suspend fun progress(
                label: String,
                completed: Int,
            ) {
                onProgress(
                    LevelMetadataAnalysisProgress(
                        overall =
                            MetadataLoadProgress(
                                "Overall analysis",
                                completedLevelCount,
                                expectedLevelCount,
                            ),
                        currentLevel = MetadataLoadProgress(label, completed, stepCount),
                    ),
                )
            }

            val appContext = context.applicationContext
            progress("Checking metadata cache", 0)
            val resultCacheRoot = File(appContext.filesDir, "level_metadata_results")
            val identityStartedAt = SystemClock.elapsedRealtime()
            val resultCacheIdentity = LevelMetadataResultCache.identify(target)
            val cachedResult =
                resultCacheIdentity?.let {
                    LevelMetadataResultCache.read(resultCacheRoot, it, target, expectedLevelCount)
                }
            RouteMetadataDiagnostics.log(
                "Level metadata result cache source=${target.displayName} " +
                    "hit=${cachedResult != null} identifiable=${resultCacheIdentity != null} " +
                    "files=${resultCacheIdentity?.sourceFiles ?: 0} " +
                    "bytes=${resultCacheIdentity?.sourceBytes ?: 0} " +
                    "elapsed_ms=${SystemClock.elapsedRealtime() - identityStartedAt}",
            )
            if (cachedResult != null) {
                completedLevelCount = expectedLevelCount
                progress("Analysis complete", stepCount)
                return@withContext cachedResult
            }
            val cacheRoot = File(appContext.cacheDir, "level_metadata")
            OwnedCacheDirectories.prune(cacheRoot, LEVEL_METADATA_CACHE_MAX_AGE_MS)
            val workDir = OwnedCacheDirectories.create(cacheRoot)
            val requestId = workDir.name
            val resultFile = File(workDir, "result.json")
            val checkpointFile = File(workDir, "checkpoint.json")
            val requestFile = File(workDir, "request.json")
            val workerFile = File(workDir, LEVEL_METADATA_WORKER_FILE)
            val queuedFile = File(workDir, LEVEL_METADATA_QUEUED_FILE)
            val cancellationFile = File(workDir, LEVEL_METADATA_CANCELLATION_FILE)
            val ownerFile = workerOwnerFile(cacheRoot, target.game)
            val startedAt = System.currentTimeMillis()
            val profilingStartedAt = SystemClock.elapsedRealtime()
            var workerStarted = false
            try {
                progress("Preparing analysis files", 0)
                val prepareStartedAt = SystemClock.elapsedRealtime()
                val request =
                    try {
                        buildRequestJson(
                            target,
                            requestId,
                            workDir,
                            resultFile,
                            checkpointFile,
                            background,
                            priority,
                            totalTimeoutMs,
                        )
                    } catch (e: CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        return@withContext LevelMetadataResult.failed(
                            target.displayName,
                            target.game,
                            e.message ?: e.javaClass.simpleName,
                        )
                    }
                RouteMetadataDiagnostics.log(
                    "Level metadata prepared request=$requestId source=${target.displayName} " +
                        "source_type=${target.sourceType} archive=${target.archivePath != null} " +
                        "entries=${target.archiveEntries.size} levels=$expectedLevelCount " +
                        "elapsed_ms=${SystemClock.elapsedRealtime() - prepareStartedAt}",
                )

                progress("Writing analysis request", 1)
                try {
                    OwnedCacheDirectories.writeUtf8Atomically(workDir, requestFile.name, request.toString(2) + "\n")
                    val intent =
                        Intent(appContext, serviceClassForGame(target.game))
                            .putExtra(LevelMetadataAnalysisService.EXTRA_REQUEST_PATH, requestFile.absolutePath)
                    if (preemptLowerPriorityWorker(appContext, cacheRoot, target.game, priority)) {
                        progress("Switching from background analysis", 2)
                    }
                    progress("Starting analysis worker", 2)
                    RouteMetadataDiagnostics.log(
                        "Level metadata submit request=$requestId source=${target.displayName} " +
                            "priority=${priority.wireName} background=$background",
                    )
                    appContext.startService(intent)
                    workerStarted = true
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    return@withContext LevelMetadataResult.failed(
                        target.displayName,
                        target.game,
                        e.message ?: e.javaClass.simpleName,
                    )
                }
                val workerStartedAt = SystemClock.elapsedRealtime()
                val progressDeadline = LevelMetadataProgressDeadline(workerStartedAt)
                if (expectedLevelCount > 0) {
                    onProgress(
                        LevelMetadataAnalysisProgress(
                            overall = MetadataLoadProgress("Overall analysis", 0, expectedLevelCount),
                            currentLevel = MetadataLoadProgress("Starting first level", 0, 0),
                        ),
                    )
                } else {
                    progress("Scanning levels", 3)
                }
                var lastCheckpoint = ""

                while (true) {
                    if (resultFile.isFile) {
                        completedLevelCount = expectedLevelCount
                        progress("Reading analysis result", 4)
                        val parseStartedAt = SystemClock.elapsedRealtime()
                        val resultText = runCatching { resultFile.readText(Charsets.UTF_8) }.getOrDefault("")
                        val parsed = parseResultText(target, resultText)
                        val cachePublished =
                            resultCacheIdentity?.let { identity ->
                                runCatching {
                                    LevelMetadataResultCache.publish(
                                        resultCacheRoot,
                                        identity,
                                        target,
                                        expectedLevelCount,
                                        resultText,
                                        parsed,
                                    )
                                }.getOrDefault(false)
                            } ?: false
                        RouteMetadataDiagnostics.log(
                            "Level metadata complete request=$requestId source=${target.displayName} " +
                                "worker_ms=${SystemClock.elapsedRealtime() - workerStartedAt} " +
                                "parse_ms=${SystemClock.elapsedRealtime() - parseStartedAt} " +
                                "total_ms=${SystemClock.elapsedRealtime() - profilingStartedAt} " +
                                "status=${parsed.status} levels=${parsed.levels.size} " +
                                "result_cache_published=$cachePublished",
                        )
                        progress("Analysis complete", 5)
                        return@withContext parsed
                    }
                    if (checkpointFile.isFile) {
                        runCatching { checkpointFile.readText(Charsets.UTF_8) }
                            .getOrNull()
                            ?.takeIf { it != lastCheckpoint }
                            ?.let { checkpoint ->
                                lastCheckpoint = checkpoint
                                parseLevelMetadataCheckpointUpdate(checkpoint)?.let { update ->
                                    progressDeadline.observe(update, SystemClock.elapsedRealtime())
                                    completedLevelCount =
                                        maxOf(completedLevelCount, update.analysisProgress.overall.completed)
                                    onProgress(
                                        update.analysisProgress.copy(
                                            estimatedLevel = levelProgressEstimator.observe(update),
                                        ),
                                    )
                                }
                            }
                    }
                    if (!isOwnedWorkerProcessRunning(
                            appContext,
                            target.game,
                            requestId,
                            workerFile,
                            queuedFile,
                            ownerFile,
                        ) &&
                        SystemClock.elapsedRealtime() - workerStartedAt > LEVEL_METADATA_WORKER_START_TIMEOUT_MS
                    ) {
                        break
                    }
                    if (progressDeadline.isExpired(SystemClock.elapsedRealtime())) break
                    delay(LEVEL_METADATA_POLL_MS)
                }

                cancelOwnedWorker(
                    appContext,
                    target.game,
                    requestId,
                    workerFile,
                    queuedFile,
                    ownerFile,
                    cancellationFile,
                )
                progress("Collecting diagnostics", 4)
                val diagnostics = collectDiagnostics(appContext, startedAt, checkpointFile)
                val status = if (diagnostics.any { it.contains("crash", ignoreCase = true) }) "crashed" else "timeout"
                RouteMetadataDiagnostics.log(
                    "Level metadata failed request=$requestId source=${target.displayName} " +
                        "total_ms=${SystemClock.elapsedRealtime() - profilingStartedAt} status=$status",
                )
                LevelMetadataResult.failed(
                    target.displayName,
                    target.game,
                    if (status == "crashed") "Analysis worker crashed" else "Analysis timed out",
                    diagnostics,
                    status,
                    status,
                )
            } finally {
                withContext(NonCancellable) {
                    if (workerStarted) {
                        cancelOwnedWorker(
                            appContext,
                            target.game,
                            requestId,
                            workerFile,
                            queuedFile,
                            ownerFile,
                            cancellationFile,
                        )
                    }
                    RouteMetadataDiagnostics.log(
                        "Level metadata dispose request=$requestId source=${target.displayName} " +
                            "result_deleted=${resultFile.isFile} " +
                            "elapsed_ms=${SystemClock.elapsedRealtime() - profilingStartedAt}",
                    )
                    OwnedCacheDirectories.delete(cacheRoot, workDir)
                }
            }
        }

    internal fun parseLevelMetadataCheckpointProgress(text: String): MetadataLoadProgress? =
        parseLevelMetadataCheckpointUpdate(text)?.progress

    internal fun parseLevelMetadataCheckpointUpdate(text: String): LevelMetadataCheckpointUpdate? =
        runCatching {
            val checkpoint = JSONObject(text)
            val stage = checkpoint.optString("stage")
            if (stage != "level" && stage != "level_done" && stage != "level_progress") {
                return@runCatching null
            }
            val total = checkpoint.optInt("total", 0)
            if (total <= 0) return@runCatching null
            val completed = checkpoint.optInt("completed", 0).coerceIn(0, total)
            val detail = checkpoint.optString("detail").substringAfterLast('/').substringAfterLast('\\')
            val phase = checkpoint.optString("phase")
            val taskId = checkpoint.optInt("task_id", 0)
            val label =
                if (stage == "level_progress") {
                    levelMetadataTaskLabel(phase, detail)
                } else if (stage == "level" && detail.isNotBlank()) {
                    "Scanning $detail"
                } else {
                    "Scanning levels"
                }
            val activityId =
                if (stage == "level_progress") {
                    "$detail:$taskId"
                } else {
                    "levels:$detail"
                }
            val taskProgress = MetadataLoadProgress(label, completed, total)
            val overallProgress =
                if (stage == "level_progress") {
                    val levelTotal = checkpoint.optInt("level_total", 0)
                    MetadataLoadProgress(
                        label = "Overall analysis",
                        completed = checkpoint.optInt("level_completed", 0).coerceIn(0, levelTotal.coerceAtLeast(0)),
                        total = levelTotal,
                    )
                } else {
                    MetadataLoadProgress("Overall analysis", completed, total)
                }
            val currentLevelProgress =
                when (stage) {
                    "level_progress" -> {
                        taskProgress
                    }

                    "level" -> {
                        MetadataLoadProgress(
                            if (detail.isBlank()) "Starting level" else "Starting $detail",
                            0,
                            0,
                        )
                    }

                    else -> {
                        MetadataLoadProgress(
                            if (detail.isBlank()) "Level complete" else "Completed $detail",
                            1,
                            1,
                        )
                    }
                }
            LevelMetadataCheckpointUpdate(
                progress = taskProgress,
                activityId = activityId,
                stage = stage,
                phase = phase,
                taskId = taskId,
                levelIdentity = detail,
                analysisProgress =
                    LevelMetadataAnalysisProgress(
                        overall = overallProgress,
                        currentLevel = currentLevelProgress,
                    ),
            )
        }.getOrNull()

    private fun levelMetadataTaskLabel(
        phase: String,
        detail: String,
    ): String {
        val action =
            when (phase) {
                "secret_areas" -> "Scanning secret areas"
                "level_topology" -> "Indexing level geometry"
                "level_summary" -> "Summarizing level"
                "route_planning" -> "Planning completion route"
                "route_visibility" -> "Checking switch firing paths"
                "route_target_visibility" -> "Checking objective visibility"
                else -> "Analyzing level"
            }
        return if (detail.isBlank()) action else "$action in $detail"
    }

    private fun serviceClassForGame(game: String): Class<out LevelMetadataAnalysisService> =
        if (game == GameFileFormats.GAME_D1) {
            LevelMetadataD1AnalysisService::class.java
        } else {
            LevelMetadataD2AnalysisService::class.java
        }

    private fun parseResultText(
        target: LevelMetadataTarget,
        text: String,
    ): LevelMetadataResult =
        try {
            LevelMetadataResult.fromJson(text)
        } catch (e: Exception) {
            LevelMetadataResult.failed(target.displayName, target.game, "Bad analysis result: ${e.message}")
        }

    private fun buildRequestJson(
        target: LevelMetadataTarget,
        requestId: String,
        workDir: File,
        resultFile: File,
        checkpointFile: File,
        background: Boolean,
        priority: RouteMetadataPriority,
        totalTimeoutMs: Long,
    ): JSONObject =
        buildPreparedRequestJson(target, requestId, workDir, "dxx-level-metadata-request-v1")
            .put("result_path", resultFile.absolutePath)
            .put("checkpoint_path", checkpointFile.absolutePath)
            .put("cancellation_path", File(workDir, LEVEL_METADATA_CANCELLATION_FILE).absolutePath)
            .put("background", background)
            .put("priority", priority.wireName)
            .put("cpu_duty_percent", priority.cpuDutyPercent)
            .put("fvi_limit", priority.fviLimit)
            .put("defer_guidebot_accessibility", target.sourceType == "active_level")
            .put("total_timeout_ms", totalTimeoutMs)

    internal fun buildPreviewRequestJson(
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
        requestId: String,
        workDir: File,
        previewWriteDir: File,
    ): JSONObject =
        buildPreparedRequestJson(target, requestId, workDir, "dxx-level-preview-request-v1")
            .put("level_file", row.levelFile)
            .put("level_num", row.levelNum)
            .put("secret_level", row.secret)
            .put("preview_write_dir", previewWriteDir.absolutePath)

    private fun buildPreparedRequestJson(
        target: LevelMetadataTarget,
        requestId: String,
        workDir: File,
        schema: String,
    ): JSONObject {
        val stageDir = File(workDir, "staged")
        val prepared = prepareTarget(target, stageDir)
        return JSONObject()
            .put("schema", schema)
            .put("request_id", requestId)
            .put("game", target.game)
            .put("source_name", target.displayName)
            .put("source_path", target.sourcePath.orEmpty())
            .put("archive_path", target.archivePath.orEmpty())
            .put("source_type", prepared.sourceType)
            .put("data_dir", prepared.dataDir)
            .put("extra_data_dir", prepared.extraDataDir)
            .put("mission_name", prepared.missionName)
            .put("mission_display_name", prepared.missionDisplayName)
            .put("mission_filename", prepared.missionFilename)
            .put("mission_type", target.missionType.orEmpty())
            .put("level_file", prepared.levelFile)
            .put("level_num", prepared.levelNum)
            .put("hog_path", prepared.hogPath)
            .put("hog_paths", JSONArray(prepared.hogPaths))
            .put("normal_level_files", JSONArray(prepared.normalLevelFiles))
            .put("secret_level_files", JSONArray(prepared.secretLevelFiles))
    }

    private data class PreparedTarget(
        val sourceType: String,
        val dataDir: String,
        val extraDataDir: String,
        val missionName: String,
        val missionDisplayName: String,
        val missionFilename: String,
        val levelFile: String,
        val levelNum: Int,
        val hogPath: String,
        val hogPaths: List<String>,
        val normalLevelFiles: List<String>,
        val secretLevelFiles: List<String>,
    )

    private fun prepareTarget(
        target: LevelMetadataTarget,
        stageDir: File,
    ): PreparedTarget {
        val archivePath = target.archivePath
        if (archivePath != null) {
            stageArchiveEntries(File(archivePath), target.archiveEntries, stageDir)
            val stagedHog =
                target.hogFile
                    ?.takeIf { target.sourceType == "hog" }
                    ?.let { File(stageDir, it).absolutePath }
                    .orEmpty()
            val stagedHogs =
                (target.hogFiles + listOfNotNull(target.hogFile).filter { target.sourceType == "hog" })
                    .distinctBy { it.lowercase(Locale.US) }
                    .map { File(stageDir, it).absolutePath }
            return PreparedTarget(
                sourceType = target.sourceType,
                dataDir = target.dataDir.orEmpty(),
                extraDataDir = stageDir.absolutePath,
                missionName = target.missionName.orEmpty(),
                missionDisplayName = target.missionDisplayName.orEmpty(),
                missionFilename = target.missionFilename.orEmpty(),
                levelFile = target.levelFile.orEmpty(),
                levelNum = target.levelNum,
                hogPath = stagedHog,
                hogPaths = stagedHogs,
                normalLevelFiles = target.normalLevelFiles,
                secretLevelFiles = target.secretLevelFiles,
            )
        }
        val source = target.sourcePath?.let(::File)
        if (source?.isDirectory == true && target.sourceType == "mission_files") {
            val hogPaths =
                target.hogFiles
                    .map { File(source, it.replace('/', File.separatorChar)).absolutePath }
                    .filter { File(it).isFile }
            return PreparedTarget(
                sourceType = target.sourceType,
                dataDir = target.dataDir.orEmpty(),
                extraDataDir = source.absolutePath,
                missionName = target.missionName.orEmpty(),
                missionDisplayName = target.missionDisplayName.orEmpty(),
                missionFilename = target.missionFilename.orEmpty(),
                levelFile = target.levelFile.orEmpty(),
                levelNum = target.levelNum,
                hogPath = "",
                hogPaths = hogPaths,
                normalLevelFiles = target.normalLevelFiles,
                secretLevelFiles = target.secretLevelFiles,
            )
        }
        return PreparedTarget(
            sourceType = target.sourceType,
            dataDir = target.dataDir ?: source?.parentFile?.absolutePath.orEmpty(),
            extraDataDir = "",
            missionName = target.missionName.orEmpty(),
            missionDisplayName = target.missionDisplayName.orEmpty(),
            missionFilename = target.missionFilename.orEmpty(),
            levelFile = target.levelFile ?: source?.name.orEmpty(),
            levelNum = target.levelNum,
            hogPath = if (target.sourceType == "hog") source?.absolutePath.orEmpty() else "",
            hogPaths = if (target.sourceType == "hog") listOfNotNull(source?.absolutePath) else emptyList(),
            normalLevelFiles = target.normalLevelFiles,
            secretLevelFiles = target.secretLevelFiles,
        )
    }

    internal fun stageArchiveEntries(
        archive: File,
        entryPaths: List<String>,
        stageDir: File,
    ) {
        if (!archive.isFile) throw IllegalArgumentException("Mission archive is missing")
        if (entryPaths.size > LEVEL_METADATA_MAX_ZIP_FILES) throw IllegalArgumentException("Too many archive entries")
        stageDir.mkdirs()
        var total = 0L
        val buffer = ByteArray(8192)
        val usedNames = mutableSetOf<String>()
        ArchiveFiles.open(archive).use { source ->
            ImportStorageGuard.requireFreeSpace(
                stageDir,
                levelMetadataArchiveStageBytes(source, entryPaths),
                "stage level metadata files",
            )
            entryPaths.forEach { path ->
                val entry = source.findEntry(path) ?: throw IllegalArgumentException("Archive entry is missing: $path")
                if (entry.isDirectory) return@forEach
                val leaf = path.substringAfterLast('/').substringAfterLast('\\')
                if (leaf.isBlank() || leaf == "." || leaf == ".." || !usedNames.add(leaf.lowercase(Locale.US))) {
                    throw IllegalArgumentException("Unsafe or duplicate archive entry: $path")
                }
                val declaredSize = entry.sizeBytes.coerceAtLeast(0)
                if (declaredSize >
                    LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES
                ) {
                    throw IllegalArgumentException("Archive entry is too large: $leaf")
                }
                if (declaredSize > 0 && total + declaredSize >
                    LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES
                ) {
                    throw IllegalArgumentException("Mission archive is too large")
                }
                val out = File(stageDir, leaf)
                var copied = 0L
                source.openInputStream(entry).use { input ->
                    out.outputStream().use { output ->
                        while (true) {
                            val read = input.read(buffer)
                            if (read <= 0) break
                            copied += read.toLong()
                            if (copied > LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES) {
                                throw IllegalArgumentException("Archive entry is too large: $leaf")
                            }
                            if (total + copied > LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES) {
                                throw IllegalArgumentException("Mission archive is too large")
                            }
                            output.write(buffer, 0, read)
                        }
                    }
                }
                if (GameFileFormats.isMissionDescriptor(leaf)) {
                    copyStageAlias(out, File(File(stageDir, "missions"), leaf))
                }
                if (GameFileFormats.isMissionDescriptor(leaf) || GameFileFormats.isLevelFile(leaf)) {
                    val lowerLeaf = leaf.lowercase(Locale.US)
                    copyStageAlias(out, File(stageDir, lowerLeaf))
                    copyStageAlias(out, File(File(stageDir, "missions"), lowerLeaf))
                }
                total += copied
            }
        }
    }

    private fun copyStageAlias(
        source: File,
        target: File,
    ) {
        if (source.absolutePath == target.absolutePath ||
            runCatching { source.canonicalFile == target.canonicalFile }.getOrDefault(false)
        ) {
            return
        }
        target.parentFile?.mkdirs()
        ImportStorageGuard.requireFreeSpace(target.parentFile ?: target, source.length(), "stage ${target.name}")
        source.copyTo(target, overwrite = true)
    }

    private fun levelMetadataArchiveStageBytes(
        archive: ReadableArchiveFile,
        entryPaths: List<String>,
    ): Long =
        ImportStorageGuard.archiveEntryBytes(
            entryPaths.mapNotNull { path ->
                archive.findEntry(path)?.takeUnless { it.isDirectory }?.sizeBytes
            },
        )

    private fun collectDiagnostics(
        context: Context,
        startedAt: Long,
        checkpointFile: File,
    ): List<String> =
        buildList {
            if (checkpointFile.isFile) {
                runCatching {
                    val checkpoint = JSONObject(checkpointFile.readText(Charsets.UTF_8))
                    add("Last stage: ${checkpoint.optString("stage")} ${checkpoint.optString("detail")}".trim())
                }
            }
            CrashLog
                .listCrashFiles(context)
                .filter { it.lastModified() >= startedAt - 1_000L }
                .take(3)
                .forEach { add("Crash report saved: ${it.name}") }
        }

    private fun requestWorkerCancellation(cancellationFile: File) {
        runCatching {
            if (cancellationFile.parentFile?.isDirectory == true) cancellationFile.createNewFile()
        }
    }

    private suspend fun preemptLowerPriorityWorker(
        context: Context,
        cacheRoot: File,
        game: String,
        incomingPriority: RouteMetadataPriority,
    ): Boolean {
        val ownerFile = workerOwnerFile(cacheRoot, game)
        val identity = LevelMetadataWorkerOwnerStore.read(ownerFile) ?: return false
        val runningPriority = levelMetadataBackgroundRequestPriority(cacheRoot, identity) ?: return false
        if (!RouteMetadataPreemption.shouldPreempt(runningPriority, incomingPriority)) return false
        val workDir = File(cacheRoot, identity.requestId)
        RouteMetadataDiagnostics.log(
            "Level metadata preempt request=${identity.requestId} priority=${runningPriority.wireName} " +
                "incoming=${incomingPriority.wireName}",
        )
        cancelOwnedWorker(
            context,
            game,
            identity.requestId,
            File(workDir, LEVEL_METADATA_WORKER_FILE),
            File(workDir, LEVEL_METADATA_QUEUED_FILE),
            ownerFile,
            File(workDir, LEVEL_METADATA_CANCELLATION_FILE),
        )
        return true
    }

    private suspend fun cancelOwnedWorker(
        context: Context,
        game: String,
        requestId: String,
        workerFile: File,
        queuedFile: File,
        ownerFile: File,
        cancellationFile: File,
    ) {
        requestWorkerCancellation(cancellationFile)
        val identityWasPublished = workerFile.isFile
        if (!identityWasPublished) return
        val graceDeadline = SystemClock.elapsedRealtime() + LEVEL_METADATA_CANCELLATION_GRACE_MS
        while (SystemClock.elapsedRealtime() < graceDeadline) {
            if (!isOwnedWorkerProcessRunning(context, game, requestId, workerFile, queuedFile, ownerFile)) return
            delay(LEVEL_METADATA_POLL_MS)
        }
        repeat(10) {
            killOwnedWorkerProcess(context, game, requestId, workerFile, ownerFile)
            delay(LEVEL_METADATA_POLL_MS)
            if (!isOwnedWorkerProcessRunning(context, game, requestId, workerFile, queuedFile, ownerFile)) return
        }
    }

    private fun isOwnedWorkerProcessRunning(
        context: Context,
        game: String,
        requestId: String,
        workerFile: File,
        queuedFile: File,
        ownerFile: File,
    ): Boolean {
        readWorkerIdentity(workerFile)?.let { identity ->
            if (LevelMetadataWorkerOwnerStore.read(ownerFile) != identity) return false
            return identityMatchesProcess(context, game, requestId, identity)
        }
        val queuedIdentity = readWorkerIdentity(queuedFile) ?: return false
        return identityMatchesProcess(context, game, requestId, queuedIdentity)
    }

    private fun identityMatchesProcess(
        context: Context,
        game: String,
        requestId: String,
        identity: LevelMetadataWorkerIdentity,
    ): Boolean {
        val process = levelMetadataWorkerProcess(context, game, identity.pid) ?: return false
        val runningStartTicks = processStartTicks(process.pid)
        return identity.requestId == requestId &&
            identity.pid == process.pid &&
            (runningStartTicks == null || runningStartTicks == identity.processStartTicks)
    }

    private fun killOwnedWorkerProcess(
        context: Context,
        game: String,
        requestId: String,
        workerFile: File,
        ownerFile: File,
    ) {
        val identity = readWorkerIdentity(workerFile) ?: return
        if (LevelMetadataWorkerOwnerStore.read(ownerFile) != identity) return
        val process = levelMetadataWorkerProcess(context, game, identity.pid) ?: return
        val runningStartTicks = processStartTicks(process.pid)
        if (identity.requestId == requestId &&
            identity.pid == process.pid &&
            (runningStartTicks == null || runningStartTicks == identity.processStartTicks)
        ) {
            Process.killProcess(process.pid)
        }
    }

    private fun readWorkerIdentity(workerFile: File): LevelMetadataWorkerIdentity? =
        runCatching { LevelMetadataWorkerIdentity.fromJson(workerFile.readText(Charsets.UTF_8)) }.getOrNull()

    private fun levelMetadataWorkerProcess(
        context: Context,
        game: String,
        pid: Int,
    ): ActivityManager.RunningAppProcessInfo? {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager ?: return null
        val processName =
            context.packageName +
                if (game == GameFileFormats.GAME_D1) {
                    LEVEL_METADATA_D1_PROCESS_SUFFIX
                } else {
                    LEVEL_METADATA_D2_PROCESS_SUFFIX
                }
        return activityManager.runningAppProcesses?.firstOrNull { it.pid == pid && it.processName == processName }
    }
}

internal object LevelMetadataNativeBridge {
    private var loadedLibrary: String? = null

    fun analyze(
        context: Context,
        requestJson: String,
        game: String,
    ): String? {
        val library =
            when (game) {
                GameFileFormats.GAME_D1 -> "dxx-redux-d1"
                else -> "dxx-redux-d2"
            }
        if (loadedLibrary != library) {
            System.loadLibrary(library)
            loadedLibrary = library
            CrashLog.installNativeHandler(context)
        }
        return nativeAnalyzeLevelMetadata(context, requestJson)
    }

    private external fun nativeAnalyzeLevelMetadata(
        context: Context,
        requestJson: String,
    ): String?
}

internal object LevelMetadataAnalysisSingleFlight {
    private val running = AtomicBoolean(false)

    fun tryEnter(): Boolean = running.compareAndSet(false, true)

    fun exit() {
        running.set(false)
    }

    fun resetForTest() {
        running.set(false)
    }
}

internal class LevelMetadataServiceLifetime {
    private var latestStartId = 0
    private var pendingCommands = 0

    @Synchronized
    fun started(startId: Int) {
        require(startId > 0)
        latestStartId = maxOf(latestStartId, startId)
        pendingCommands++
    }

    @Synchronized
    fun completed(): Int? {
        check(pendingCommands > 0)
        pendingCommands--
        return if (pendingCommands == 0) latestStartId else null
    }
}

internal class LevelMetadataServiceCommandQueue(
    private val onDrained: (Int) -> Unit,
    private val executor: ExecutorService =
        Executors.newSingleThreadExecutor { command ->
            Thread(command, "level-metadata-command")
        },
) {
    private val lifetime = LevelMetadataServiceLifetime()

    fun submit(
        startId: Int,
        command: () -> Unit,
    ) {
        lifetime.started(startId)
        try {
            executor.execute {
                try {
                    runCatching { Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND) }
                    command()
                } finally {
                    completeCommand()
                }
            }
        } catch (e: RuntimeException) {
            completeCommand()
            throw e
        }
    }

    fun shutdown() {
        executor.shutdown()
    }

    private fun completeCommand() {
        lifetime.completed()?.let(onDrained)
    }
}

open class LevelMetadataAnalysisService : Service() {
    private lateinit var commandQueue: LevelMetadataServiceCommandQueue

    override fun onCreate() {
        super.onCreate()
        commandQueue =
            LevelMetadataServiceCommandQueue(
                onDrained = { startId -> stopSelfResult(startId) },
            )
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        val requestPath = intent?.getStringExtra(EXTRA_REQUEST_PATH)
        if (requestPath == null) {
            commandQueue.submit(startId) {}
            return START_NOT_STICKY
        }
        val requestFile = File(requestPath)
        val queuedFile = File(requestFile.parentFile, LEVEL_METADATA_QUEUED_FILE)
        runCatching { publishQueuedIdentity(requestFile, queuedFile) }
            .onFailure { Log.e(TAG, "Level metadata queue publication failed", it) }
        RouteMetadataDiagnostics.log("Level metadata service queued request=${requestFile.parentFile?.name.orEmpty()}")
        commandQueue.submit(startId) {
            try {
                if (!LevelMetadataAnalysisSingleFlight.tryEnter()) {
                    runCatching { writeBusyResult(requestFile) }
                        .onFailure { Log.e(TAG, "Level metadata busy result failed", it) }
                    return@submit
                }
                try {
                    runCatching { runOwnedAnalysis(requestFile, queuedFile) }
                        .onFailure { Log.e(TAG, "Level metadata analysis failed", it) }
                } finally {
                    LevelMetadataAnalysisSingleFlight.exit()
                }
            } finally {
                queuedFile.delete()
            }
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        commandQueue.shutdown()
        super.onDestroy()
    }

    private fun publishQueuedIdentity(
        requestFile: File,
        queuedFile: File,
    ) {
        val workDir = requestFile.parentFile ?: error("Level metadata request directory is missing")
        val requestId = workDir.name
        val pid = Process.myPid()
        val startTicks = processStartTicks(pid) ?: error("Could not identify level metadata service process")
        OwnedCacheDirectories.writeUtf8Atomically(
            workDir,
            queuedFile.name,
            LevelMetadataWorkerIdentity(requestId, pid, startTicks).toJson(),
        )
    }

    private fun runOwnedAnalysis(
        requestFile: File,
        queuedFile: File,
    ) {
        val requestJson = requestFile.readText(Charsets.UTF_8)
        val request = JSONObject(requestJson)
        val priority =
            RouteMetadataPriority.entries.firstOrNull {
                it.wireName == request.optString("priority")
            } ?: RouteMetadataPriority.FILL
        runCatching { Process.setThreadPriority(priority.threadPriority) }
        val requestId = request.optString("request_id")
        RouteMetadataDiagnostics.log(
            "Level metadata worker starting request=$requestId source=${request.optString("source_name")} " +
                "priority=${priority.wireName}",
        )
        val workDir = requestFile.parentFile ?: error("Level metadata request directory is missing")
        require(requestId.isNotBlank() && requestId == workDir.name) { "Invalid level metadata request identity" }
        val cancellationFile = File(workDir, LEVEL_METADATA_CANCELLATION_FILE)
        if (cancellationFile.exists()) return
        val pid = Process.myPid()
        val startTicks = processStartTicks(pid) ?: error("Could not identify level metadata worker process")
        val identity = LevelMetadataWorkerIdentity(requestId, pid, startTicks)
        val ownerFile =
            workerOwnerFile(
                workDir.parentFile ?: error("Level metadata cache root is missing"),
                request.optString("game"),
            )
        val lockFile = File(workDir.parentFile, LEVEL_METADATA_GLOBAL_LOCK_FILE)
        RandomAccessFile(lockFile, "rw").use { lockAccess ->
            val lock = lockAccess.channel.tryLock()
            if (lock == null) {
                writeBusyResult(requestFile)
                return
            }
            lock.use {
                LevelMetadataWorkerOwnerStore.publish(ownerFile, identity)
                try {
                    val workerFile =
                        OwnedCacheDirectories.writeUtf8Atomically(
                            workDir,
                            LEVEL_METADATA_WORKER_FILE,
                            identity.toJson(),
                        )
                    queuedFile.delete()
                    if (cancellationFile.exists()) {
                        workerFile.delete()
                        return
                    }
                    val watchdog = startWatchdog(request, cancellationFile)
                    try {
                        runAnalysis(requestJson, request)
                    } finally {
                        watchdog.interrupt()
                        workerFile.delete()
                    }
                } finally {
                    LevelMetadataWorkerOwnerStore.clearIfOwned(ownerFile, identity)
                }
            }
        }
    }

    private fun runAnalysis(
        requestJson: String,
        request: JSONObject,
    ) {
        val resultPath = request.optString("result_path")
        val resultFile = File(resultPath)
        val result =
            try {
                LevelMetadataNativeBridge.analyze(this, requestJson, request.optString("game"))
                    ?: failedJson(request, "Native bridge returned no result", "internal_error")
            } catch (e: Throwable) {
                failedJson(request, e.message ?: e.javaClass.simpleName, "internal_error")
            }
        writeResult(resultFile, result)
    }

    private fun startWatchdog(
        request: JSONObject,
        cancellationFile: File,
    ): Thread =
        Thread {
            val startedAt = SystemClock.elapsedRealtime()
            val totalTimeoutMs = request.optLong("total_timeout_ms", Long.MAX_VALUE)
            val deadline = LevelMetadataProgressDeadline(SystemClock.elapsedRealtime())
            val checkpointFile = File(request.optString("checkpoint_path"))
            var lastCheckpoint = ""
            var cancellationStartedAt = -1L
            while (!Thread.currentThread().isInterrupted) {
                if (cancellationFile.exists()) {
                    if (cancellationStartedAt < 0L) {
                        cancellationStartedAt = SystemClock.elapsedRealtime()
                        Log.w(TAG, "Waiting for level metadata worker checkpoint flush")
                    } else if (SystemClock.elapsedRealtime() - cancellationStartedAt >=
                        LEVEL_METADATA_CANCELLATION_GRACE_MS
                    ) {
                        Log.w(TAG, "Canceling unresponsive level metadata worker")
                        Process.killProcess(Process.myPid())
                        return@Thread
                    }
                }
                if (checkpointFile.isFile) {
                    runCatching { checkpointFile.readText(Charsets.UTF_8) }
                        .getOrNull()
                        ?.takeIf { it != lastCheckpoint }
                        ?.let { checkpoint ->
                            lastCheckpoint = checkpoint
                            LevelMetadataAnalyzer.parseLevelMetadataCheckpointUpdate(checkpoint)?.let { update ->
                                deadline.observe(update, SystemClock.elapsedRealtime())
                            }
                        }
                }
                if (deadline.isExpired(SystemClock.elapsedRealtime())) {
                    Log.e(TAG, "Level metadata worker deadline expired")
                    Process.killProcess(Process.myPid())
                    return@Thread
                }
                if (SystemClock.elapsedRealtime() - startedAt >= totalTimeoutMs) {
                    Log.e(TAG, "Level metadata worker total deadline expired")
                    Process.killProcess(Process.myPid())
                    return@Thread
                }
                try {
                    Thread.sleep(LEVEL_METADATA_POLL_MS)
                } catch (_: InterruptedException) {
                    return@Thread
                }
            }
        }.apply {
            name = "level-metadata-watchdog"
            isDaemon = true
            start()
        }

    private fun writeBusyResult(requestFile: File) {
        val requestJson = requestFile.readText(Charsets.UTF_8)
        val request = JSONObject(requestJson)
        writeResult(
            File(request.optString("result_path")),
            failedJson(request, "Level metadata analysis is already running", "busy"),
        )
    }

    private fun writeResult(
        resultFile: File,
        result: String,
    ) {
        val formattedResult = formatJsonResult(result)
        resultFile.parentFile?.mkdirs()
        val tmp = File(resultFile.parentFile, resultFile.name + ".tmp")
        tmp.writeText(formattedResult, Charsets.UTF_8)
        if (!tmp.renameTo(resultFile)) {
            resultFile.writeText(formattedResult, Charsets.UTF_8)
            tmp.delete()
        }
    }

    private fun formatJsonResult(text: String): String {
        val trimmed = text.trim()
        val formatted =
            try {
                if (trimmed.startsWith("[")) JSONArray(trimmed).toString(2) else JSONObject(trimmed).toString(2)
            } catch (e: Throwable) {
                trimmed
            }
        return formatted + "\n"
    }

    private fun failedJson(
        request: JSONObject,
        problem: String,
        failureKind: String,
    ): String =
        JSONObject()
            .put("schema", "dxx-level-metadata-v1")
            .put("status", "failed")
            .put("request_id", request.optString("request_id"))
            .put("game", request.optString("game"))
            .put("source", request.optString("source_name"))
            .put("failure_kind", failureKind)
            .put("levels", JSONArray())
            .put("problems", JSONArray().put(problem))
            .toString(2)

    companion object {
        const val EXTRA_REQUEST_PATH = "request_path"
        private const val TAG = "DXX-LevelMetadata"
    }
}

class LevelMetadataD1AnalysisService : LevelMetadataAnalysisService()

class LevelMetadataD2AnalysisService : LevelMetadataAnalysisService()

private fun JSONObject.optStringList(name: String): List<String> {
    val array = optJSONArray(name) ?: return emptyList()
    return buildList {
        for (index in 0 until array.length()) {
            val value = array.optString(index)
            if (value.isNotBlank()) add(value)
        }
    }
}

private fun JSONObject.optRouteSteps(name: String): List<LevelMetadataRouteStep> {
    val array = optJSONArray(name) ?: return emptyList()
    return buildList {
        for (index in 0 until array.length()) {
            val step = array.optJSONObject(index) ?: continue
            val opens = step.optJSONArray("opens") ?: JSONArray()
            val labelPosition = step.optJSONObject("label_pos")
            add(
                LevelMetadataRouteStep(
                    index = step.optInt("index", index),
                    kind = step.optString("kind"),
                    activationKind = step.optString("activation_kind"),
                    calculated = step.optBoolean("calculated", true),
                    label = step.optString("label"),
                    seg = step.optInt("seg", -1),
                    side = step.optInt("side", -1),
                    wall = step.optInt("wall", -1),
                    labelPosition =
                        labelPosition?.let {
                            LevelMetadataPosition(
                                x = it.optDouble("x"),
                                y = it.optDouble("y"),
                                z = it.optDouble("z"),
                            )
                        },
                    distance = step.optDouble("distance", 0.0),
                    key = step.optString("key"),
                    canBeBypassed = step.optBoolean("can_be_bypassed", false),
                    keyCarrierObjnum = step.optInt("key_carrier_objnum", -1),
                    trigger = step.optInt("trigger", -1),
                    triggerType = step.optString("trigger_type"),
                    triggerTypeId = step.optInt("trigger_type_id", -1),
                    opens =
                        buildList {
                            for (openIndex in 0 until opens.length()) {
                                val open = opens.optJSONObject(openIndex) ?: continue
                                add(
                                    LevelMetadataRouteOpenLink(
                                        seg = open.optInt("seg", -1),
                                        side = open.optInt("side", -1),
                                        wall = open.optInt("wall", -1),
                                    ),
                                )
                            }
                        },
                ),
            )
        }
    }
}
