package com.dxxredux.app

import android.content.Context
import android.util.Log
import java.io.File

internal fun updateDescentCfgResolution(
    filesDir: File,
    resolution: String,
): Boolean {
    val (width, height) = parseSupportedAndroidRenderResolution(resolution) ?: return false
    updateAllConfigFiles(filesDir, listOf("ResolutionX" to "$width", "ResolutionY" to "$height"))
    return true
}

internal fun parseSupportedAndroidRenderResolution(resolution: String): Pair<Int, Int>? {
    val match = Regex("([0-9]+)x([0-9]+)").matchEntire(resolution) ?: return null
    val width = match.groupValues[1].toIntOrNull() ?: return null
    val height = match.groupValues[2].toIntOrNull() ?: return null
    return if (isSupportedAndroidRenderResolution(width, height)) width to height else null
}

// Keep these limits synchronized with shared/android_render_resolution.h.
internal fun isSupportedAndroidRenderResolution(
    width: Int,
    height: Int,
): Boolean =
    width in 320..4096 &&
        height in 200..4096 &&
        width.toLong() * height.toLong() <= 3840L * 2160L

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

internal fun readGraphicsConfigSnapshot(
    filesDir: File,
    game: String,
): List<Pair<String, Int>> {
    val gameSubdir = if (game == "d1") "d1x-redux" else "d2x-redux"
    val configTexts =
        listOf(File(filesDir, "$gameSubdir/descent.cfg"), File(filesDir, "descent.cfg"))
            .filter(File::exists)
            .map(File::readText)
    val options =
        mutableListOf(
            "GammaLevel" to "gamma_level",
            "TexFilt" to "tex_filt",
            "MenuTexFilt" to "menu_tex_filt",
            "HudTexFilt" to "hud_tex_filt",
            "MainViewFov" to "main_view_fov",
            "CornerTextInset" to "corner_text_inset",
            "AnisoLevel" to "aniso_level",
            "MsaaLevel" to "msaa_level",
            "ClassicDepth" to "classic_depth",
        )
    if (game == "d2") options.add("MovieTexFilt" to "movie_tex_filt")
    return options.mapNotNull { (configKey, nativeName) ->
        configTexts
            .firstNotNullOfOrNull { text ->
                Regex("^$configKey=(.*)$", RegexOption.MULTILINE)
                    .find(text)
                    ?.groupValues
                    ?.get(1)
                    ?.trim()
            }?.toIntOrNull()
            ?.let { nativeName to it }
    }
}

internal fun applyGraphicsOptionSnapshot(
    options: List<Pair<String, Int>>,
    apply: (String, Int) -> Boolean,
): Boolean = options.all { (name, value) -> apply(name, value) }

private fun updateConfigPaths(
    cfgPaths: List<File>,
    settings: List<Pair<String, String>>,
) {
    val updates =
        cfgPaths.distinctBy { it.absolutePath }.map { cfgFile ->
            var text = if (cfgFile.exists()) cfgFile.readText() else ""
            for ((key, value) in settings) {
                val regex = Regex("^$key=.*$", RegexOption.MULTILINE)
                text =
                    if (regex.containsMatchIn(text)) {
                        regex.replace(text) { "$key=$value" }
                    } else {
                        text.trimEnd() + "\n$key=$value\n"
                    }
            }
            cfgFile to text
        }
    AtomicFilePublication.writeUtf8Batch(updates)
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
    updateAllConfigFiles(filesDir, listOf("MusicType" to MUSIC_TYPE_REDBOOK.toString(), "OrigTrackOrder" to "1"))
    context
        .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        .edit()
        .putString("music_mode", "cd")
        .apply()
    Log.i("DXX-Setup", "Set music_mode=cd in SharedPreferences")
}

internal data class MusicLaunchPolicy(
    val musicType: String,
    val useMissionZipSoundtrack: Boolean,
    val useCdTrackOrder: Boolean,
    val useCustomAudioFiles: Boolean,
)

internal fun resolveMusicLaunchPolicy(
    musicSource: String?,
    missionHasSoundtrack: Boolean,
): MusicLaunchPolicy {
    val source = musicSource?.takeIf { it in setOf("mission", "midi", "cd", "files") } ?: "midi"
    val useMissionZipSoundtrack = source == "mission" && missionHasSoundtrack
    val musicType =
        when (source) {
            "mission", "midi" -> MUSIC_TYPE_BUILTIN.toString()
            "cd" -> MUSIC_TYPE_REDBOOK.toString()
            "files" -> MUSIC_TYPE_CUSTOM.toString()
            else -> MUSIC_TYPE_BUILTIN.toString()
        }

    return MusicLaunchPolicy(
        musicType = musicType,
        useMissionZipSoundtrack = useMissionZipSoundtrack,
        useCdTrackOrder = source == "cd",
        useCustomAudioFiles = source == "files",
    )
}

internal fun SetupActivity.writeMusicConfigForLaunch(
    game: String? = null,
    includeD1MissionZipsForD2: Boolean = true,
): Boolean {
    val prefs = getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
    val pilotMusic =
        NativePilotPreferences.readMusicPrefsForAll(
            preferredGame = game ?: "d2",
            filesDir = filesDir.absolutePath,
        )
    val source =
        if (pilotMusic.hasPilotFile) {
            when (pilotMusic.source) {
                "mission",
                "files",
                "cd",
                "midi",
                -> pilotMusic.source

                else -> "midi"
            }
        } else {
            prefs.getString("music_mode", "midi") ?: "midi"
        }
    val policy =
        resolveMusicLaunchPolicy(
            musicSource = source,
            missionHasSoundtrack =
                game != null &&
                    ModManager.forActiveSet(filesDir).hasEnabledMissionZipSoundtrack(game, includeD1MissionZipsForD2),
        )

    if (policy.useMissionZipSoundtrack) {
        Log.i("DXX-Setup", "Using mission zip soundtrack for $game launch")
    }

    val settings = mutableListOf("MusicType" to policy.musicType)
    if (pilotMusic.hasPilotFile) {
        settings.add("MusicVolume" to pilotMusic.volume.toString())
        settings.add("CMLevelMusicPlayOrder" to pilotMusic.playOrder.toString())
    }

    if (policy.useCdTrackOrder) {
        settings.add("OrigTrackOrder" to "1")
    } else if (policy.useCustomAudioFiles) {
        val m3uPath = CustomAudioSetManager.forActiveSet(filesDir).writeM3U(this)
        if (m3uPath == null) return false
        settings.add("CMLevelMusicPath" to m3uPath)
        if (!pilotMusic.hasPilotFile) {
            settings.add("CMLevelMusicPlayOrder" to "0")
        }
    }

    if (game == null) {
        updateAllConfigFiles(filesDir, settings)
    } else {
        updateConfigFilesForGame(filesDir, game, settings)
    }
    return true
}

internal fun selectBundledMusicForNewMission(
    filesDir: File,
    context: Context,
    game: String,
): Int {
    val existing = NativePilotPreferences.readMusicPrefsForAll(game, filesDir.absolutePath)
    val count =
        NativePilotPreferences.writeMusicPrefsToAll(
            filesDir.absolutePath,
            "mission",
            true,
            existing.playOrder,
            existing.volume,
        )
    if (count >= 0) {
        context
            .getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
            .edit()
            .putString("music_mode", "mission")
            .apply()
    }
    return count
}
