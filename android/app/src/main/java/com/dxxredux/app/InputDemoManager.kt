package com.dxxredux.app

import java.io.File
import java.util.Locale

data class StagedInputDemo(
    val file: File,
    val game: String,
    val mission: String,
    val level: Int,
    val frameCount: Int,
    val headerReadable: Boolean,
)

internal object InputDemoManager {
    const val INPUT_DEMO_EXTENSION = ".dximdemo"

    private const val INPUT_DEMO_STAGING_RELATIVE_DIR = "input_demo_recordings/new"
    private const val FALLBACK_INSTALL_NAME = "input_demo"
    private val gamePrefDirs = linkedMapOf("d1" to "d1x-redux", "d2" to "d2x-redux")
    private val headerTypePattern = Regex("\"type\"\\s*:\\s*\"([^\"]+)\"")

    fun listStagedDemos(filesDir: File): List<StagedInputDemo> {
        val demos = mutableListOf<StagedInputDemo>()

        for ((game, prefDir) in gamePrefDirs) {
            val stagingDir = File(File(filesDir, prefDir), INPUT_DEMO_STAGING_RELATIVE_DIR)
            val files = stagingDir.listFiles() ?: emptyArray()
            for (file in files) {
                if (!file.isFile || !file.name.lowercase(Locale.US).endsWith(INPUT_DEMO_EXTENSION)) continue
                demos += parseStagedDemo(file, game)
            }
        }

        return demos.sortedWith(
            compareByDescending<StagedInputDemo> { it.file.lastModified() }
                .thenBy { it.file.name.lowercase(Locale.US) },
        )
    }

    fun sanitizeInstallName(name: String): String {
        val withoutExtension =
            name
                .trim()
                .removeSuffix(INPUT_DEMO_EXTENSION)
                .replace(Regex("[^A-Za-z0-9._-]+"), "_")
                .trim('_', '.')

        return if (withoutExtension.isEmpty()) FALLBACK_INSTALL_NAME else withoutExtension
    }

    fun installToActiveSet(
        demo: StagedInputDemo,
        fileSetManager: FileSetManager,
        requestedName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): File = installToSet(demo, fileSetManager.getSetDir(fileSetManager.getActive()), requestedName, onProgress)

    fun installToSet(
        demo: StagedInputDemo,
        setDir: File,
        requestedName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): File {
        val demosDir = File(setDir, "demos").also { it.mkdirs() }
        val safeName = sanitizeInstallName(requestedName)
        val destFile = File(demosDir, safeName + INPUT_DEMO_EXTENSION)

        LauncherFileCopy.copyFileToFile(demo.file, destFile, demo.file.name, onProgress)
        demo.file.delete()
        return destFile
    }

    fun deleteStagedDemo(demo: StagedInputDemo): Boolean = demo.file.delete()

    fun deleteAllStagedDemos(filesDir: File): Int {
        var deleted = 0

        for (demo in listStagedDemos(filesDir)) {
            if (demo.file.delete()) deleted++
        }
        return deleted
    }

    private fun parseStagedDemo(
        file: File,
        fallbackGame: String,
    ): StagedInputDemo {
        val header = readHeader(file, fallbackGame)

        return if (header != null) {
            StagedInputDemo(
                file = file,
                game = header.game,
                mission = header.mission,
                level = header.level,
                frameCount = header.frameCount,
                headerReadable = true,
            )
        } else {
            StagedInputDemo(
                file = file,
                game = fallbackGame,
                mission = file.nameWithoutExtension,
                level = 0,
                frameCount = 0,
                headerReadable = false,
            )
        }
    }

    private fun readHeader(
        file: File,
        fallbackGame: String,
    ): ParsedHeader? {
        return try {
            val firstLine = file.useLines { lines -> lines.firstOrNull { it.isNotBlank() } } ?: return null
            if (extractQuotedField(firstLine, headerTypePattern) != "header") return null

            val parsedGame =
                extractQuotedField(firstLine, "game")
                    .takeIf { it == "d1" || it == "d2" }
                    ?: fallbackGame
            val parsedMission = extractQuotedField(firstLine, "mission").ifBlank { file.nameWithoutExtension }

            ParsedHeader(
                game = parsedGame,
                mission = parsedMission,
                level = extractIntField(firstLine, "level") ?: 0,
                frameCount = extractIntField(firstLine, "frame_count") ?: 0,
            )
        } catch (_: Exception) {
            null
        }
    }

    private fun extractQuotedField(
        line: String,
        fieldName: String,
    ): String {
        val pattern = Regex("\"${Regex.escape(fieldName)}\"\\s*:\\s*\"([^\"]*)\"")
        return extractQuotedField(line, pattern).orEmpty()
    }

    private fun extractQuotedField(
        line: String,
        pattern: Regex,
    ): String? = pattern.find(line)?.groupValues?.getOrNull(1)

    private fun extractIntField(
        line: String,
        fieldName: String,
    ): Int? {
        val pattern = Regex("\"${Regex.escape(fieldName)}\"\\s*:\\s*(-?\\d+)")
        return pattern
            .find(line)
            ?.groupValues
            ?.getOrNull(1)
            ?.toIntOrNull()
    }

    private data class ParsedHeader(
        val game: String,
        val mission: String,
        val level: Int,
        val frameCount: Int,
    )
}
