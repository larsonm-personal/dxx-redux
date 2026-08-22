package com.dxxredux.app

import org.json.JSONObject
import java.io.File
import java.util.Locale

private const val LEVEL_PREVIEW_CACHE_DIR = "level_preview"
private const val LEVEL_PREVIEW_CACHE_MAX_AGE_MS = 24L * 60L * 60L * 1_000L

internal data class LevelPreviewLaunchRequest(
    val game: String,
    val requestFile: File,
)

internal data class LevelPreviewRuntimeRequest(
    val game: String,
    val requestFile: File,
    val dataDir: File,
)

internal object LevelPreviewRequestStore {
    fun create(
        cacheDir: File,
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
    ): LevelPreviewLaunchRequest {
        validateLevel(target, row)
        val cacheRoot = root(cacheDir)
        OwnedCacheDirectories.prune(cacheRoot, LEVEL_PREVIEW_CACHE_MAX_AGE_MS)
        val workDir = OwnedCacheDirectories.create(cacheRoot)
        try {
            val previewWriteDir = File(workDir, "runtime-write").canonicalFile
            check(previewWriteDir.mkdir()) { "Could not create isolated preview write directory" }
            val request =
                LevelMetadataAnalyzer.buildPreviewRequestJson(
                    canonicalTarget(target),
                    row,
                    workDir.name,
                    workDir,
                    previewWriteDir,
                )
            val requestFile =
                OwnedCacheDirectories.writeUtf8Atomically(
                    workDir,
                    "request.json",
                    request.toString(2) + "\n",
                )
            return LevelPreviewLaunchRequest(target.game, requestFile)
        } catch (e: Exception) {
            OwnedCacheDirectories.delete(cacheRoot, workDir)
            throw e
        }
    }

    fun delete(
        cacheDir: File,
        requestPath: String,
    ): Boolean {
        val requestFile = File(requestPath)
        if (requestFile.name != "request.json") return false
        return OwnedCacheDirectories.delete(root(cacheDir), requestFile.parentFile ?: return false)
    }

    fun validateForLaunch(
        cacheDir: File,
        requestPath: String,
        expectedGame: String,
    ): LevelPreviewRuntimeRequest {
        val requestFile = File(requestPath).canonicalFile
        val workDir = requestFile.parentFile ?: throw IllegalArgumentException("Preview request directory is missing")
        require(requestFile.name == "request.json" && OwnedCacheDirectories.isOwned(root(cacheDir), workDir)) {
            "Preview request is outside the preview cache"
        }
        val request = JSONObject(requestFile.readText(Charsets.UTF_8))
        require(request.optString("schema") == "dxx-level-preview-request-v1") { "Unsupported preview request" }
        require(
            request.optString("request_id") == workDir.name,
        ) { "Preview request identity does not match its directory" }
        require(request.optString("game") == expectedGame) { "Preview request game does not match the Activity" }
        val previewWriteDir = File(request.optString("preview_write_dir")).canonicalFile
        require(previewWriteDir.isDirectory && previewWriteDir.parentFile == workDir.canonicalFile) {
            "Preview write directory is outside the request directory"
        }
        val dataDir = File(request.optString("data_dir")).canonicalFile
        require(dataDir.isDirectory) { "Preview base data directory is missing" }
        return LevelPreviewRuntimeRequest(expectedGame, requestFile, dataDir)
    }

    fun prune(cacheDir: File): Int = OwnedCacheDirectories.prune(root(cacheDir), LEVEL_PREVIEW_CACHE_MAX_AGE_MS)

    private fun validateLevel(
        target: LevelMetadataTarget,
        row: LevelMetadataLevelRow,
    ) {
        require(target.game == GameFileFormats.GAME_D1 || target.game == GameFileFormats.GAME_D2) {
            "Unsupported preview game"
        }
        require(row.status.lowercase(Locale.US) == "ok") { "Only successfully scanned levels can be previewed" }
        require(row.levelFile.isNotBlank()) { "Preview level filename is missing" }
        val knownLevels = target.normalLevelFiles + target.secretLevelFiles + listOfNotNull(target.levelFile)
        if (knownLevels.isNotEmpty()) {
            require(knownLevels.any { it.equals(row.levelFile, ignoreCase = true) }) {
                "Preview level is not part of the selected metadata target"
            }
        }
    }

    private fun canonicalTarget(target: LevelMetadataTarget): LevelMetadataTarget =
        target.copy(
            sourcePath = target.sourcePath?.let { canonicalInput(it, "source") },
            dataDir =
                canonicalDirectory(
                    requireNotNull(target.dataDir) { "Preview base data directory is missing" },
                    "base data",
                ),
            extraDataDir = target.extraDataDir?.let { canonicalDirectory(it, "mission data") },
            archivePath = target.archivePath?.let { canonicalFile(it, "archive") },
        )

    private fun canonicalInput(
        path: String,
        label: String,
    ): String {
        val file = File(path).canonicalFile
        require(file.exists()) { "Preview $label is missing" }
        return file.absolutePath
    }

    private fun canonicalDirectory(
        path: String,
        label: String,
    ): String {
        val file = File(path).canonicalFile
        require(file.isDirectory) { "Preview $label directory is missing" }
        return file.absolutePath
    }

    private fun canonicalFile(
        path: String,
        label: String,
    ): String {
        val file = File(path).canonicalFile
        require(file.isFile) { "Preview $label file is missing" }
        return file.absolutePath
    }

    private fun root(cacheDir: File): File = File(cacheDir, LEVEL_PREVIEW_CACHE_DIR)
}
