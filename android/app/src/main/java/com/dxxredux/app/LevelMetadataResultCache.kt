package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.util.Locale

private const val LEVEL_METADATA_RESULT_CACHE_SCHEMA = "dxx-level-metadata-result-cache-v4"
private const val LEVEL_METADATA_RESULT_CACHE_MAX_FILES = 512
private const val LEVEL_METADATA_RESULT_CACHE_MAX_BYTES = 256L * 1024L * 1024L
private const val LEVEL_METADATA_RESULT_CACHE_FILE_MAX_BYTES = 32L * 1024L * 1024L

internal object LevelMetadataResultCache {
    data class Identity(
        val key: String,
        val sourceFiles: Int,
        val sourceBytes: Long,
    )

    fun identify(target: LevelMetadataTarget): Identity? {
        // Explicit mission bundles include their complete source tree and ordered HOG mounts
        // Generic loose-file targets still lack a complete description of their mount scope
        val sourceDirectory = target.sourcePath?.let(::File)?.takeIf(File::isDirectory)
        val missionDirectory = sourceDirectory != null && target.sourceType == "mission_files"
        if ((target.archivePath.isNullOrBlank() && !missionDirectory) ||
            (sourceDirectory != null && !missionDirectory)
        ) {
            return null
        }
        val files = sourceFiles(target)
        if (files.none { it.first.startsWith("source:") }) return null
        val fileIdentities = JSONArray()
        var totalBytes = 0L
        files.forEach { (role, file) ->
            val hash = sha256(file) ?: return null
            totalBytes += file.length()
            fileIdentities.put(
                JSONObject()
                    .put("role", role)
                    .put("size", file.length())
                    .put("sha256", hash),
            )
        }
        val definition =
            JSONObject()
                .put("schema", LEVEL_METADATA_RESULT_CACHE_SCHEMA)
                .put("route_cache_generation", ROUTE_METADATA_CACHE_GENERATION)
                .put("game", target.game)
                .put("source_type", target.sourceType)
                .put("mission_name", target.missionName.orEmpty())
                .put("mission_filename", target.missionFilename.orEmpty())
                .put("mission_type", target.missionType.orEmpty())
                .put("mission_mode_flags", JSONArray(target.missionModeFlags.sorted()))
                .put("level_file", target.levelFile.orEmpty())
                .put("level_num", target.levelNum)
                .put("hog_file", target.hogFile.orEmpty())
                .put("hog_files", JSONArray(target.hogFiles))
                .put("normal_level_files", JSONArray(target.normalLevelFiles))
                .put("secret_level_files", JSONArray(target.secretLevelFiles))
                .put("archive_entries", JSONArray(target.archiveEntries))
                .put("files", fileIdentities)
        return Identity(sha256(definition.toString().toByteArray(Charsets.UTF_8)), files.size, totalBytes)
    }

    fun read(
        root: File,
        identity: Identity,
        target: LevelMetadataTarget,
        expectedLevelCount: Int,
    ): LevelMetadataResult? {
        val file = cacheFile(root, identity)
        if (!file.isFile || file.length() !in 1..LEVEL_METADATA_RESULT_CACHE_FILE_MAX_BYTES) return null
        return runCatching {
            val envelope = JSONObject(file.readText(Charsets.UTF_8))
            check(envelope.optString("schema") == LEVEL_METADATA_RESULT_CACHE_SCHEMA)
            check(envelope.optInt("route_cache_generation") == ROUTE_METADATA_CACHE_GENERATION)
            check(envelope.optString("identity") == identity.key)
            val result = LevelMetadataResult.fromJson(envelope.getJSONObject("result").toString())
            check(cacheable(result, target, expectedLevelCount))
            val gameWriteDir = File(root.parentFile, "${target.game}x-redux")
            check(
                result.levels.all { level ->
                    level.routeCacheFile.isBlank() || File(gameWriteDir, level.routeCacheFile).isFile
                },
            )
            file.setLastModified(System.currentTimeMillis())
            result
        }.getOrElse {
            file.delete()
            null
        }
    }

