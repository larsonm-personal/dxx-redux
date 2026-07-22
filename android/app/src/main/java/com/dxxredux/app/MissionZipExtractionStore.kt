package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.util.Locale

internal const val MISSION_ZIP_EXTRACTED_DIR = ".extracted_mission_zips"
internal const val MISSION_ZIP_GENERATED_MISSION_DIR = "missions"

private const val MANIFEST_FILE = "manifest.json"
private const val MANIFEST_SCHEMA = "dxx-mission-zip-extractions-v2"
private val MISSION_ZIP_SONG_LIST_FILES = setOf("descent.sng", "dxx-r.sng")

internal data class MissionZipExtractedFile(
    val entryPath: String,
    val relativePath: String,
    val sizeBytes: Long,
)

internal data class MissionZipExtractionRecord(
    val ownerFilename: String,
    val ownerSizeBytes: Long,
    val ownerLastModifiedMs: Long,
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
        val ownerRoot = extractedRoot(ownerFilename)
        val tempRoot = File(rootDir, "${safeMissionZipDirName(ownerFilename)}.tmp")
        tempRoot.deleteRecursively()
        val files =
            try {
                extractZipToRoot(modFile, tempRoot, scan, onProgress)
            } catch (e: Exception) {
                tempRoot.deleteRecursively()
                throw e
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
                ownerSizeBytes = modFile.length(),
                ownerLastModifiedMs = modFile.lastModified(),
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
        if (!record.rootDir.isDirectory) return null
        if (modFile != null &&
            (
                !modFile.isFile ||
                    modFile.length() != record.ownerSizeBytes ||
                    modFile.lastModified() != record.ownerLastModifiedMs
            )
        ) {
            return null
        }
        val allFilesPresent =
            record.files.all { file ->
                File(record.rootDir, file.relativePath.replace('/', File.separatorChar)).let {
                    it.isFile && (file.sizeBytes <= 0L || it.length() == file.sizeBytes)
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
            missionFilename = sourceLayout.relativeToRoot(stagedRelativePath(scan, mission.path)),
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
            val entries = root.optJSONArray("entries") ?: JSONArray()
            buildList {
                for (index in 0 until entries.length()) {
                    val obj = entries.optJSONObject(index) ?: continue
                    val owner = obj.optString("owner_filename").takeIf { it.isNotBlank() } ?: continue
                    val files = obj.optJSONArray("files") ?: JSONArray()
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
                                    ),
                                )
                            }
                        }
                    add(
                        MissionZipExtractionRecord(
                            ownerFilename = owner,
                            ownerSizeBytes = obj.optLong("owner_size_bytes"),
                            ownerLastModifiedMs = obj.optLong("owner_last_modified_ms"),
                            rootDir = File(obj.optString("root_path").ifBlank { extractedRoot(owner).absolutePath }),
                            files = extractedFiles,
                            archiveFormat = obj.optString("archive_format", "zip"),
                            importMode = obj.optString("import_mode", "extracted_bundle"),
                            sourceArchiveName = obj.optString("source_archive_name", owner),
                            extractedSizeBytes =
                                obj.optLong("extracted_size_bytes", extractedFiles.sumOf { it.sizeBytes }),
                            fileCount = obj.optInt("file_count", extractedFiles.size),
                        ),
                    )
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
                                        .put("size_bytes", file.sizeBytes),
                                )
                            }
                        },
                    ),
            )
        }
        manifestFile.writeText(JSONObject().put("schema", MANIFEST_SCHEMA).put("entries", entries).toString(2))
    }
}

internal fun missionZipExtractedStoreForArchivePath(archivePath: String): MissionZipExtractionStore? {
    val modsDir = File(archivePath).parentFile ?: return null
    val filesDir = modsDir.parentFile ?: return null
    return MissionZipExtractionStore(filesDir)
}

internal fun missionZipLaunchStageBytes(entries: List<ArchiveFileEntry>): Long =
    ImportStorageGuard.archiveEntryBytes(entries.filterNot { it.isDirectory }.map { it.sizeBytes })

internal fun stagedRelativePath(
    scan: MissionZip.ScanResult,
    path: String,
): String {
    val normalized = path.replace('\\', '/').trim('/')
    return if (missionZipUsesRootedLayout(scan)) {
        normalized
    } else {
        "$MISSION_ZIP_GENERATED_MISSION_DIR/$normalized"
    }
}

internal fun missionZipUsesRootedLayout(scan: MissionZip.ScanResult): Boolean =
    scan.constituents.any {
        val path = it.path.lowercase(Locale.US)
        path.startsWith("$MISSION_ZIP_GENERATED_MISSION_DIR/") &&
            (
                it.role == GameFileFormats.MISSION_ZIP_DESCRIPTOR ||
                    it.role == GameFileFormats.MISSION_ZIP_HOG
            )
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

internal fun extractZipToRoot(
    modFile: File,
    targetRoot: File,
    scan: MissionZip.ScanResult,
    onProgress: (Long, Long, String) -> Unit = { _, _, _ -> },
): List<MissionZipExtractedFile> {
    val stageRoot = targetRoot.canonicalFile
    if (stageRoot.exists()) stageRoot.deleteRecursively()
    stageRoot.mkdirs()
    val extractedFiles = mutableListOf<MissionZipExtractedFile>()
    val songLists = mutableListOf<File>()
    var engineSongListExists = false
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
        archive.entries.forEach { entry ->
            extractionBudget.registerEntry(
                if (entry.isDirectory) 0 else entry.sizeBytes,
                if (entry.isDirectory) 0 else entry.compressedSizeBytes,
                entry.path,
            )
        }
        totalBytes = missionZipLaunchStageBytes(archive.entries)
        report("", force = true)
        ImportStorageGuard.requireFreeSpace(
            stageRoot,
            totalBytes,
            "extract ${modFile.name}",
        )
        for (entry in archive.entries) {
            if (entry.isDirectory) continue
            val normalized = entry.path.replace('\\', '/').trim('/')
            if (normalized.isBlank()) continue
            val relativePath = stagedRelativePath(scan, normalized)
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
                        compressedSize = entry.compressedSizeBytes,
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
            val leaf = normalized.substringAfterLast('/').lowercase(Locale.US)
            if ('/' !in normalized && launcherExtensionOf(leaf) == "sng") {
                if (leaf in MISSION_ZIP_SONG_LIST_FILES) {
                    engineSongListExists = true
                } else {
                    songLists += output
                }
            }
        }
    }
    if (!engineSongListExists && songLists.size == 1) {
        val source = songLists.single()
        val relativePath = "$MISSION_ZIP_GENERATED_MISSION_DIR/descent.sng"
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
    onProgress(totalBytes, totalBytes, "")
    return extractedFiles
}
