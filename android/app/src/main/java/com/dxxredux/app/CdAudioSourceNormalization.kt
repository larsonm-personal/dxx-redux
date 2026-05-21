package com.dxxredux.app

import java.io.File
import java.util.Locale

private const val CD_SECTOR_BYTES = 2352L
internal const val GENERATED_MERGED_CUE_MARKER = "REM DXX-REDUX GENERATED MERGED LOCAL SOURCE"
internal const val GENERATED_CD_AUDIO_ARTIFACT_DIR = "cd_audio"
private const val FALLBACK_CD_AUDIO_SOURCE_STEM = "cd_audio_source"

internal fun isLocalCdContentPath(path: String): Boolean =
    path.startsWith("/") || Regex("^[A-Za-z]:[\\\\/]").containsMatchIn(path)

internal fun generatedCdAudioArtifactsDir(filesDir: File): File =
    File(ImportLocationManager(filesDir).getActiveRoot(), GENERATED_CD_AUDIO_ARTIFACT_DIR).also { it.mkdirs() }

internal fun resolveCdAudioSourceFile(
    filesDir: File,
    path: String,
): File {
    val direct = File(path)
    return if (direct.isAbsolute || isLocalCdContentPath(path)) direct.absoluteFile else File(filesDir, path)
}

internal fun hasSafLinkedCdContent(source: AudioSourceManager.AudioSource): Boolean =
    source.binContentUri?.let { !isLocalCdContentPath(it) } == true ||
        source.cueContentUri?.let { !isLocalCdContentPath(it) } == true

internal fun sanitizeCdAudioImportStem(name: String): String {
    val safeName =
        name
            .trim()
            .replace(Regex("[^A-Za-z0-9._-]+"), "_")
            .trim('_', '.')
            .lowercase(Locale.US)

    return if (safeName.isEmpty()) FALLBACK_CD_AUDIO_SOURCE_STEM else safeName
}

internal fun chooseUniqueCdAudioImportStem(
    preferredStem: String,
    existingFileNames: Set<String>,
): String {
    val normalizedStem = sanitizeCdAudioImportStem(preferredStem)
    val takenNames = existingFileNames.map { it.lowercase(Locale.US) }.toSet()
    var candidate = normalizedStem
    var suffix = 2
    while ("$candidate.bin" in takenNames || "$candidate.cue" in takenNames) {
        candidate = "$normalizedStem-$suffix"
        suffix += 1
    }
    return candidate
}

internal fun isGeneratedMergedCueFile(cueFile: File): Boolean {
    if (!cueFile.isFile || !cueFile.name.endsWith(".cue", ignoreCase = true)) return false
    if (cueFile.nameWithoutExtension.startsWith("custom-")) return true
    return try {
        cueFile.bufferedReader().use { it.readLine() == GENERATED_MERGED_CUE_MARKER }
    } catch (_: Exception) {
        false
    }
}

internal fun isGeneratedMergedStorageArtifact(file: File): Boolean =
    when {
        !file.isFile -> {
            false
        }

        file.name.endsWith(".cue", ignoreCase = true) -> {
            isGeneratedMergedCueFile(file)
        }

        file.name.endsWith(".bin", ignoreCase = true) -> {
            val siblingCue = File(file.parentFile ?: return false, "${file.nameWithoutExtension}.cue")
            isGeneratedMergedCueFile(siblingCue)
        }

        else -> {
            false
        }
    }

internal fun usesMultipleCueFiles(tracks: List<DiscImportBridge.CueTrack>): Boolean {
    if (tracks.isEmpty()) return false
    val firstFileIndex = tracks.first().fileIndex
    return tracks.any { it.fileIndex != firstFileIndex }
}

internal fun normalizeCueTracksForMergedBin(
    tracks: List<DiscImportBridge.CueTrack>,
    binSizes: List<Long>,
): List<DiscImportBridge.CueTrack> {
    require(tracks.isNotEmpty()) { "tracks must not be empty" }

    val fileStartSectors = IntArray(binSizes.size)
    var sectorCursor = 0L
    for (index in binSizes.indices) {
        fileStartSectors[index] = sectorCursor.toInt()
        sectorCursor += binSizes[index] / CD_SECTOR_BYTES
    }

    return tracks.map { track ->
        require(track.fileIndex in binSizes.indices) {
            "Track ${track.trackNum} references missing BIN ${track.fileIndex}"
        }
        track.copy(
            fileIndex = 0,
            startSector = fileStartSectors[track.fileIndex] + track.startSector,
        )
    }
}

internal fun buildMergedCueText(
    binFileName: String,
    tracks: List<DiscImportBridge.CueTrack>,
): String {
    val builder = StringBuilder()
    builder.append(GENERATED_MERGED_CUE_MARKER)
    builder.append('\n')
    builder.append("FILE \"")
    builder.append(escapeCueText(binFileName))
    builder.append("\" BINARY\n")
    for (track in tracks) {
        builder.append("  TRACK ")
        builder.append(track.trackNum.toString().padStart(2, '0'))
        builder.append(' ')
        builder.append(if (track.isAudio) "AUDIO" else "MODE1/2352")
        builder.append('\n')
        if (track.title.isNotEmpty()) {
            builder.append("    TITLE \"")
            builder.append(escapeCueText(track.title))
            builder.append("\"\n")
        }
        builder.append("    INDEX 01 ")
        builder.append(sectorToMsf(track.startSector))
        builder.append('\n')
    }
    return builder.toString()
}

private fun sectorToMsf(sector: Int): String {
    val minutes = sector / (60 * 75)
    val seconds = (sector / 75) % 60
    val frames = sector % 75
    return String.format(Locale.US, "%02d:%02d:%02d", minutes, seconds, frames)
}

private fun escapeCueText(value: String): String =
    value
        .replace('"', '\'')
        .replace('\r', ' ')
        .replace('\n', ' ')
