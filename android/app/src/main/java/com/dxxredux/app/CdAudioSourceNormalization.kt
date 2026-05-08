package com.dxxredux.app

import java.util.Locale

private const val CD_SECTOR_BYTES = 2352L

internal fun isLocalCdContentPath(path: String): Boolean =
    path.startsWith("/") || Regex("^[A-Za-z]:[\\\\/]").containsMatchIn(path)

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
