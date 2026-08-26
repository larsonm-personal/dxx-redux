package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.security.MessageDigest
import java.util.Locale

internal const val MISSION_ZIP_EXTRACTED_DIR = ".extracted_mission_zips"
internal const val MISSION_ZIP_GENERATED_MISSION_DIR = "missions"

private const val MANIFEST_FILE = "manifest.json"
private const val MANIFEST_SCHEMA = "dxx-mission-zip-extractions-v3"
private val MISSION_ZIP_SONG_LIST_FILES = setOf("descent.sng", "dxx-r.sng")

internal data class MissionZipExtractedFile(
    val entryPath: String,
    val relativePath: String,
    val sizeBytes: Long,
    val contentSha256: String = "",
)

internal data class MissionZipExtractionRecord(
    val ownerFilename: String,
    val ownerSizeBytes: Long,
    val ownerLastModifiedMs: Long,
    val ownerSha256: String = "",
    val rootDir: File,
    val files: List<MissionZipExtractedFile>,
    val archiveFormat: String = "zip",
    val importMode: String = "extracted_bundle",
    val sourceArchiveName: String = ownerFilename,
    val extractedSizeBytes: Long = files.sumOf { it.sizeBytes },
    val fileCount: Int = files.size,
)

internal data class MissionZipLinkedFile(
    val ownerFilename: String,
    val ownerFile: File,
    val rootDir: File,
    val relativePath: String,
    val sourceExists: Boolean,
)

internal data class MissionZipExtractedEntry(
    val file: File,
    val relativePath: String,
    val rootDir: File,
)

