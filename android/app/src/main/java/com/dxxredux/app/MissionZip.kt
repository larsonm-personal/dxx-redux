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
            val budget = ExtractionBudget()
            val metadataBudget = descriptorBudget()
            val constituents = mutableListOf<Constituent>()
            val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
            for (entry in archive.entries) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.sizeBytes,
                    if (entry.isDirectory) 0 else entry.compressedSizeBytes,
                    entry.path,
                )
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
                    val text =
                        archive.openInputStream(entry).use {
                            it
                                .readBytesBounded(
                                    ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                    entry.path,
                                    metadataBudget,
                                    entry.compressedSizeBytes,
                                ).toString(Charsets.UTF_8)
                        }
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
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
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
                        missions +=
                            parseMissionDescriptor(
                                entry.name,
                                zip
                                    .readBytesBounded(
                                        ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                        entry.name,
                                        metadataBudget,
                                        entry.compressedSize,
                                    ).toString(Charsets.UTF_8),
                            )
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
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        for (file in record.files) {
            val entryPath = normalizePath(file.entryPath).takeIf { it.isNotBlank() } ?: continue
            val name = leafName(entryPath)
            val role = GameFileFormats.missionZipRoleForFile(name)
            budget.registerEntry(file.sizeBytes, file.sizeBytes, entryPath)
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
                    missions +=
                        diskFile.inputStream().use {
                            parseMissionDescriptor(
                                entryPath,
                                it
                                    .readBytesBounded(
                                        ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                        entryPath,
                                        metadataBudget,
                                        file.sizeBytes,
                                    ).toString(Charsets.UTF_8),
                            )
                        }
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
        val budget = ExtractionBudget()
        val metadataBudget = descriptorBudget()
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
        var hasRebirthChildZip = false
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                budget.registerEntry(
                    if (entry.isDirectory) 0 else entry.size,
                    if (entry.isDirectory) 0 else entry.compressedSize,
                    entry.name,
                )
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = GameFileFormats.missionZipRoleForFile(name)
                    constituents +=
                        Constituent(
                            path = normalizePath(entry.name),
                            name = name,
                            role = role,
                            sizeBytes = entry.size.coerceAtLeast(0),
                            compressedSizeBytes = entry.compressedSize.coerceAtLeast(0),
                        )
                    if (role == GameFileFormats.MISSION_ZIP_DESCRIPTOR) {
                        missions +=
                            parseMissionDescriptor(
                                entry.name,
                                zip
                                    .readBytesBounded(
                                        ExtractionLimits.MAX_DESCRIPTOR_BYTES,
                                        entry.name,
                                        metadataBudget,
                                        entry.compressedSize,
                                    ).toString(Charsets.UTF_8),
                            )
                    }
                    if (GameFileFormats.extensionOf(name) == "zip" &&
                        name.lowercase(Locale.US).contains("rebirth")
                    ) {
                        hasRebirthChildZip = true
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return hasRebirthChildZip || playableMissionSets(sortedConstituents(constituents), missions).isNotEmpty()
    }

    private fun descriptorBudget() =
        ExtractionBudget(
            maxEntryBytes = ExtractionLimits.MAX_DESCRIPTOR_BYTES,
            maxTotalBytes = ExtractionLimits.MAX_METADATA_BYTES,
        )

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
        val sortedConstituents = sortedConstituents(constituents)
        val missionSets = playableMissionSets(sortedConstituents, missions)
        val mission = missionSets.firstOrNull()?.mission ?: return null
        val game =
            missionSets
                .map { it.mission }
                .map { it.game }
                .distinct()
                .singleOrNull()
                ?: "both"
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

    private fun playableMissionSets(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
    ): List<MissionSet> =
        missions
            .filter { it.valid }
            .sortedWith(
                compareBy<GameFileFormats.MissionDescriptor> { it.path.lowercase(Locale.US) }
                    .thenBy { it.path },
            ).mapNotNull { mission ->
                val stem = leafName(mission.path).substringBeforeLast('.').lowercase(Locale.US)
                val missionDir = parentPath(mission.path)
                val levelNames =
                    (mission.levelNames + mission.secretLevelNames)
                        .map { leafName(it).lowercase(Locale.US) }
                        .toSet()
                val inMissionDirectory =
                    constituents.filter { constituent ->
                        constituent.path.equals(mission.path, ignoreCase = true) ||
                            (
                                parentPath(constituent.path).equals(missionDir, ignoreCase = true) &&
                                    isMissionPayload(constituent, stem, levelNames)
                            )
                    }
                val hasArchive = inMissionDirectory.any(::isMissionArchive)
                val directLevels =
                    inMissionDirectory
                        .filter { it.name.lowercase(Locale.US) in levelNames }
                        .map { it.name.lowercase(Locale.US) }
                        .toSet()
                if (!hasArchive && !directLevels.containsAll(levelNames)) return@mapNotNull null
                MissionSet(mission, inMissionDirectory)
            }

    private fun isMissionPayload(
        constituent: Constituent,
        missionStem: String,
        levelNames: Set<String>,
    ): Boolean =
        constituent.name.lowercase(Locale.US) in levelNames ||
            (
                constituent.name.substringBeforeLast('.').lowercase(Locale.US) == missionStem &&
                    isMissionArchive(constituent)
            )

    private fun isMissionArchive(constituent: Constituent): Boolean =
        constituent.role in
            setOf(
                GameFileFormats.MISSION_ZIP_HOG,
                GameFileFormats.MISSION_ZIP_MOD_ARCHIVE,
            )

    private fun isMissionHog(constituent: Constituent): Boolean = constituent.role == GameFileFormats.MISSION_ZIP_HOG

    private fun parentPath(path: String): String = normalizePath(path).substringBeforeLast('/', "")

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
