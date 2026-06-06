package com.dxxredux.app

import java.io.File
import java.io.InputStream
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
        val game: String,
        val category: String = CATEGORY_LEVELS,
        val totalSizeBytes: Long,
        val importMode: String,
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
            return buildResult(constituents, missions, file.length())
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
        return buildResult(constituents, missions, constituents.sumOf { it.sizeBytes })
    }

    fun parseMissionDescriptor(
        path: String,
        text: String,
    ): GameFileFormats.MissionDescriptor = GameFileFormats.parseMissionDescriptor(path, text)

    private fun buildResult(
        constituents: List<Constituent>,
        missions: List<GameFileFormats.MissionDescriptor>,
        totalSizeBytes: Long,
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
        return ScanResult(
            constituents = constituents.sortedBy { it.path.lowercase(Locale.US) },
            mission = mission,
            game = game,
            totalSizeBytes = totalSizeBytes,
            importMode = if (totalSizeBytes <= SMALL_IN_MEMORY_LIMIT_BYTES) "stored_zip" else "extracted_bundle",
        )
    }

    private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

    private fun normalizePath(path: String): String = path.replace('\\', '/').trim('/')
}
