package com.dxxredux.app

import java.io.File
import java.util.Locale

data class StagedInputDemo(
    val file: File,
    val traceFile: File?,
    val game: String,
    val mission: String,
    val level: Int,
    val frameCount: Int,
    val durationMillis: Long?,
    val headerReadable: Boolean,
)

internal object InputDemoManager {
    const val INPUT_DEMO_EXTENSION = ".dximdemo"
    // Keep in sync with INPUT_DEMO_RNG_TRACE_SUFFIX in native shared code.
    const val INPUT_DEMO_RNG_TRACE_SUFFIX = ".rngtrace.jsonl"

    private const val FIX_ONE = 65536L
    private const val INPUT_DEMO_STAGING_RELATIVE_DIR = "input_demo_recordings/new"
    private const val FALLBACK_INSTALL_NAME = "input_demo"
    private val gamePrefDirs = linkedMapOf("d1" to "d1x-redux", "d2" to "d2x-redux")
    private val recordTypePattern = Regex("\"type\"\\s*:\\s*\"([^\"]+)\"")

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

    fun exportFiles(demo: StagedInputDemo): List<File> = listOfNotNull(demo.file, demo.traceFile)

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
        demo.traceFile?.let {
            LauncherFileCopy.copyFileToFile(
                it,
                File(demosDir, destFile.name + INPUT_DEMO_RNG_TRACE_SUFFIX),
                it.name,
                onProgress,
            )
        }
        demo.file.delete()
        demo.traceFile?.delete()
        return destFile
    }

    fun deleteStagedDemo(demo: StagedInputDemo): Boolean {
        val deletedDemo = demo.file.delete()
        val deletedTrace = demo.traceFile?.delete() ?: true

        return deletedDemo && deletedTrace
    }

    fun deleteAllStagedDemos(filesDir: File): Int {
        var deleted = 0

        for (demo in listStagedDemos(filesDir)) {
            if (deleteStagedDemo(demo)) deleted++
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
                traceFile = traceFileFor(file),
                game = header.game,
                mission = header.mission,
                level = header.level,
                frameCount = header.frameCount,
                durationMillis = header.durationMillis,
                headerReadable = true,
            )
        } else {
            StagedInputDemo(
                file = file,
                traceFile = traceFileFor(file),
                game = fallbackGame,
                mission = file.nameWithoutExtension,
                level = 0,
                frameCount = 0,
                durationMillis = null,
                headerReadable = false,
            )
        }
    }

    private fun traceFileFor(file: File): File? {
        val trace = File(file.parentFile, file.name + INPUT_DEMO_RNG_TRACE_SUFFIX)
        return trace.takeIf { it.isFile }
    }

    private fun readHeader(
        file: File,
        fallbackGame: String,
    ): ParsedHeader? {
        return try {
            file.bufferedReader().use { reader ->
                val lines = reader.lineSequence().iterator()
                var firstLine: String? = null

                while (lines.hasNext()) {
                    val candidate = lines.next().trim()
                    if (candidate.isNotEmpty()) {
                        firstLine = candidate
                        break
                    }
                }

                val headerLine = firstLine ?: return null
                if (extractQuotedField(headerLine, recordTypePattern) != "header") return null

                val parsedGame =
                    extractQuotedField(headerLine, "game")
                        .takeIf { it == "d1" || it == "d2" }
                        ?: fallbackGame
                val parsedMission = extractQuotedField(headerLine, "mission").ifBlank { file.nameWithoutExtension }
                var durationFixed = 0L
                var previousFrameTime: Int? = null
                var sawFrame = false
                var durationReadable = true

                while (lines.hasNext()) {
                    val line = lines.next().trim()
                    if (line.isEmpty()) continue
                    if (extractQuotedField(line, recordTypePattern) != "frame") continue

                    sawFrame = true
                    val parsedFrameTime = extractIntField(line, "ft")
                    val frameTime =
                        when {
                            parsedFrameTime != null -> {
                                parsedFrameTime
                            }

                            previousFrameTime != null && !line.contains("\"ft\"") -> {
                                previousFrameTime
                            }

                            else -> {
                                durationReadable = false
                                null
                            }
                        }

                    if (frameTime != null) {
                        previousFrameTime = frameTime
                        durationFixed += frameTime.toLong()
                    }
                }

                ParsedHeader(
                    game = parsedGame,
                    mission = parsedMission,
                    level = extractIntField(headerLine, "level") ?: 0,
                    frameCount = extractIntField(headerLine, "frame_count") ?: 0,
                    durationMillis = if (sawFrame && durationReadable) fixedToMillis(durationFixed) else null,
                )
            }
        } catch (_: Exception) {
            null
        }
    }

    private fun fixedToMillis(durationFixed: Long): Long = ((durationFixed * 1000L) + (FIX_ONE / 2L)) / FIX_ONE

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
        val durationMillis: Long?,
    )
}
