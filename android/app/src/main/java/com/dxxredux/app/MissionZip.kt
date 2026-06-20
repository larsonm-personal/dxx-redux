package com.dxxredux.app

import java.io.File
import java.io.InputStream
import java.nio.charset.Charset
import java.util.Locale

object MissionZip {
    const val KIND = "mission_zip"
    const val CATEGORY_LEVELS = "levels"
    const val DURABLE_EXTRACT_THRESHOLD_BYTES = 10L * 1024L * 1024L
    const val SMALL_IN_MEMORY_LIMIT_BYTES = 100L * 1024L * 1024L
    const val SMALL_NESTED_ARCHIVE_LIMIT_BYTES = 32L * 1024L * 1024L
    private val INLINE_README_EXTENSIONS = setOf("txt")
    private val EXTERNAL_README_EXTENSIONS = setOf("pdf", "rtf", "doc", "docx")

    data class Constituent(
        val path: String,
        val name: String,
        val role: String,
        val sizeBytes: Long,
        val compressedSizeBytes: Long,
    )

    data class ScanResult(
        val constituents: List<Constituent>,
        val mission: GameFileFormats.MissionDescriptor,
        val missionSets: List<MissionSet>,
        val game: String,
        val category: String = CATEGORY_LEVELS,
        val totalSizeBytes: Long,
        val importMode: String,
        val readme: Constituent? = null,
        val archiveFormat: String = "zip",
    )

    data class MissionSet(
        val mission: GameFileFormats.MissionDescriptor,
        val constituents: List<Constituent>,
    )

    data class TextFileContent(
        val text: String,
        val truncated: Boolean,
        val problem: String? = null,
    )

