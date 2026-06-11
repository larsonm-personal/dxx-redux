package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.util.Locale
import java.util.zip.ZipFile

internal const val MISSION_ZIP_EXTRACTED_DIR = ".extracted_mission_zips"
internal const val MISSION_ZIP_GENERATED_MISSION_DIR = "missions"

private const val MANIFEST_FILE = "manifest.json"
private const val MANIFEST_SCHEMA = "dxx-mission-zip-extractions-v1"
private val MISSION_ZIP_SONG_LIST_FILES = setOf("descent.sng", "dxx-r.sng")

internal data class MissionZipExtractedFile(
    val entryPath: String,
    val relativePath: String,
    val sizeBytes: Long,
)

internal data class MissionZipExtractionRecord(
    val ownerFilename: String,
    val ownerSizeBytes: Long,
    val rootDir: File,
    val files: List<MissionZipExtractedFile>,
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
    ): MissionZipExtractionRecord {
        freshRecord(ownerFilename, modFile)?.let { return it }
        val ownerRoot = extractedRoot(ownerFilename)
        val tempRoot = File(rootDir, "${safeMissionZipDirName(ownerFilename)}.tmp")
        tempRoot.deleteRecursively()
        val files =
            try {
                extractZipToRoot(modFile, tempRoot, scan)
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
                rootDir = ownerRoot,
                files = files,
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
        if (modFile != null && (!modFile.isFile || modFile.length() != record.ownerSizeBytes)) return null
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
        scan: MissionZip.ScanResult,
    ): List<String> {
        val record = ensureExtracted(ownerFilename, modFile, scan)
        return buildList {
            add(record.rootDir.absolutePath)
            for (constituent in scan.constituents.filter { it.role == GameFileFormats.MISSION_ZIP_MOD_ARCHIVE }) {
                val archive =
                    File(
                        record.rootDir,
                        stagedRelativePath(scan, constituent.path).replace('/', File.separatorChar),
                    )
                if (archive.isFile) add(archive.absolutePath)
            }
        }
    }

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
        return LevelMetadataTarget(
            displayName = mission.displayName,
            game =
                mission.game.takeIf {
                    it == GameFileFormats.GAME_D1 || it == GameFileFormats.GAME_D2
                } ?: return null,
            sourceType = "mission_files",
            sourcePath = record.rootDir.absolutePath,
            dataDir = setDir.absolutePath,
            missionName =
                mission.path
                    .substringAfterLast('/')
                    .substringBeforeLast('.'),
            missionFilename = mission.path.substringAfterLast('/').substringAfterLast('\\'),
            missionType = mission.type,
            hogFiles = hogFiles,
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
                    add(
                        MissionZipExtractionRecord(
                            ownerFilename = owner,
                            ownerSizeBytes = obj.optLong("owner_size_bytes"),
                            rootDir = File(obj.optString("root_path").ifBlank { extractedRoot(owner).absolutePath }),
                            files =
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
                                },
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
                    .put("root_path", record.rootDir.absolutePath)
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

internal fun missionZipLaunchStageBytes(zip: ZipFile): Long {
    val sizes = mutableListOf<Long>()
    val entries = zip.entries()
    while (entries.hasMoreElements()) {
        val entry = entries.nextElement()
        if (!entry.isDirectory) sizes += entry.size
    }
    return ImportStorageGuard.archiveEntryBytes(sizes)
}

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

internal fun extractZipToRoot(
    modFile: File,
    targetRoot: File,
    scan: MissionZip.ScanResult,
): List<MissionZipExtractedFile> {
    val stageRoot = targetRoot.canonicalFile
    if (stageRoot.exists()) stageRoot.deleteRecursively()
    stageRoot.mkdirs()
    val extractedFiles = mutableListOf<MissionZipExtractedFile>()
    val songLists = mutableListOf<File>()
    var engineSongListExists = false
    ZipFile(modFile).use { zip ->
        ImportStorageGuard.requireFreeSpace(
            stageRoot,
            missionZipLaunchStageBytes(zip),
            "extract ${modFile.name}",
        )
        val entries = zip.entries()
        while (entries.hasMoreElements()) {
            val entry = entries.nextElement()
            if (entry.isDirectory) continue
            val normalized = entry.name.replace('\\', '/').trim('/')
            if (normalized.isBlank()) continue
            val relativePath = stagedRelativePath(scan, normalized)
            val output =
                File(
                    stageRoot,
                    relativePath.replace('/', File.separatorChar),
                ).canonicalFile
            if (!output.path.startsWith(stageRoot.path + File.separator)) continue
            output.parentFile?.mkdirs()
            zip.getInputStream(entry).use { input ->
                FileOutputStream(output).use { outputStream ->
                    input.copyTo(outputStream)
                }
            }
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
        source.copyTo(target, overwrite = true)
        extractedFiles +=
            MissionZipExtractedFile(
                entryPath = "",
                relativePath = relativePath,
                sizeBytes = target.length(),
            )
    }
    return extractedFiles
}
