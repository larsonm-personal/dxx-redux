package com.dxxredux.app

import java.io.File
import java.io.InputStream
import java.nio.charset.Charset
import java.util.Locale
import java.util.zip.ZipFile

object MissionZip {
    const val KIND = "mission_zip"
    const val CATEGORY_LEVELS = "levels"
    const val SMALL_IN_MEMORY_LIMIT_BYTES = 100L * 1024L * 1024L

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
        ZipFile(file).use { zip ->
            val constituents = mutableListOf<Constituent>()
            val missions = mutableListOf<GameFileFormats.MissionDescriptor>()
            val entries = zip.entries()
            while (entries.hasMoreElements()) {
                val entry = entries.nextElement()
                if (entry.isDirectory) continue
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
                    val text = zip.getInputStream(entry).bufferedReader().use { it.readText() }
                    missions += parseMissionDescriptor(entry.name, text)
                }
            }
            return buildResult(constituents, missions, file.length(), file.name.substringBeforeLast('.'))
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
        return buildResult(constituents, missions, constituents.sumOf { it.sizeBytes }, zipStem = null)
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
            ZipFile(file).use { zip ->
                val entry = zip.getEntry(path) ?: return TextFileContent("", false, "Text file is missing")
                if (!isTextFile(entry.name)) return TextFileContent("", false, "Only .txt files can be viewed")
                val limit = maxBytes.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
                val bytes =
                    zip.getInputStream(entry).use { input ->
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

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): GameFileFormats.MissionDescriptor = GameFileFormats.parseMissionDescriptor(path, text)

    private fun buildResult(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
        totalSizeBytes: Long,
        zipStem: String?,
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
            importMode = if (totalSizeBytes <= SMALL_IN_MEMORY_LIMIT_BYTES) "stored_zip" else "extracted_bundle",
            readme = chooseReadme(sortedConstituents, zipStem),
        )
    }

    private fun missionSets(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
    ): List<MissionSet> =
        missions.map { mission ->
            val stem = leafName(mission.path).substringBeforeLast('.').lowercase(Locale.US)
            val related =
                constituents.filter { constituent ->
                    constituent.path.equals(mission.path, ignoreCase = true) ||
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
            compareBy<Constituent> { if (isTextFile(it.name)) 0 else 1 }
                .thenBy { it.path.lowercase(Locale.US) },
        )

    private fun chooseReadme(
        constituents: List<Constituent>,
        zipStem: String?,
    ): Constituent? {
        val textFiles = constituents.filter { isTextFile(it.name) }
        if (textFiles.size <= 1) return textFiles.firstOrNull()
        textFiles.firstOrNull { it.name.equals("README.txt", ignoreCase = true) }?.let { return it }
        val zipPrefix = zipStem?.takeIf { it.isNotBlank() }?.lowercase(Locale.US)
        if (zipPrefix != null) {
            textFiles.firstOrNull { it.name.lowercase(Locale.US).startsWith(zipPrefix) }?.let { return it }
        }
        return textFiles.maxByOrNull { it.sizeBytes }
    }

    private fun isTextFile(path: String): Boolean = GameFileFormats.extensionOf(path) == "txt"

    private fun decodeText(bytes: ByteArray): String {
        val utf8 = bytes.toString(Charsets.UTF_8)
        return if ('\uFFFD' in utf8) bytes.toString(Charset.forName("windows-1252")) else utf8
    }

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}