    fun inspect(file: File): ScanResult? {
        if (!file.isFile) return null
        ArchiveFiles.open(file).use { archive ->
            val constituents = mutableListOf<Constituent>()
            val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
            for (entry in archive.entries) {
                if (entry.isDirectory) continue
                val name = leafName(entry.path)
                val role = GameFileFormats.missionZipRoleForFile(name)
                constituents +=
                    Constituent(
                        path = normalizePath(entry.path),
                        name = name,
                        role = role,
                        sizeBytes = entry.sizeBytes,
                        compressedSizeBytes = entry.compressedSizeBytes,
                    )
                if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                    val text = archive.openInputStream(entry).bufferedReader().use { it.readText() }
                    missions += parseMissionDescriptor(entry.path, text)
                }
            }
            return buildResult(
                constituents,
                missions,
                file.length(),
                file.name.substringBeforeLast('.'),
                archive.format,
            )
        }
    }

    fun inspect(input: InputStream): ScanResult? {
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = GameFileFormats.missionZipRoleForFile(name)
                    val size = entry.size.coerceAtLeast(0)
                    constituents +=
                        Constituent(
                            path = normalizePath(entry.name),
                            name = name,
                            role = role,
                            sizeBytes = size,
                            compressedSizeBytes = entry.compressedSize.coerceAtLeast(0),
                        )
                    if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                        missions += parseMissionDescriptor(entry.name, zip.readBytes().toString(Charsets.UTF_8))
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return buildResult(
            constituents,
            missions,
            constituents.sumOf { it.sizeBytes },
            zipStem = null,
            archiveFormat = "zip",
        )
    }

    internal fun inspectExtracted(record: MissionZipExtractionRecord): ScanResult? {
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        for (file in record.files) {
            val entryPath = normalizePath(file.entryPath).takeIf { it.isNotBlank() } ?: continue
            val name = leafName(entryPath)
            val role = GameFileFormats.missionZipRoleForFile(name)
            constituents +=
                Constituent(
                    path = entryPath,
                    name = name,
                    role = role,
                    sizeBytes = file.sizeBytes,
                    compressedSizeBytes = file.sizeBytes,
                )
            if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                val diskFile = File(record.rootDir, file.relativePath.replace('/', File.separatorChar))
                if (diskFile.isFile) {
                    missions += parseMissionDescriptor(entryPath, diskFile.readText(Charsets.UTF_8))
                }
            }
        }
        return buildResult(
            constituents,
            missions,
            record.ownerSizeBytes,
            record.ownerFilename.substringBeforeLast('.'),
            record.archiveFormat.ifBlank { "zip" },
        )?.copy(importMode = record.importMode.ifBlank { "extracted_bundle" })
    }

    fun isImportCandidate(input: InputStream): Boolean {
        var hasMissionDescriptor = false
        var hasMissionAssets = false
        var hasRebirthChildZip = false
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = GameFileFormats.missionZipRoleForFile(name)
                    if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) hasMissionDescriptor = true
                    if (role == GameFileFormats.MISSION_ZIP_HOG ||
                        role == GameFileFormats.MISSION_ZIP_MOD_ARCHIVE
                    ) {
                        hasMissionAssets = true
                    }
                    if (GameFileFormats.extensionOf(name) == "zip" &&
                        name.lowercase(Locale.US).contains("rebirth")
                    ) {
                        hasRebirthChildZip = true
                    }
                    if (hasRebirthChildZip || (hasMissionDescriptor && hasMissionAssets)) return true
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return (hasMissionDescriptor && hasMissionAssets) || hasRebirthChildZip
    }

    fun readTextFile(
        file: File,
        path: String,
        maxBytes: Long = 1024L * 1024L,
    ): TextFileContent {
        if (!file.isFile) return TextFileContent("", truncated = false, problem = "Mission ZIP is missing")
        return try {
            ArchiveFiles.open(file).use { archive ->
                val entry = archive.findEntry(path) ?: return TextFileContent("", false, "Text file is missing")
                if (!isTextFile(entry.path)) return TextFileContent("", false, "Only .txt files can be viewed")
                val limit = maxBytes.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
                val bytes =
                    archive.openInputStream(entry).use { input ->
                        val buffer = ByteArray(limit + 1)
                        var total = 0
                        while (total < buffer.size) {
                            val read = input.read(buffer, total, buffer.size - total)
                            if (read <= 0) break
                            total += read
                        }
                        buffer.copyOf(total)
                    }
                val truncated = bytes.size > limit
                TextFileContent(decodeText(bytes.copyOf(minOf(bytes.size, limit))), truncated)
            }
        } catch (e: Exception) {
            TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
        }
    }

    fun isInlineReadmeCandidate(path: String): Boolean = GameFileFormats.extensionOf(path) in INLINE_README_EXTENSIONS

    fun isExternalReadmeCandidate(path: String): Boolean =
        GameFileFormats.extensionOf(path) in EXTERNAL_README_EXTENSIONS

    fun isReadmeCandidate(path: String): Boolean = isInlineReadmeCandidate(path) || isExternalReadmeCandidate(path)

    fun externalViewMimeType(path: String): String =
        when (GameFileFormats.extensionOf(path)) {
            "pdf" -> "application/pdf"
            "rtf" -> "application/rtf"
            "doc" -> "application/msword"
            "docx" -> "application/vnd.openxmlformats-officedocument.wordprocessingml.document"
            "txt" -> "text/plain"
            else -> "application/octet-stream"
        }

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): GameFileFormats.MissionDescriptor = GameFileFormats.parseMissionDescriptor(path, text)

    private fun buildResult(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
        totalSizeBytes: Long,
        zipStem: String?,
        archiveFormat: String,
    ): ScanResult? {
        val mission = missions.firstOrNull() ?: return null
        val roles = constituents.map { it.role }.toSet()
        if (GameFileFormats.MISSION_ZIP_HOG !in roles && GameFileFormats.MISSION_ZIP_MOD_ARCHIVE !in roles) return null
        val game =
            missions
                .map { it.game }
                .distinct()
                .singleOrNull()
                ?: "both"
        val sortedConstituents = sortedConstituents(constituents)
        val missionSets = missionSets(sortedConstituents, missions)
        return ScanResult(
            constituents = sortedConstituents,
            mission = mission,
            missionSets = missionSets,
            game = game,
            totalSizeBytes = totalSizeBytes,
            importMode =
                if (shouldStoreArchive(
                        archiveFormat,
                        totalSizeBytes,
                        sortedConstituents,
                    )
                ) {
                    "stored_zip"
                } else {
                    "extracted_bundle"
                },
            readme = chooseReadme(sortedConstituents, zipStem),
            archiveFormat = archiveFormat,
        )
    }

    private fun shouldStoreArchive(
        archiveFormat: String,
        totalSizeBytes: Long,
        constituents: List<Constituent>,
    ): Boolean {
        if (archiveFormat != "zip") return false
        if (totalSizeBytes > DURABLE_EXTRACT_THRESHOLD_BYTES) return false
        return constituents.none {
            it.role in
                setOf(
                    GameFileFormats.MISSION_ZIP_HOG,
                    GameFileFormats.MISSION_ZIP_MOD_ARCHIVE,
                ) &&
                it.sizeBytes > SMALL_NESTED_ARCHIVE_LIMIT_BYTES
        }
    }

    private fun missionSets(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
    ): List<MissionSet> =
        missions.map { mission ->
            val stem = leafName(mission.path).substringBeforeLast('.').lowercase(Locale.US)
            val levelNames =
                (mission.levelNames + mission.secretLevelNames)
                    .map { leafName(it).lowercase(Locale.US) }
                    .toSet()
            val related =
                constituents.filter { constituent ->
                    constituent.path.equals(mission.path, ignoreCase = true) ||
                        constituent.name.lowercase(Locale.US) in levelNames ||
                        (
                            constituent.name.substringBeforeLast('.').lowercase(Locale.US) == stem &&
                                constituent.role in
                                setOf(
                                    GameFileFormats.MISSION_ZIP_HOG,
                                    GameFileFormats.MISSION_ZIP_MOD_ARCHIVE,
                                )
                        )
                }
            MissionSet(mission, related.ifEmpty { constituents })
        }

    private fun sortedConstituents(constituents: List<Constituent>): List<Constituent> =
        constituents.sortedWith(
            compareBy<Constituent> { readmeSortGroup(it.name) }
                .thenBy { it.path.lowercase(Locale.US) },
        )

    private fun chooseReadme(
        constituents: List<Constituent>,
        zipStem: String?,
    ): Constituent? {
        chooseReadmeFromCandidates(constituents.filter { isInlineReadmeCandidate(it.name) }, zipStem)?.let {
            return it
        }
        return chooseReadmeFromCandidates(constituents.filter { isExternalReadmeCandidate(it.name) }, zipStem)
    }

    private fun chooseReadmeFromCandidates(
        candidates: List<Constituent>,
        zipStem: String?,
    ): Constituent? {
        if (candidates.size <= 1) return candidates.firstOrNull()
        candidates.firstOrNull { isReadmeNamed(it.name) }?.let { return it }
        val zipPrefix = zipStem?.takeIf { it.isNotBlank() }?.lowercase(Locale.US)
        if (zipPrefix != null) {
            candidates.firstOrNull { it.name.lowercase(Locale.US).startsWith(zipPrefix) }?.let { return it }
        }
        return candidates.sortedWith(compareByDescending<Constituent> { it.sizeBytes }.thenBy { it.path }).firstOrNull()
    }

    private fun readmeSortGroup(path: String): Int =
        when {
            isInlineReadmeCandidate(path) -> 0
            isExternalReadmeCandidate(path) -> 1
            else -> 2
        }

    private fun isReadmeNamed(path: String): Boolean =
        leafName(path).substringBeforeLast('.').equals("README", ignoreCase = true) && isReadmeCandidate(path)

    private fun isTextFile(path: String): Boolean = isInlineReadmeCandidate(path)

    private fun decodeText(bytes: ByteArray): String {
        val utf8 = bytes.toString(Charsets.UTF_8)
        return if ('\uFFFD' in utf8) bytes.toString(Charset.forName("windows-1252")) else utf8
    }

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}
