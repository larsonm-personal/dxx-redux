package com.dxxredux.app

import android.content.Context
import android.util.Log
import java.io.File

internal fun updateDescentCfgResolution(
    filesDir: File,
    resolution: String,
) {
    val parts = resolution.split("x")
    val width = parts.getOrNull(0)?.toIntOrNull() ?: return
    val height = parts.getOrNull(1)?.toIntOrNull() ?: return
    updateAllConfigFiles(filesDir, listOf("ResolutionX" to "$width", "ResolutionY" to "$height"))
}

internal fun readConfigValue(
    filesDir: File,
    key: String,
): String? {
    for (subdir in listOf("d2x-redux", "d1x-redux", "")) {
        val cfgFile =
            if (subdir.isEmpty()) {
                File(
                    filesDir,
                    "descent.cfg",
                )
            } else {
                File(File(filesDir, subdir), "descent.cfg")
            }
        if (!cfgFile.exists()) continue
        val regex = Regex("^$key=(.*)$", RegexOption.MULTILINE)
        val match = regex.find(cfgFile.readText()) ?: continue
        return match.groupValues[1].trim()
    }
    return null
}

internal fun readConfigValueForGame(
    filesDir: File,
    game: String,
    key: String,
): String? {
    val gameSubdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    for (subdir in listOf(gameSubdir, "")) {
        val cfgFile =
            if (subdir.isEmpty()) {
                File(
                    filesDir,
                    "descent.cfg",
                )
            } else {
                File(File(filesDir, subdir), "descent.cfg")
            }
        if (!cfgFile.exists()) continue
        val regex = Regex("^$key=(.*)$", RegexOption.MULTILINE)
        val match = regex.find(cfgFile.readText()) ?: continue
        return match.groupValues[1].trim()
    }
    return null
}

private fun updateConfigPaths(
    cfgPaths: List<File>,
    settings: List<Pair<String, String>>,
) {
    for (cfgFile in cfgPaths.distinctBy { it.absolutePath }) {
        var text = if (cfgFile.exists()) cfgFile.readText() else ""
        for ((key, value) in settings) {
            val regex = Regex("^$key=.*$", RegexOption.MULTILINE)
            text =
                if (regex.containsMatchIn(text)) {
                    regex.replace(text, "$key=$value")
                } else {
                    text.trimEnd() + "\n$key=$value\n"
                }
        }
        cfgFile.writeText(text)
    }
}

private fun setupLogInfo(message: String) {
    try {
        Log.i("DXX-Setup", message)
    } catch (_: Throwable) {
        println("[INFO] DXX-Setup: $message")
    }
}

internal fun updateAllConfigFiles(
    filesDir: File,
    settings: List<Pair<String, String>>,
) {
    val cfgPaths = mutableListOf(File(filesDir, "descent.cfg"))
    for (subdir in listOf("d1x-redux", "d2x-redux")) {
        val dir = File(filesDir, subdir)
        if (dir.isDirectory) cfgPaths.add(File(dir, "descent.cfg"))
    }
    updateConfigPaths(cfgPaths, settings)
    setupLogInfo(
        "Updated ${cfgPaths.size} descent.cfg files: ${settings.joinToString { "${it.first}=${it.second}" }}",
    )
}

internal fun updateConfigFilesForGame(
    filesDir: File,
    game: String,
    settings: List<Pair<String, String>>,
) {
    val subdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val cfgPaths = mutableListOf(File(filesDir, "descent.cfg"))
    val dir = File(filesDir, subdir)
    if (dir.isDirectory) cfgPaths.add(File(dir, "descent.cfg"))
    updateConfigPaths(cfgPaths, settings)
    setupLogInfo(
        "Updated ${cfgPaths.size} $game descent.cfg file(s): ${settings.joinToString { "${it.first}=${it.second}" }}",
    )
}

internal fun enableRedbookInConfig(
    filesDir: File,
    context: Context,
) {
    updateAllConfigFiles(filesDir, listOf("MusicType" to "2", "OrigTrackOrder" to "1"))
    context
        .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        .edit()
        .putString("music_mode", "cd")
        .apply()
    Log.i("DXX-Setup", "Set music_mode=cd in SharedPreferences")
}

internal fun SetupActivity.writeMusicConfigForLaunch(game: String? = null) {
    val prefs = getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
    val mode = prefs.getString("music_mode", "cd") ?: "cd"
    val missionZipBuiltinMusic =
        game != null && mode == "cd" && ModManager(filesDir).hasEnabledMissionZipBuiltinMusic(game)
    val musicType =
        when {
            missionZipBuiltinMusic -> "1"
            mode == "midi" -> "1"
            mode == "cd" -> "2"
            mode == "files" -> "3"
            else -> "2"
        }

    if (missionZipBuiltinMusic) {
        Log.i("DXX-Setup", "Using mission zip built-in music for $game launch")
    }

    val settings = mutableListOf("MusicType" to musicType)

    if (!missionZipBuiltinMusic && mode == "cd") {
        settings.add("OrigTrackOrder" to "1")
    } else if (mode == "files") {
        val m3uPath = CustomAudioSetManager(filesDir).writeM3U(this)
        if (m3uPath != null) {
            settings.add("CMLevelMusicPath" to m3uPath)
            settings.add("CMLevelMusicPlayOrder" to "0")
        }
    }

    if (game == null) {
        updateAllConfigFiles(filesDir, settings)
    } else {
        updateConfigFilesForGame(filesDir, game, settings)
    }
}