internal class MissionZipExtractionStore(
    private val filesDir: File,
) {
    private val modsDir get() = File(filesDir, "mods")
    private val rootDir get() = File(modsDir, MISSION_ZIP_EXTRACTED_DIR)
    private val manifestFile get() = File(rootDir, MANIFEST_FILE)

    fun ensureExtracted(
        ownerFilename: String,
        modFile: File,
        scan: MissionZip.ScanResult,
        onProgress: (Long, Long, String) -> Unit = { _, _, _ -> },
    ): MissionZipExtractionRecord {
        freshRecord(ownerFilename, modFile)?.let { return it }
        if (!modFile.isFile) throw IOException("Mission archive is missing: ${modFile.absolutePath}")
        val ownerSizeBytes = modFile.length()
        val ownerLastModifiedMs = modFile.lastModified()
        val ownerSha256 = missionZipFileSha256(modFile)
        val ownerRoot = extractedRoot(ownerFilename)
        val tempRoot = File(rootDir, "${safeMissionZipDirName(ownerFilename)}.tmp")
        tempRoot.deleteRecursively()
        val extractedFiles =
            try {
                extractZipToRoot(modFile, tempRoot, scan, onProgress)
            } catch (e: Exception) {
                tempRoot.deleteRecursively()
                throw e
            }
        val ownerSha256After = missionZipFileSha256(modFile)
        if (modFile.length() != ownerSizeBytes ||
            modFile.lastModified() != ownerLastModifiedMs ||
            ownerSha256After != ownerSha256
        ) {
            tempRoot.deleteRecursively()
            throw IOException("Mission archive changed during extraction: ${modFile.name}")
        }
        val files =
            extractedFiles.map { file ->
                val output = File(tempRoot, file.relativePath.replace('/', File.separatorChar))
                file.copy(contentSha256 = missionZipFileSha256(output))
            }
        try {
            ownerRoot.deleteRecursively()
            ownerRoot.parentFile?.mkdirs()
            if (!tempRoot.renameTo(ownerRoot)) {
                tempRoot.copyRecursively(ownerRoot, overwrite = true)
                tempRoot.deleteRecursively()
            }
        } catch (e: Exception) {
            ownerRoot.deleteRecursively()
            tempRoot.deleteRecursively()
            throw e
        }
        val record =
            MissionZipExtractionRecord(
                ownerFilename = ownerFilename,
                ownerSizeBytes = ownerSizeBytes,
                ownerLastModifiedMs = ownerLastModifiedMs,
                ownerSha256 = ownerSha256,
                rootDir = ownerRoot,
                files = files,
                archiveFormat = scan.archiveFormat,
                importMode = scan.importMode,
                sourceArchiveName = modFile.name,
            )
        upsert(record)
        return record
    }

    fun freshRecord(
        ownerFilename: String,
        modFile: File? = File(modsDir, ownerFilename),
    ): MissionZipExtractionRecord? {
        val record = records().firstOrNull { it.ownerFilename == ownerFilename } ?: return null
        if (!record.hasValidContentIdentity()) return null
        if (!record.rootDir.isDirectory) return null
        if (modFile != null &&
            (
                !modFile.isFile ||
                    modFile.length() != record.ownerSizeBytes ||
                    modFile.lastModified() != record.ownerLastModifiedMs ||
                    record.ownerSha256.isBlank() ||
                    runCatching { missionZipFileSha256(modFile) }.getOrNull() != record.ownerSha256
            )
        ) {
            return null
        }
        val allFilesPresent =
            record.files.all { file ->
                File(record.rootDir, file.relativePath.replace('/', File.separatorChar)).let {
                    it.isFile &&
                        it.length() == file.sizeBytes &&
                        file.contentSha256.isNotBlank() &&
                        runCatching { missionZipFileSha256(it) }.getOrNull() == file.contentSha256
                }
            }
        return if (allFilesPresent) record else null
    }

    fun activePathLines(
        ownerFilename: String,
        modFile: File,
    ): List<String>? = freshRecord(ownerFilename, modFile)?.let { activePathLines(it) }

    fun activePathLines(
        ownerFilename: String,
        modFile: File,
        scan: MissionZip.ScanResult,
    ): List<String> {
        val record = ensureExtracted(ownerFilename, modFile, scan)
        return activePathLines(record)
    }

    private fun activePathLines(record: MissionZipExtractionRecord): List<String> =
        buildList {
            add(record.rootDir.absolutePath)
            for (file in record.files.filter { GameFileFormats.extensionOf(it.relativePath) == "dxa" }) {
                val archive = File(record.rootDir, file.relativePath.replace('/', File.separatorChar))
                if (archive.isFile) add(archive.absolutePath)
            }
        }

    fun extractedFileForRecordEntry(
        record: MissionZipExtractionRecord,
        entryPath: String,
    ): File? {
        val normalized = entryPath.replace('\\', '/').trim('/')
        val extracted = record.files.firstOrNull { it.entryPath.equals(normalized, ignoreCase = true) } ?: return null
        val file = File(record.rootDir, extracted.relativePath.replace('/', File.separatorChar))
        return file.takeIf { it.isFile }
    }

    fun extractedSummaryForRecordEntry(
        record: MissionZipExtractionRecord,
        entryPath: String,
    ): GameFileMetadata.Summary? =
        extractedFileForRecordEntry(record, entryPath)?.let(GameFileMetadata::summarizeLocalFile)

    fun removeOwner(ownerFilename: String): Boolean {
        var removed = false
        records().firstOrNull { it.ownerFilename == ownerFilename }?.let {
            if (it.rootDir.exists()) {
                it.rootDir.deleteRecursively()
                removed = true
            }
        }
        val root = extractedRoot(ownerFilename)
        if (root.exists()) {
            root.deleteRecursively()
            removed = true
        }
        val kept = records().filterNot { it.ownerFilename == ownerFilename }
        saveRecords(kept)
        return removed
    }

    fun pruneMissingOwners(): List<String> {
        val removed = mutableListOf<String>()
        val kept = mutableListOf<MissionZipExtractionRecord>()
        for (record in records()) {
            val owner = File(modsDir, record.ownerFilename)
            if (owner.isFile && owner.length() == record.ownerSizeBytes) {
                kept += record
            } else {
                record.rootDir.deleteRecursively()
                removed += record.ownerFilename
            }
        }
        if (removed.isNotEmpty()) saveRecords(kept)
        return removed
    }

    fun linkedFilesByAbsolutePath(): Map<String, MissionZipLinkedFile> =
        buildMap {
            for (record in records()) {
                val ownerFile = File(modsDir, record.ownerFilename)
                for (file in record.files) {
                    val diskFile = File(record.rootDir, file.relativePath.replace('/', File.separatorChar))
                    if (!diskFile.isFile) continue
                    put(
                        diskFile.absolutePath,
                        MissionZipLinkedFile(
                            ownerFilename = record.ownerFilename,
                            ownerFile = ownerFile,
                            rootDir = record.rootDir,
                            relativePath = file.relativePath,
                            sourceExists = ownerFile.isFile,
                        ),
                    )
                }
            }
        }

    fun extractedEntryForArchiveEntry(
        archivePath: String,
        entryPath: String,
    ): MissionZipExtractedEntry? {
        val archive = File(archivePath)
        val normalized = entryPath.replace('\\', '/').trim('/')
        val record = freshRecord(archive.name, archive) ?: return null
        val extracted = record.files.firstOrNull { it.entryPath.equals(normalized, ignoreCase = true) } ?: return null
        val file = File(record.rootDir, extracted.relativePath.replace('/', File.separatorChar))
        if (!file.isFile) return null
        return MissionZipExtractedEntry(file, extracted.relativePath, record.rootDir)
    }

    fun findExtractedSameStemEntry(
        archivePath: String,
        entryPath: String,
        extension: String,
    ): MissionZipExtractedEntry? {
        val archive = File(archivePath)
        val normalized = entryPath.replace('\\', '/').trim('/')
        val record = freshRecord(archive.name, archive) ?: return null
        val dir = normalized.substringBeforeLast('/', "")
        val stem = normalized.substringAfterLast('/').substringBeforeLast('.')
        val sibling = if (dir.isBlank()) "$stem.$extension" else "$dir/$stem.$extension"
        val extracted =
            record.files.firstOrNull { it.entryPath.equals(sibling, ignoreCase = true) }
                ?: record.files.firstOrNull {
                    it.entryPath
                        .substringAfterLast('/')
                        .lowercase(Locale.US) == "$stem.$extension".lowercase(Locale.US)
                }
                ?: return null
        val file = File(record.rootDir, extracted.relativePath.replace('/', File.separatorChar))
        if (!file.isFile) return null
        return MissionZipExtractedEntry(file, extracted.relativePath, record.rootDir)
    }

    fun extractedTarget(
        archivePath: String,
        setDir: File,
        scan: MissionZip.ScanResult,
        missionSet: MissionZip.MissionSet,
    ): LevelMetadataTarget? {
        val archive = File(archivePath)
        val ownerFilename = archive.name
        val record = freshRecord(ownerFilename, archive) ?: return null
        val mission = missionSet.mission
        val hogFiles =
            missionSet.constituents
                .filter { GameFileFormats.extensionOf(it.name) == "hog" }
                .map { stagedRelativePath(scan, it.path) }
        if (hogFiles.isEmpty()) return null
        val sourceLayout = missionFileSourceLayout(record.rootDir, hogFiles + stagedRelativePath(scan, mission.path))
        return LevelMetadataTarget(
            displayName = mission.displayName,
            game =
                mission.game.takeIf {
                    it == GameFileFormats.GAME_D1 || it == GameFileFormats.GAME_D2
                } ?: return null,
            sourceType = "mission_files",
            sourcePath = sourceLayout.root.absolutePath,
            dataDir = setDir.absolutePath,
            missionName =
                mission.path
                    .substringAfterLast('/')
                    .substringBeforeLast('.'),
            missionDisplayName = mission.displayName,
            missionFilename = sourceLayout.relativeToRoot(stagedRelativePath(scan, mission.path)),
            missionPath = mission.path,
            missionType = mission.type,
            hogFiles = hogFiles.map(sourceLayout::relativeToRoot),
            normalLevelFiles = mission.levelNames,
            secretLevelFiles = mission.secretLevelNames,
        )
    }

    private fun extractedRoot(ownerFilename: String): File = File(rootDir, safeMissionZipDirName(ownerFilename))

    private fun upsert(record: MissionZipExtractionRecord) {
        saveRecords(records().filterNot { it.ownerFilename == record.ownerFilename } + record)
    }

    private fun records(): List<MissionZipExtractionRecord> {
        if (!manifestFile.isFile) return emptyList()
        return try {
            val root = JSONObject(manifestFile.readText(Charsets.UTF_8))
            if (root.optString("schema") != MANIFEST_SCHEMA) return emptyList()
            val entries = root.optJSONArray("entries") ?: JSONArray()
            buildList {
                for (index in 0 until entries.length()) {
                    val obj = entries.optJSONObject(index) ?: continue
                    val owner = obj.optString("owner_filename").takeIf { it.isNotBlank() } ?: continue
                    val files = obj.optJSONArray("files") ?: JSONArray()
                    if (!obj.has("owner_size_bytes") ||
                        !obj.has("owner_last_modified_ms") ||
                        !obj.has("owner_sha256") ||
                        !obj.has("extracted_size_bytes") ||
                        !obj.has("file_count") ||
                        obj.optInt("file_count", -1) != files.length()
                    ) {
                        continue
                    }
                    val extractedFiles =
                        buildList {
                            for (fileIndex in 0 until files.length()) {
                                val file = files.optJSONObject(fileIndex) ?: continue
                                val relativePath = file.optString("relative_path")
                                if (relativePath.isBlank()) continue
                                add(
                                    MissionZipExtractedFile(
                                        entryPath = file.optString("entry_path"),
                                        relativePath = relativePath,
                                        sizeBytes = file.optLong("size_bytes"),
                                        contentSha256 = file.optString("sha256"),
                                    ),
                                )
                            }
                        }
                    val record =
                        MissionZipExtractionRecord(
                            ownerFilename = owner,
                            ownerSizeBytes = obj.optLong("owner_size_bytes"),
                            ownerLastModifiedMs = obj.optLong("owner_last_modified_ms"),
                            ownerSha256 = obj.optString("owner_sha256"),
                            rootDir = File(obj.optString("root_path").ifBlank { extractedRoot(owner).absolutePath }),
                            files = extractedFiles,
                            archiveFormat = obj.optString("archive_format", "zip"),
                            importMode = obj.optString("import_mode", "extracted_bundle"),
                            sourceArchiveName = obj.optString("source_archive_name", owner),
                            extractedSizeBytes =
                                obj.optLong("extracted_size_bytes", extractedFiles.sumOf { it.sizeBytes }),
                            fileCount = obj.optInt("file_count", extractedFiles.size),
                        )
                    if (record.hasValidContentIdentity()) add(record)
                }
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    private fun saveRecords(records: List<MissionZipExtractionRecord>) {
        rootDir.mkdirs()
        val entries = JSONArray()
        records.sortedBy { it.ownerFilename.lowercase(Locale.US) }.forEach { record ->
            entries.put(
                JSONObject()
                    .put("owner_filename", record.ownerFilename)
                    .put("owner_size_bytes", record.ownerSizeBytes)
                    .put("owner_last_modified_ms", record.ownerLastModifiedMs)
                    .put("owner_sha256", record.ownerSha256)
                    .put("root_path", record.rootDir.absolutePath)
                    .put("archive_format", record.archiveFormat)
                    .put("import_mode", record.importMode)
                    .put("source_archive_name", record.sourceArchiveName)
                    .put("extracted_size_bytes", record.extractedSizeBytes)
                    .put("file_count", record.fileCount)
                    .put(
                        "files",
                        JSONArray().also { files ->
                            record.files.sortedBy { it.relativePath.lowercase(Locale.US) }.forEach { file ->
                                files.put(
                                    JSONObject()
                                        .put("entry_path", file.entryPath)
                                        .put("relative_path", file.relativePath)
                                        .put("size_bytes", file.sizeBytes)
                                        .put("sha256", file.contentSha256),
                                )
                            }
                        },
                    ),
            )
        }
        manifestFile.writeText(JSONObject().put("schema", MANIFEST_SCHEMA).put("entries", entries).toString(2))
    }
}

private fun missionZipFileSha256(file: File): String {
    val digest = MessageDigest.getInstance("SHA-256")
    file.inputStream().buffered().use { input ->
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
        }
    }
    return digest.digest().joinToString("") { "%02x".format(Locale.US, it) }
}

private fun MissionZipExtractionRecord.hasValidContentIdentity(): Boolean {
    if (!ownerSha256.isSha256() || ownerSizeBytes < 0L || files.isEmpty() || fileCount != files.size) return false
    if (files.any { it.sizeBytes < 0L || !it.contentSha256.isSha256() }) return false
    if (files.map { it.relativePath.lowercase(Locale.US) }.toSet().size != files.size) return false
    val total = runCatching { files.fold(0L) { sum, file -> Math.addExact(sum, file.sizeBytes) } }.getOrNull()
    return total == extractedSizeBytes
}

private fun String.isSha256(): Boolean = length == 64 && all { it in '0'..'9' || it in 'a'..'f' }

internal fun missionZipExtractedStoreForArchivePath(archivePath: String): MissionZipExtractionStore? {
    val modsDir = File(archivePath).parentFile ?: return null
    val parent = modsDir.parentFile ?: return null
    val supportDir =
        if (modsDir.name == "mods" && parent.name == ".content") {
            File(parent, "mod_support")
        } else {
            parent
        }
    return MissionZipExtractionStore(supportDir)
}

internal fun missionZipLaunchStageBytes(entries: List<ArchiveFileEntry>): Long =
    ImportStorageGuard.archiveEntryBytes(entries.filterNot { it.isDirectory }.map { it.sizeBytes })

private fun isMissionSetConstituent(
    scan: MissionZip.ScanResult,
    path: String,
): Boolean =
    scan.missionSets.any { missionSet ->
        missionSet.constituents.any { it.path.equals(path, ignoreCase = true) }
    }

private fun isLaunchMissionSetConstituent(
    scan: MissionZip.ScanResult,
    path: String,
): Boolean =
    scan.effectiveMissionSets.any { missionSet ->
        missionSet.constituents.any { it.path.equals(path, ignoreCase = true) }
    }

internal fun stagedRelativePath(
    scan: MissionZip.ScanResult,
    path: String,
): String {
    val normalized = path.replace('\\', '/').trim('/')
    if (normalized.startsWith("$MISSION_ZIP_GENERATED_MISSION_DIR/", ignoreCase = true)) return normalized
    if (isLaunchMissionSetConstituent(scan, normalized)) return "$MISSION_ZIP_GENERATED_MISSION_DIR/$normalized"
    return normalized
}

internal fun safeMissionZipDirName(filename: String): String = filename.replace(Regex("[^a-zA-Z0-9._-]"), "_")

private data class MissionFileSourceLayout(
    val root: File,
    val prefix: String,
) {
    fun relativeToRoot(path: String): String = if (prefix.isEmpty()) path else path.removePrefix(prefix)
}

private fun missionFileSourceLayout(
    extractedRoot: File,
    relativePaths: List<String>,
): MissionFileSourceLayout {
    val dirs =
        relativePaths
            .map { it.replace('\\', '/').trim('/').substringBeforeLast('/', "") }
            .filter { it.isNotBlank() }
            .distinctBy { it.lowercase(Locale.US) }
    if (dirs.size != 1) return MissionFileSourceLayout(extractedRoot, "")
    val prefix = "${dirs.single().trim('/')}/"
    val root = File(extractedRoot, prefix.replace('/', File.separatorChar))
    return if (root.isDirectory) MissionFileSourceLayout(root, prefix) else MissionFileSourceLayout(extractedRoot, "")
}

private data class MissionZipPlannedEntry(
    val archiveEntry: ArchiveFileEntry,
    val entryPath: String,
    val relativePath: String,
)

private data class MissionZipExtractionPlan(
    val files: List<MissionZipPlannedEntry>,
    val generatedSongListSource: MissionZipPlannedEntry?,
)

private fun isGeneratedD2xxlCachePath(path: String): Boolean =
    path.substringBefore('/').equals("cache", ignoreCase = true)

private fun isD2xxlMissionModSongList(
    scan: MissionZip.ScanResult,
    path: String,
): Boolean {
    val parts = path.split('/')
    if (parts.size != 3 ||
        !parts[0].equals("mods", ignoreCase = true) ||
        !parts[2].equals("descent.sng", ignoreCase = true)
    ) {
        return false
    }
    return scan.missionSets.any { missionSet ->
        missionSet.mission.path
            .substringAfterLast('/')
            .substringBeforeLast('.')
            .equals(parts[1], ignoreCase = true)
    }
}

private fun missionZipExtractionPlan(
    entries: List<ArchiveFileEntry>,
    scan: MissionZip.ScanResult,
): MissionZipExtractionPlan {
    val candidates =
        entries.mapNotNull { entry ->
            val entryPath = normalizeArchivePath(entry.path)
            if (entryPath.isBlank()) return@mapNotNull null
            if (isGeneratedD2xxlCachePath(entryPath) && !isMissionSetConstituent(scan, entryPath)) {
                return@mapNotNull null
            }
            MissionZipPlannedEntry(entry, entryPath, stagedRelativePath(scan, entryPath))
        }
    val songLists =
        candidates.filter { candidate ->
            !candidate.archiveEntry.isDirectory &&
                launcherExtensionOf(candidate.entryPath) == "sng" &&
                (
                    (
                        '/' !in candidate.entryPath &&
                            candidate.entryPath.lowercase(Locale.US) !in MISSION_ZIP_SONG_LIST_FILES
                    ) || isD2xxlMissionModSongList(scan, candidate.entryPath)
                )
        }
    val engineSongListExists =
        candidates.any { candidate ->
            !candidate.archiveEntry.isDirectory &&
                '/' !in candidate.entryPath &&
                candidate.entryPath.lowercase(Locale.US) in MISSION_ZIP_SONG_LIST_FILES
        }
    val generatedSongListSource = songLists.singleOrNull().takeIf { !engineSongListExists }
    val generatedRelativePath = "descent.sng"
    val projections =
        candidates.map { candidate ->
            ArchiveOutputProjection(
                sourcePath = candidate.entryPath,
                relativePath = candidate.relativePath,
                isDirectory = candidate.archiveEntry.isDirectory,
            )
        } +
            listOfNotNull(
                generatedSongListSource?.let {
                    ArchiveOutputProjection("<generated descent.sng>", generatedRelativePath, false)
                },
            )
    val validated = validateArchiveOutputProjections(projections, "mission archive")
    return MissionZipExtractionPlan(
        files =
            candidates
                .zip(validated.take(candidates.size))
                .filterNot { it.first.archiveEntry.isDirectory }
                .map { (candidate, output) -> candidate.copy(relativePath = output.relativePath) },
        generatedSongListSource =
            generatedSongListSource?.let { source ->
                val index = candidates.indexOf(source)
                source.copy(relativePath = validated[index].relativePath)
            },
    )
}

internal fun extractZipToRoot(
    modFile: File,
    targetRoot: File,
    scan: MissionZip.ScanResult,
    onProgress: (Long, Long, String) -> Unit = { _, _, _ -> },
): List<MissionZipExtractedFile> {
    val stageRoot = targetRoot.canonicalFile
    val extractedFiles = mutableListOf<MissionZipExtractedFile>()
    var totalBytes = 0L
    var extractedBytes = 0L
    var lastReportedBytes = -1024L * 1024L
    var lastSpaceCheckBytes = 0L
    val extractionBudget = ExtractionBudget()

    fun report(
        path: String,
        force: Boolean = false,
    ) {
        if (force || extractedBytes - lastReportedBytes >= 1024L * 1024L || extractedBytes >= totalBytes) {
            lastReportedBytes = extractedBytes
            onProgress(extractedBytes.coerceAtMost(totalBytes), totalBytes, path)
        }
    }
    ArchiveFiles.open(modFile).use { archive ->
        val plan = missionZipExtractionPlan(archive.entries, scan)
        if (stageRoot.exists()) stageRoot.deleteRecursively()
        stageRoot.mkdirs()
        plan.files.forEach { planned ->
            val entry = planned.archiveEntry
            extractionBudget.registerEntry(
                entry.sizeBytes,
                entry.compressedSizeBytes ?: 0,
                entry.path,
            )
        }
        totalBytes = missionZipLaunchStageBytes(plan.files.map { it.archiveEntry })
        report("", force = true)
        ImportStorageGuard.requireFreeSpace(
            stageRoot,
            totalBytes,
            "extract ${modFile.name}",
        )
        for (planned in plan.files) {
            val entry = planned.archiveEntry
            val normalized = planned.entryPath
            val relativePath = planned.relativePath
            val output =
                File(
                    stageRoot,
                    relativePath.replace('/', File.separatorChar),
                ).canonicalFile
            if (!output.path.startsWith(stageRoot.path + File.separator)) continue
            output.parentFile?.mkdirs()
            archive.openInputStream(entry).use { input ->
                FileOutputStream(output).use { outputStream ->
                    input.copyToBounded(
                        output = outputStream,
                        budget = extractionBudget,
                        compressedSize = entry.compressedSizeBytes ?: -1,
                        label = normalized,
                    ) { count ->
                        extractedBytes += count.toLong()
                        if (extractedBytes - lastSpaceCheckBytes >= 8L * 1024L * 1024L) {
                            ImportStorageGuard.requireFreeSpace(stageRoot, 0, "extract ${modFile.name}")
                            lastSpaceCheckBytes = extractedBytes
                        }
                        report(normalized)
                    }
                }
            }
            report(normalized, force = true)
            extractedFiles +=
                MissionZipExtractedFile(
                    entryPath = normalized,
                    relativePath = relativePath,
                    sizeBytes = output.length(),
                )
        }
        plan.generatedSongListSource?.let { generatedSource ->
            val source = File(stageRoot, generatedSource.relativePath.replace('/', File.separatorChar))
            val relativePath = "descent.sng"
            val target = File(stageRoot, relativePath.replace('/', File.separatorChar))
            ImportStorageGuard.requireFreeSpace(target.parentFile ?: target, source.length(), "extract descent.sng")
            target.parentFile?.mkdirs()
            extractionBudget.registerEntry(source.length(), source.length(), relativePath)
            source.inputStream().use { input ->
                FileOutputStream(target).use { output ->
                    input.copyToBounded(output, extractionBudget, source.length(), relativePath)
                }
            }
            extractedFiles +=
                MissionZipExtractedFile(
                    entryPath = "",
                    relativePath = relativePath,
                    sizeBytes = target.length(),
                )
        }
    }
    onProgress(totalBytes, totalBytes, "")
    return extractedFiles
}
