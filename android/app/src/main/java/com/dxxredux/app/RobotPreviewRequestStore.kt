package com.dxxredux.app

import org.json.JSONObject
import java.io.File
import java.util.Locale

private const val ROBOT_PREVIEW_CACHE_DIR = "robot_preview"
private const val ROBOT_PREVIEW_CACHE_MAX_AGE_MS = 24L * 60L * 60L * 1_000L

internal data class RobotPreviewLaunchRequest(
    val game: String,
    val requestFile: File,
)

internal data class RobotPreviewRuntimeRequest(
    val game: String,
    val requestFile: File,
    val dataDir: File,
    val robotNumber: Int,
    val robotLabel: String,
)

internal object RobotPreviewRequestStore {
    fun baseGameForFile(name: String): String? =
        when (name.lowercase(Locale.US)) {
            "descent.hog", "descent.pig" -> GameFileFormats.GAME_D1
            "descent2.hog", "d2demo.hog", "descent2.ham", "d2demo.ham" -> GameFileFormats.GAME_D2
            else -> null
        }

    fun findSourceLevel(
        levels: List<LevelMetadataLevelRow>,
        item: LevelMetadataReplacementItem,
    ): LevelMetadataLevelRow? =
        levels.firstOrNull { row -> row.replacementGroups.any { group -> group.items.contains(item) } }

    fun create(
        cacheDir: File,
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
        item: LevelMetadataReplacementItem,
        robotLabel: String,
    ): RobotPreviewLaunchRequest {
        validate(target, row, item)
        val cacheRoot = root(cacheDir)
        OwnedCacheDirectories.prune(cacheRoot, ROBOT_PREVIEW_CACHE_MAX_AGE_MS)
        val workDir = OwnedCacheDirectories.create(cacheRoot)
        try {
            val previewWriteDir = File(workDir, "runtime-write").canonicalFile
            check(previewWriteDir.mkdir()) { "Could not create isolated robot preview write directory" }
            val robotNumbers =
                row.replacementGroups
                    .flatMap { group -> group.items }
                    .filter { replacement -> replacement.kind == "robot" && replacement.number >= 0 }
                    .map { replacement -> replacement.number }
                    .distinct()
                    .sorted()
            val request =
                LevelMetadataAnalyzer.buildRobotPreviewRequestJson(
                    target,
                    row,
                    workDir.name,
                    workDir,
                    previewWriteDir,
                    item.number,
                    robotLabel,
                    robotNumbers,
                )
            val requestFile =
                OwnedCacheDirectories.writeUtf8Atomically(
                    workDir,
                    "request.json",
                    request.toString(2) + "\n",
                )
            return RobotPreviewLaunchRequest(target.game, requestFile)
        } catch (e: Exception) {
            OwnedCacheDirectories.delete(cacheRoot, workDir)
            throw e
        }
    }

    fun createBase(
        cacheDir: File,
        game: String,
        dataDir: File,
        robotNumber: Int,
        robotLabel: String,
    ): RobotPreviewLaunchRequest {
        require(game == GameFileFormats.GAME_D1 || game == GameFileFormats.GAME_D2) {
            "Unsupported robot preview game"
        }
        val canonicalDataDir = dataDir.canonicalFile
        require(canonicalDataDir.isDirectory) { "Robot preview base data directory is missing" }
        require(robotNumber >= 0) { "Robot preview number is invalid" }
        val cacheRoot = root(cacheDir)
        OwnedCacheDirectories.prune(cacheRoot, ROBOT_PREVIEW_CACHE_MAX_AGE_MS)
        val workDir = OwnedCacheDirectories.create(cacheRoot)
        try {
            val previewWriteDir = File(workDir, "runtime-write").canonicalFile
            check(previewWriteDir.mkdir()) { "Could not create isolated robot preview write directory" }
            val request =
                LevelMetadataAnalyzer.buildBaseRobotPreviewRequestJson(
                    game,
                    canonicalDataDir,
                    workDir.name,
                    previewWriteDir,
                    robotNumber,
                    robotLabel,
                )
            val requestFile =
                OwnedCacheDirectories.writeUtf8Atomically(
                    workDir,
                    "request.json",
                    request.toString(2) + "\n",
                )
            return RobotPreviewLaunchRequest(game, requestFile)
        } catch (e: Exception) {
            OwnedCacheDirectories.delete(cacheRoot, workDir)
            throw e
        }
    }

    fun validateForLaunch(
        cacheDir: File,
        requestPath: String,
        expectedGame: String,
    ): RobotPreviewRuntimeRequest {
        val requestFile = File(requestPath).canonicalFile
        val workDir =
            requestFile.parentFile ?: throw IllegalArgumentException("Robot preview request directory is missing")
        require(requestFile.name == "request.json" && OwnedCacheDirectories.isOwned(root(cacheDir), workDir)) {
            "Robot preview request is outside the preview cache"
        }
        val request = JSONObject(requestFile.readText(Charsets.UTF_8))
        require(request.optString("schema") == "dxx-robot-preview-request-v1") { "Unsupported robot preview request" }
        require(
            request.optString("request_id") == workDir.name,
        ) { "Robot preview identity does not match its directory" }
        require(request.optString("game") == expectedGame) { "Robot preview game does not match the Activity" }
        val previewWriteDir = File(request.optString("preview_write_dir")).canonicalFile
        require(previewWriteDir.isDirectory && previewWriteDir.parentFile == workDir.canonicalFile) {
            "Robot preview write directory is outside the request directory"
        }
        val dataDir = File(request.optString("data_dir")).canonicalFile
        require(dataDir.isDirectory) { "Robot preview base data directory is missing" }
        val robotNumber = request.optInt("robot_number", -1)
        require(robotNumber >= 0) { "Robot preview number is invalid" }
        request.optJSONArray("robot_numbers")?.let { numbers ->
            val navigationNumbers =
                List(numbers.length()) { index -> numbers.optInt(index, -1) }
            require(navigationNumbers.isNotEmpty() && navigationNumbers.all { it >= 0 }) {
                "Robot preview navigation list is invalid"
            }
            require(navigationNumbers.distinct().size == navigationNumbers.size) {
                "Robot preview navigation list contains duplicates"
            }
            require(robotNumber in navigationNumbers) {
                "Robot preview navigation list does not contain the selected robot"
            }
        }
        return RobotPreviewRuntimeRequest(
            expectedGame,
            requestFile,
            dataDir,
            robotNumber,
            request.optString("robot_label", "Robot $robotNumber"),
        )
    }

    fun delete(
        cacheDir: File,
        requestPath: String,
    ): Boolean {
        val requestFile = File(requestPath)
        if (requestFile.name != "request.json") return false
        return OwnedCacheDirectories.delete(root(cacheDir), requestFile.parentFile ?: return false)
    }

    private fun validate(
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
        item: LevelMetadataReplacementItem,
    ) {
        require(target.game == GameFileFormats.GAME_D1 || target.game == GameFileFormats.GAME_D2) {
            "Unsupported robot preview game"
        }
        require(row.status.lowercase(Locale.US) == "ok") { "Robot preview source level was not scanned successfully" }
        require(row.levelFile.isNotBlank()) { "Robot preview source level is missing" }
        require(item.kind == "robot" && item.number >= 0) { "Replacement is not a previewable robot" }
        require(row.replacementGroups.any { group -> group.items.contains(item) }) {
            "Robot replacement is not present in the selected source level"
        }
    }

    private fun root(cacheDir: File): File = File(cacheDir, ROBOT_PREVIEW_CACHE_DIR)
}