    fun publish(
        root: File,
        identity: Identity,
        target: LevelMetadataTarget,
        expectedLevelCount: Int,
        resultText: String,
        result: LevelMetadataResult,
    ): Boolean {
        if (!cacheable(result, target, expectedLevelCount)) return false
        val envelope =
            JSONObject()
                .put("schema", LEVEL_METADATA_RESULT_CACHE_SCHEMA)
                .put("route_cache_generation", ROUTE_METADATA_CACHE_GENERATION)
                .put("identity", identity.key)
                .put("result", JSONObject(resultText))
                .toString(2) + "\n"
        if (envelope.toByteArray(Charsets.UTF_8).size > LEVEL_METADATA_RESULT_CACHE_FILE_MAX_BYTES) return false
        AtomicFilePublication.writeUtf8(cacheFile(root, identity), envelope)
        prune(root)
        return true
    }

    internal fun cacheFile(
        root: File,
        identity: Identity,
    ): File = File(root, "g$ROUTE_METADATA_CACHE_GENERATION/${identity.key}.json")

    private fun cacheable(
        result: LevelMetadataResult,
        target: LevelMetadataTarget,
        expectedLevelCount: Int,
    ): Boolean =
        result.status == "ok" &&
            result.game == target.game &&
            result.levels.size == expectedLevelCount &&
            result.levels.all {
                // A finished scan can contain a partial route, such as an unreachable key
                // Route readiness describes navigation, not whether the scan has finished
                it.status == "ok" && it.secretAreasComplete &&
                    it.failureKind.isBlank() &&
                    it.routeReadiness in setOf("", "complete", "next_ready", "partial")
            }

    private fun sourceFiles(target: LevelMetadataTarget): List<Pair<String, File>> {
        val files = mutableListOf<Pair<String, File>>()
        target.archivePath?.let { files += "source:archive" to File(it) }
        target.sourcePath?.let { path ->
            val source = File(path)
            if (source.isFile) {
                files += "source:file" to source
            } else if (source.isDirectory && target.sourceType == "mission_files") {
                val entries = source.walkTopDown().filter(File::isFile).toList()
                val names = entries.map { it.relativeTo(source).invariantSeparatorsPath.lowercase(Locale.US) }
                if (names.distinct().size != names.size) return emptyList()
                entries.zip(names).forEach { (file, name) -> files += "source:mission:$name" to file }
            }
        }

        // Include all installed semantic assets, including non-groupa palettes/PIGs
        val semanticExtensions =
            setOf("hog", "ham", "pig", "pog", "pg1", "dtx", "256", "bin", "tbl", "rl2", "rdl", "mn2", "msn", "hxm")
        listOfNotNull(target.dataDir, target.extraDataDir).distinct().forEachIndexed { index, path ->
            val dataDir = File(path)
            val entries = dataDir.listFiles() ?: return emptyList()
            val semanticFiles =
                entries.filter {
                    it.isFile && it.extension.lowercase(Locale.US) in semanticExtensions
                }
            if (semanticFiles.map { it.name.lowercase(Locale.US) }.distinct().size !=
                semanticFiles.size
            ) {
                return emptyList()
            }
            semanticFiles.forEach { file ->
                files += "base$index:${file.name.lowercase(Locale.US)}" to file
            }
        }
        return files
            .filter { it.second.isFile }
            .distinctBy { it.second.canonicalPath.lowercase(Locale.US) }
            .sortedBy { it.first }
    }

    private fun prune(root: File) {
        val files = root.listFiles()?.flatMap { it.listFiles()?.filter(File::isFile).orEmpty() }.orEmpty()
        var bytes = files.sumOf(File::length)
        var count = files.size
        files.sortedBy(File::lastModified).forEach { file ->
            if (count <= LEVEL_METADATA_RESULT_CACHE_MAX_FILES && bytes <= LEVEL_METADATA_RESULT_CACHE_MAX_BYTES) {
                return
            }
            val length = file.length()
            if (file.delete()) {
                count--
                bytes -= length
            }
        }
    }

    private fun sha256(file: File): String? =
        runCatching {
            val digest = MessageDigest.getInstance("SHA-256")
            file.inputStream().buffered().use { input ->
                val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    digest.update(buffer, 0, count)
                }
            }
            digest.digest().toHex()
        }.getOrNull()

    private fun sha256(bytes: ByteArray): String =
        MessageDigest
            .getInstance("SHA-256")
            .digest(bytes)
            .toHex()

    private fun ByteArray.toHex(): String = joinToString("") { "%02x".format(Locale.US, it) }
}
