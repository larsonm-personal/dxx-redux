package com.dxxredux.app

import java.io.File
import java.io.InputStream
import java.util.Locale
import java.util.zip.ZipFile

object SectorgameMissionZip {
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

    data class MissionDescriptor(
        val path: String,
        val name: String?,
        val type: String?,
        val author: String?,
        val editor: String?,
        val levelNames: List<String>,
        val game: String,
    ) {
        val displayName: String get() = name?.takeIf { it.isNotBlank() } ?: path.substringAfterLast('/').substringBeforeLast('.')
    }

    data class ScanResult(
        val constituents: List<Constituent>,
        val mission: MissionDescriptor,
        val game: String,
        val category: String = CATEGORY_LEVELS,
        val totalSizeBytes: Long,
        val importMode: String,
    )

    fun inspect(file: File): ScanResult? {
        if (!file.isFile) return null
        ZipFile(file).use { zip ->
            val constituents = mutableListOf<Constituent>()
            val missions = mutableListOf<MissionDescriptor>()
            val entries = zip.entries()
            while (entries.hasMoreElements()) {
                val entry = entries.nextElement()
                if (entry.isDirectory) continue
                val name = leafName(entry.name)
                val role = roleForName(name)
                constituents +=
                    Constituent(
                        path = normalizePath(entry.name),
                        name = name,
                        role = role,
                        sizeBytes = entry.size.coerceAtLeast(0),
                        compressedSizeBytes = entry.compressedSize.coerceAtLeast(0),
                    )
                if (role == "mission_descriptor") {
                    val text = zip.getInputStream(entry).bufferedReader().use { it.readText() }
                    missions += parseMissionDescriptor(entry.name, text)
                }
            }
            return buildResult(constituents, missions, file.length())
        }
    }

    fun inspect(input: InputStream): ScanResult? {
        val constituents = mutableListOf<Constituent>()
        val missions = mutableListOf<MissionDescriptor>()
        openZipInputStreamSkippingPreamble(input).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val name = leafName(entry.name)
                    val role = roleForName(name)
                    val size = entry.size.coerceAtLeast(0)
                    constituents +=
                        Constituent(
                            path = normalizePath(entry.name),
                            name = name,
                            role = role,
                            sizeBytes = size,
                            compressedSizeBytes = entry.compressedSize.coerceAtLeast(0),
                        )
                    if (role == "mission_descriptor") {
                        missions += parseMissionDescriptor(entry.name, zip.readBytes().toString(Charsets.UTF_8))
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return buildResult(constituents, missions, constituents.sumOf { it.sizeBytes })
    }

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): MissionDescriptor {
        val values = linkedMapOf<String, String>()
        val levels = mutableListOf<String>()
        var remainingLevels = 0
        for (rawLine in text.lineSequence()) {
            val line = rawLine.trim()
            if (line.isBlank() || line.startsWith(";") || line.startsWith("#")) continue
            if (remainingLevels > 0 && '=' !in line) {
                val level = line.substringBefore(',').trim()
                if (level.isNotBlank()) levels += level
                remainingLevels--
                continue
            }
            val eq = line.indexOf('=')
            if (eq < 0) continue
            val key = line.substring(0, eq).trim().lowercase(Locale.US)
            val value = line.substring(eq + 1).trim()
            values[key] = value
            if (key == "num_levels") remainingLevels = value.toIntOrNull()?.coerceAtLeast(0) ?: 0
        }
        return MissionDescriptor(
            path = normalizePath(path),
            name = firstMissionValue(values, "name", "xname", "zname", "!name"),
            type = values["type"],
            author = values["author"],
            editor = values["editor"],
            levelNames = levels,
            game = detectGame(path, levels),
        )
    }

    private fun buildResult(
        constituents: List<Constituent>,
        missions: List<MissionDescriptor>,
        totalSizeBytes: Long,
    ): ScanResult? {
        val mission = missions.firstOrNull() ?: return null
        val roles = constituents.map { it.role }.toSet()
        if ("mission_hog" !in roles && "mod_archive" !in roles) return null
        val game =
            missions
                .map { it.game }
                .distinct()
                .singleOrNull()
                ?: "both"
        return ScanResult(
            constituents = constituents.sortedBy { it.path.lowercase(Locale.US) },
            mission = mission,
            game = game,
            totalSizeBytes = totalSizeBytes,
            importMode = if (totalSizeBytes <= SMALL_IN_MEMORY_LIMIT_BYTES) "stored_zip" else "extracted_bundle",
        )
    }

    private fun firstMissionValue(
        values: Map<String, String>,
        vararg keys: String,
    ): String? = keys.firstNotNullOfOrNull { values[it]?.takeIf { value -> value.isNotBlank() } }

    private fun detectGame(
        path: String,
        levelNames: List<String>,
    ): String {
        val ext = launcherExtensionOf(path)
        if (ext == "mn2") return "d2"
        if (levelNames.any { launcherExtensionOf(it) in setOf("rl2", "sl2") }) return "d2"
        if (ext == "msn") return "d1"
        if (levelNames.any { launcherExtensionOf(it) in setOf("rdl", "sdl") }) return "d1"
        return "both"
    }

    private fun roleForName(name: String): String =
        when (launcherExtensionOf(name)) {
            "mn2", "msn" -> "mission_descriptor"
            "hog" -> "mission_hog"
            "dxa" -> "mod_archive"
            "txt", "md", "rtf" -> "documentation"
            else -> "other"
        }

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}
