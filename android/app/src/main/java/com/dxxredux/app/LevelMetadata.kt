package com.dxxredux.app

import android.app.ActivityManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.os.Process
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.Locale
import java.util.UUID
import java.util.zip.ZipFile

private const val LEVEL_METADATA_TIMEOUT_MS = 30_000L
private const val LEVEL_METADATA_POLL_MS = 200L
private const val LEVEL_METADATA_MAX_ZIP_FILES = 240
private const val LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES = 256L * 1024L * 1024L
private const val LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES = 64L * 1024L * 1024L
private const val LEVEL_METADATA_D1_PROCESS_SUFFIX = ":levelmeta_d1"
private const val LEVEL_METADATA_D2_PROCESS_SUFFIX = ":levelmeta_d2"

internal data class LevelMetadataTarget(
    val displayName: String,
    val game: String,
    val sourceType: String,
    val sourcePath: String? = null,
    val dataDir: String? = null,
    val missionName: String? = null,
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
    val travelStatus: String,
    val travelProblem: String,
    val travelNote: String,
    val travelTargetsReached: Int,
    val travelTargetsTotal: Int,
    val travelKeyDetours: Int,
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
                                travelStatus = row.optString("travel_status", "failed"),
                                travelProblem = row.optString("travel_problem"),
                                travelNote = row.optString("travel_note"),
                                travelTargetsReached = row.optInt("travel_targets_reached"),
                                travelTargetsTotal = row.optInt("travel_targets_total"),
                                travelKeyDetours = row.optInt("travel_key_detours"),
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
            )
        }

        fun failed(
            source: String,
            game: String,
            problem: String,
            diagnostics: List<String> = emptyList(),
            status: String = "failed",
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
        if (ext == "hog" && !isBaseHog(file.name)) {
            descriptorForHog(file, game)?.let { mission ->
                return LevelMetadataTarget(
                    displayName = file.name,
                    game = game,
                    sourceType = "hog",
                    sourcePath = file.absolutePath,
                    dataDir = setDir.absolutePath,
                    missionName = file.name.substringBeforeLast('.'),
                    missionFilename = descriptorFileNameForHog(file, game),
                    missionType = mission.type,
                    hogFile = file.name,
                    normalLevelFiles = mission.levelNames,
                    secretLevelFiles = mission.secretLevelNames,
                )
            }
            hogLevelFiles(
                metadata?.contents.orEmpty(),
            ).takeIf { it.first.isNotEmpty() || it.second.isNotEmpty() }?.let {
                return LevelMetadataTarget(
                    displayName = file.name,
                    game = game,
                    sourceType = "hog",
                    sourcePath = file.absolutePath,
                    dataDir = setDir.absolutePath,
                    missionName = file.name.substringBeforeLast('.'),
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
        ZipFile(archive).use { zip ->
            val entries =
                zip
                    .entries()
                    .asSequence()
                    .filterNot { it.isDirectory }
                    .take(LEVEL_METADATA_MAX_ZIP_FILES + 1)
                    .toList()
            if (entries.size > LEVEL_METADATA_MAX_ZIP_FILES) return null
            entries.forEach { entry ->
                val name = entry.name.substringAfterLast('/').substringAfterLast('\\')
                when {
                    GameFileFormats.isLevelFile(name) -> {
                        archiveEntries += entry.name
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
                                GameFileMetadata.summarizeZipConstituent(archive, entry.name, name)
                            }.getOrNull()
                        val hogLevels = hogLevelFiles(summary?.contents.orEmpty())
                        if (hogLevels.first.isNotEmpty() || hogLevels.second.isNotEmpty()) {
                            archiveEntries += entry.name
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
        val game = gameForFile(constituent.name, metadata?.contents?.map { it.name }.orEmpty()) ?: return null
        val ext = GameFileFormats.extensionOf(constituent.name)
        if (GameFileFormats.isMissionDescriptor(constituent.name)) {
            descriptorTargetForZipConstituent(archivePath, setDir, constituent, game)?.let { return it }
        }
        if (ext == "hog" && !isBaseHog(constituent.name)) {
            descriptorTargetForZipHog(archivePath, setDir, constituent, game)?.let { return it }
            hogLevelFiles(
                metadata?.contents.orEmpty(),
            ).takeIf { it.first.isNotEmpty() || it.second.isNotEmpty() }?.let {
                return LevelMetadataTarget(
                    displayName = constituent.name,
                    game = game,
                    sourceType = "hog",
                    dataDir = setDir.absolutePath,
                    archivePath = archivePath,
                    archiveEntries = listOf(constituent.path),
                    missionName = constituent.name.substringBeforeLast('.'),
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

    private fun isBaseHog(name: String): Boolean =
        when (name.lowercase(Locale.US)) {
            "descent.hog", "descent2.hog", "d2demo.hog" -> true
            else -> false
        }

    private fun hogLevelFiles(entries: List<GameFileMetadata.EntrySummary>): Pair<List<String>, List<String>> {
        val normal = mutableListOf<String>()
        val secret = mutableListOf<String>()
        entries.forEach { entry ->
            when (GameFileFormats.extensionOf(entry.name)) {
                "rdl", "rl2" -> normal += entry.name
                "sdl", "sl2" -> secret += entry.name
            }
        }
        return normal to secret
    }

    private fun descriptorTargetForFile(
        descriptor: File,
        setDir: File,
        game: String,
    ): LevelMetadataTarget? {
        val mission =
            runCatching {
                GameFileFormats.parseMissionDescriptor(descriptor.name, descriptor.readText(Charsets.UTF_8))
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
        ZipFile(archive).use { zip ->
            val descriptorEntry = zip.getEntry(constituent.path) ?: return null
            val mission =
                runCatching {
                    zip.getInputStream(descriptorEntry).bufferedReader().use {
                        GameFileFormats.parseMissionDescriptor(constituent.path, it.readText())
                    }
                }.getOrNull() ?: return null
            val hogEntry = findSameStemZipEntry(zip, constituent.path, "hog") ?: return null
            val hogName = hogEntry.name.substringAfterLast('/').substringAfterLast('\\')
            return LevelMetadataTarget(
                displayName = constituent.name,
                game = game,
                sourceType = "mission_files",
                dataDir = setDir.absolutePath,
                archivePath = archivePath,
                archiveEntries = listOf(constituent.path, hogEntry.name),
                missionName = constituent.name.substringBeforeLast('.'),
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
        ZipFile(archive).use { zip ->
            val descriptorEntry = findSameStemZipEntry(zip, constituent.path, descriptorExt) ?: return null
            val mission =
                runCatching {
                    zip.getInputStream(descriptorEntry).bufferedReader().use {
                        GameFileFormats.parseMissionDescriptor(descriptorEntry.name, it.readText())
                    }
                }.getOrNull() ?: return null
            return LevelMetadataTarget(
                displayName = constituent.name,
                game = game,
                sourceType = "mission_files",
                dataDir = setDir.absolutePath,
                archivePath = archivePath,
                archiveEntries = listOf(constituent.path, descriptorEntry.name),
                missionName = constituent.name.substringBeforeLast('.'),
                missionFilename = descriptorEntry.name.substringAfterLast('/').substringAfterLast('\\'),
                missionType = mission.type,
                hogFiles = listOf(constituent.name),
                normalLevelFiles = mission.levelNames,
                secretLevelFiles = mission.secretLevelNames,
            )
        }
    }

    private fun findSameStemZipEntry(
        zip: ZipFile,
        path: String,
        extension: String,
    ): java.util.zip.ZipEntry? {
        val normalized = path.replace('\\', '/').trim('/')
        val dir = normalized.substringBeforeLast('/', "")
        val stem = normalized.substringAfterLast('/').substringBeforeLast('.')
        val sibling = if (dir.isBlank()) "$stem.$extension" else "$dir/$stem.$extension"
        zip.getEntry(sibling)?.let { return it }
        val wanted = "$stem.$extension".lowercase(Locale.US)
        return zip.entries().asSequence().firstOrNull {
            !it.isDirectory &&
                it.name
                    .substringAfterLast('/')
                    .substringAfterLast('\\')
                    .lowercase(Locale.US) == wanted
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
            GameFileFormats.parseMissionDescriptor(descriptor.name, descriptor.readText(Charsets.UTF_8))
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
    ): LevelMetadataResult =
        withContext(Dispatchers.IO) {
            val appContext = context.applicationContext
            val requestId = UUID.randomUUID().toString()
            val workDir = File(appContext.cacheDir, "level_metadata/$requestId")
            val resultFile = File(workDir, "result.json")
            val checkpointFile = File(workDir, "checkpoint.json")
            val requestFile = File(workDir, "request.json")
            val startedAt = System.currentTimeMillis()
            val request =
                try {
                    buildRequestJson(target, requestId, workDir, resultFile, checkpointFile)
                } catch (e: Exception) {
                    return@withContext LevelMetadataResult.failed(
                        target.displayName,
                        target.game,
                        e.message ?: e.javaClass.simpleName,
                    )
                }

            workDir.mkdirs()
            requestFile.writeText(request.toString(), Charsets.UTF_8)
            val intent =
                Intent(appContext, serviceClassForGame(target.game))
                    .putExtra(LevelMetadataAnalysisService.EXTRA_REQUEST_PATH, requestFile.absolutePath)
            appContext.startService(intent)

            while (System.currentTimeMillis() - startedAt < LEVEL_METADATA_TIMEOUT_MS) {
                if (resultFile.isFile) {
                    return@withContext parseResultFile(target, resultFile)
                }
                if (!isWorkerProcessRunning(appContext, target.game) &&
                    System.currentTimeMillis() - startedAt > 1_000L
                ) {
                    break
                }
                delay(LEVEL_METADATA_POLL_MS)
            }

            killWorkerProcess(appContext, target.game)
            val diagnostics = collectDiagnostics(appContext, startedAt, checkpointFile)
            val status = if (diagnostics.any { it.contains("crash", ignoreCase = true) }) "crashed" else "timeout"
            LevelMetadataResult.failed(
                target.displayName,
                target.game,
                if (status == "crashed") "Analysis worker crashed" else "Analysis timed out",
                diagnostics,
                status,
            )
        }

    private fun serviceClassForGame(game: String): Class<out LevelMetadataAnalysisService> =
        if (game == GameFileFormats.GAME_D1) {
            LevelMetadataD1AnalysisService::class.java
        } else {
            LevelMetadataD2AnalysisService::class.java
        }

    private fun parseResultFile(
        target: LevelMetadataTarget,
        resultFile: File,
    ): LevelMetadataResult =
        try {
            LevelMetadataResult.fromJson(resultFile.readText(Charsets.UTF_8))
        } catch (e: Exception) {
            LevelMetadataResult.failed(target.displayName, target.game, "Bad analysis result: ${e.message}")
        }

    private fun buildRequestJson(
        target: LevelMetadataTarget,
        requestId: String,
        workDir: File,
        resultFile: File,
        checkpointFile: File,
    ): JSONObject {
        val stageDir = File(workDir, "staged")
        val prepared = prepareTarget(target, stageDir)
        return JSONObject()
            .put("schema", "dxx-level-metadata-request-v1")
            .put("request_id", requestId)
            .put("game", target.game)
            .put("source_name", target.displayName)
            .put("source_type", prepared.sourceType)
            .put("data_dir", prepared.dataDir)
            .put("extra_data_dir", prepared.extraDataDir)
            .put("mission_name", prepared.missionName)
            .put("mission_filename", prepared.missionFilename)
            .put("mission_type", target.missionType.orEmpty())
            .put("level_file", prepared.levelFile)
            .put("level_num", prepared.levelNum)
            .put("hog_path", prepared.hogPath)
            .put("hog_paths", JSONArray(prepared.hogPaths))
            .put("normal_level_files", JSONArray(prepared.normalLevelFiles))
            .put("secret_level_files", JSONArray(prepared.secretLevelFiles))
            .put("result_path", resultFile.absolutePath)
            .put("checkpoint_path", checkpointFile.absolutePath)
    }

    private data class PreparedTarget(
        val sourceType: String,
        val dataDir: String,
        val extraDataDir: String,
        val missionName: String,
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
            stageZipEntries(File(archivePath), target.archiveEntries, stageDir)
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
        return PreparedTarget(
            sourceType = target.sourceType,
            dataDir = target.dataDir ?: source?.parentFile?.absolutePath.orEmpty(),
            extraDataDir = "",
            missionName = target.missionName.orEmpty(),
            missionFilename = target.missionFilename.orEmpty(),
            levelFile = target.levelFile ?: source?.name.orEmpty(),
            levelNum = target.levelNum,
            hogPath = if (target.sourceType == "hog") source?.absolutePath.orEmpty() else "",
            hogPaths = if (target.sourceType == "hog") listOfNotNull(source?.absolutePath) else emptyList(),
            normalLevelFiles = target.normalLevelFiles,
            secretLevelFiles = target.secretLevelFiles,
        )
    }

    private fun stageZipEntries(
        archive: File,
        entryPaths: List<String>,
        stageDir: File,
    ) {
        if (!archive.isFile) throw IllegalArgumentException("Mission ZIP is missing")
        if (entryPaths.size > LEVEL_METADATA_MAX_ZIP_FILES) throw IllegalArgumentException("Too many ZIP entries")
        stageDir.mkdirs()
        var total = 0L
        val buffer = ByteArray(8192)
        val usedNames = mutableSetOf<String>()
        ZipFile(archive).use { zip ->
            entryPaths.forEach { path ->
                val entry = zip.getEntry(path) ?: throw IllegalArgumentException("ZIP entry is missing: $path")
                if (entry.isDirectory) return@forEach
                val leaf = path.substringAfterLast('/').substringAfterLast('\\')
                if (leaf.isBlank() || leaf == "." || leaf == ".." || !usedNames.add(leaf.lowercase(Locale.US))) {
                    throw IllegalArgumentException("Unsafe or duplicate ZIP entry: $path")
                }
                val declaredSize = entry.size.coerceAtLeast(0)
                if (declaredSize >
                    LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES
                ) {
                    throw IllegalArgumentException("ZIP entry is too large: $leaf")
                }
                if (declaredSize > 0 && total + declaredSize >
                    LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES
                ) {
                    throw IllegalArgumentException("ZIP package is too large")
                }
                val out = File(stageDir, leaf)
                var copied = 0L
                zip.getInputStream(entry).use { input ->
                    out.outputStream().use { output ->
                        while (true) {
                            val read = input.read(buffer)
                            if (read <= 0) break
                            copied += read.toLong()
                            if (copied > LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES) {
                                throw IllegalArgumentException("ZIP entry is too large: $leaf")
                            }
                            if (total + copied > LEVEL_METADATA_MAX_ZIP_TOTAL_BYTES) {
                                throw IllegalArgumentException("ZIP package is too large")
                            }
                            output.write(buffer, 0, read)
                        }
                    }
                }
                if (GameFileFormats.isMissionDescriptor(leaf)) {
                    val missionOut = File(File(stageDir, "missions"), leaf)
                    missionOut.parentFile?.mkdirs()
                    out.copyTo(missionOut, overwrite = true)
                }
                total += copied
            }
        }
    }

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

    private fun isWorkerProcessRunning(
        context: Context,
        game: String,
    ): Boolean = levelMetadataWorkerProcess(context, game) != null

    private fun killWorkerProcess(
        context: Context,
        game: String,
    ) {
        levelMetadataWorkerProcess(context, game)?.let { Process.killProcess(it.pid) }
    }

    private fun levelMetadataWorkerProcess(
        context: Context,
        game: String,
    ): ActivityManager.RunningAppProcessInfo? {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager ?: return null
        val processName =
            context.packageName +
                if (game == GameFileFormats.GAME_D1) {
                    LEVEL_METADATA_D1_PROCESS_SUFFIX
                } else {
                    LEVEL_METADATA_D2_PROCESS_SUFFIX
                }
        return activityManager.runningAppProcesses?.firstOrNull { it.processName == processName }
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

open class LevelMetadataAnalysisService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        val requestPath = intent?.getStringExtra(EXTRA_REQUEST_PATH)
        if (requestPath == null) {
            stopSelf(startId)
            return START_NOT_STICKY
        }
        Thread {
            runCatching { runAnalysis(File(requestPath)) }
                .onFailure { Log.e(TAG, "Level metadata analysis failed", it) }
            stopSelf(startId)
        }.start()
        return START_NOT_STICKY
    }

    private fun runAnalysis(requestFile: File) {
        val requestJson = requestFile.readText(Charsets.UTF_8)
        val request = JSONObject(requestJson)
        val resultPath = request.optString("result_path")
        val resultFile = File(resultPath)
        val result =
            try {
                LevelMetadataNativeBridge.analyze(this, requestJson, request.optString("game"))
                    ?: failedJson(request, "Native bridge returned no result")
            } catch (e: Throwable) {
                failedJson(request, e.message ?: e.javaClass.simpleName)
            }
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
    ): String =
        JSONObject()
            .put("schema", "dxx-level-metadata-v1")
            .put("status", "failed")
            .put("request_id", request.optString("request_id"))
            .put("game", request.optString("game"))
            .put("source", request.optString("source_name"))
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
