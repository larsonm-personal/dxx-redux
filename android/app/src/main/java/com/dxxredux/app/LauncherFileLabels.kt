package com.dxxredux.app

import java.io.File
import java.util.Locale

private val launcherFileTypeLabels =
    mapOf(
        "hog" to "Mission or game archive",
        "mn2" to "Descent 2 mission descriptor",
        "msn" to "Descent 1 mission descriptor",
        "ham" to "Robot and weapon data",
        "vham" to "Variant robot and weapon data",
        "pig" to "Texture and sound data",
        "pog" to "Texture override pack",
        "pcx" to "Briefing or cutscene image",
        "s11" to "11 kHz sound data",
        "s22" to "22 kHz sound data",
        "hmp" to "HMI MIDI music",
        "mid" to "MIDI music",
        "raw" to "Raw PCM audio",
        "rl2" to "Descent 2 level",
        "rdl" to "Descent 1 level",
        "mvl" to "Movie library archive",
        "dxa" to "Game mod",
        "dtx" to "D2X-XL texture pack",
        "gog" to "GOG CD audio image",
        "inst" to "GOG CD audio cue sheet",
        "bin" to "CD disc image",
        "cue" to "CD cue sheet",
        "dem" to "Demo recording",
        "zip" to "ZIP archive",
        "7z" to "7z archive",
        "mp3" to "MP3 music file",
        "ogg" to "Ogg Vorbis music file",
        "flac" to "FLAC music file",
        "wav" to "WAV audio file",
        "m3u" to "Music playlist",
        "plr" to "Pilot file",
        "plx" to "Extended pilot settings",
        "eff" to "Effects settings",
        "ngp" to "Network game profile",
        "cfg" to "Game configuration",
        "json" to "Launcher settings",
        "jsonl" to "Launcher event log",
        "txt" to "Text file",
        "log" to "Log file",
        "exe" to "Windows installer",
        "pkg" to "Mac installer",
        "sow" to "SOW archive",
    )

private val savedGameExtensions =
    buildSet {
        for (i in 0..9) {
            add("sg$i")
            add("mg$i")
        }
    }

private fun leafName(path: String): String = path.substringAfterLast('/').substringAfterLast('\\')

private fun extensionOf(filename: String): String {
    val leaf = leafName(filename).lowercase(Locale.US)
    val ext = leaf.substringAfterLast('.', "")
    return if (ext == leaf) "" else ext
}

private fun lowerFirst(text: String): String =
    if (text.isEmpty()) text else text.substring(0, 1).lowercase(Locale.US) + text.substring(1)

internal fun launcherFileTypeLabel(filename: String): String {
    val leaf = leafName(filename).lowercase(Locale.US)
    val ext = extensionOf(leaf)
    return when {
        leaf == "assets.json" -> "Game file manifest"
        leaf == ".saf_manifest.json" -> "SAF game-file link manifest"
        leaf == "audio_sources.json" -> "CD audio source settings"
        leaf == "audio_playlist.json" -> "Generated CD audio playlist"
        leaf == "file_sets.json" -> "Imported file set settings"
        leaf == "import_location.txt" -> "Imported files location setting"
        ext in savedGameExtensions -> "Saved game"
        ext.isEmpty() -> "File"
        else -> launcherFileTypeLabels[ext] ?: ".$ext file"
    }
}

internal fun launcherExtensionDescription(filename: String): String {
    val ext = extensionOf(filename)
    val label = launcherFileTypeLabel(filename)
    return if (ext.isEmpty()) label else ".$ext - ${lowerFirst(label)}"
}

internal fun launcherStorageFilePurpose(
    file: File,
    relativePath: String,
    importedRootFile: Boolean,
): String {
    val name = file.name.lowercase(Locale.US)
    val path = relativePath.replace('\\', '/').lowercase(Locale.US)
    val importedPath = importedRootFile || path.startsWith("imported/") || path.startsWith("sets/")
    return when {
        isGeneratedMergedStorageArtifact(file) -> "Imported (merged) CD audio"
        importedPath -> launcherFileTypeLabel(name)
        path.startsWith("d1x-redux/") || path.startsWith("d2x-redux/") -> launcherFileTypeLabel(name)
        path.startsWith("mods/") -> launcherFileTypeLabel(name)
        path.startsWith("custom_music/") -> launcherFileTypeLabel(name)
        path.startsWith("debuglogs/") -> "Debug log"
        path.startsWith("tombstones/") || path.startsWith("crashlogs/") -> "Crash report"
        path.startsWith("tmp/") -> "Temporary import file"
        else -> launcherFileTypeLabel(name)
    }
}
