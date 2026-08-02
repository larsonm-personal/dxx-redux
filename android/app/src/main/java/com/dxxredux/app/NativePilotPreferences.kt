package com.dxxredux.app

/**
 * JNI wrapper for launcher-side engine preferences stored in pilot files.
 *
 * D1 and D2 store these fields differently, so the native layer keeps the
 * player-file parsing and patching in playsave.c.
 */
object NativePilotPreferences {
    data class EnginePrefs(
        val hasPilotFile: Boolean,
        val cockpitMode: Int,
        val autoLeveling: Boolean,
        val showRobotHostageCounts: Boolean,
        val showBossHealthBar: Boolean,
        val headlightActiveDefault: Boolean,
    )

    data class VisualPrefs(
        val hasPilotFile: Boolean,
        val alphaEffects: Boolean,
        val dynLightColor: Boolean,
    )

    data class OriginalHomingPrefs(
        val hasPilotFile: Boolean,
        val enabled: Boolean,
    )

    data class MusicPrefs(
        val hasPilotFile: Boolean,
        val source: String,
        val preferMissionSoundtrack: Boolean,
        val playOrder: Int,
        val volume: Int,
    )

    init {
        System.loadLibrary("dxx-redux-d1")
        System.loadLibrary("dxx-redux-d2")
    }

    @JvmStatic external fun nativeReadEnginePrefsD1(filesDir: String): IntArray

    @JvmStatic external fun nativeReadEnginePrefsD2(filesDir: String): IntArray

    @JvmStatic external fun nativeWriteEnginePrefsD1(
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
        showRobotHostageCounts: Boolean,
        showBossHealthBar: Boolean,
        headlightActiveDefault: Boolean,
    ): Int

    @JvmStatic external fun nativeWriteEnginePrefsD2(
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
        showRobotHostageCounts: Boolean,
        showBossHealthBar: Boolean,
        headlightActiveDefault: Boolean,
    ): Int

    @JvmStatic external fun nativeReadVisualPrefsD1(filesDir: String): IntArray

    @JvmStatic external fun nativeReadVisualPrefsD2(filesDir: String): IntArray

    @JvmStatic external fun nativeReadOriginalHomingPrefsD1(filesDir: String): IntArray

    @JvmStatic external fun nativeReadOriginalHomingPrefsD2(filesDir: String): IntArray

    @JvmStatic external fun nativeWriteOriginalHomingPrefsD1(
        filesDir: String,
        enabled: Boolean,
    ): Int

    @JvmStatic external fun nativeWriteOriginalHomingPrefsD2(
        filesDir: String,
        enabled: Boolean,
    ): Int

    @JvmStatic external fun nativeWriteVisualPrefsD1(
        filesDir: String,
        alphaEffects: Boolean,
        dynLightColor: Boolean,
    ): Int

    @JvmStatic external fun nativeWriteVisualPrefsD2(
        filesDir: String,
        alphaEffects: Boolean,
        dynLightColor: Boolean,
    ): Int

    @JvmStatic external fun nativeReadMusicPrefsD1(filesDir: String): IntArray

    @JvmStatic external fun nativeReadMusicPrefsD2(filesDir: String): IntArray

    @JvmStatic external fun nativeWriteMusicPrefsD1(
        filesDir: String,
        source: Int,
        preferMissionSoundtrack: Boolean,
        playOrder: Int,
        volume: Int,
    ): Int

    @JvmStatic external fun nativeWriteMusicPrefsD2(
        filesDir: String,
        source: Int,
        preferMissionSoundtrack: Boolean,
        playOrder: Int,
        volume: Int,
    ): Int

    private fun decodeEnginePrefs(raw: IntArray): EnginePrefs =
        EnginePrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            cockpitMode = if (raw.size >= 2) raw[1] else 0,
            autoLeveling = raw.size >= 3 && raw[2] != 0,
            showRobotHostageCounts = raw.size >= 4 && raw[3] != 0,
            showBossHealthBar = raw.size < 5 || raw[4] != 0,
            headlightActiveDefault = raw.size >= 6 && raw[5] != 0,
        )

    private fun decodeVisualPrefs(raw: IntArray): VisualPrefs =
        VisualPrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            alphaEffects = raw.size >= 2 && raw[1] != 0,
            dynLightColor = raw.size >= 3 && raw[2] != 0,
        )

    private fun decodeOriginalHomingPrefs(raw: IntArray): OriginalHomingPrefs =
        OriginalHomingPrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            enabled = raw.size >= 2 && raw[1] != 0,
        )

    private fun musicSourceName(source: Int): String =
        when (source) {
            0 -> "mission"
            1 -> "files"
            3 -> "midi"
            else -> "cd"
        }

    private fun musicSourceCode(source: String): Int =
        when (source) {
            "mission" -> 0
            "files" -> 1
            "midi" -> 3
            else -> 2
        }

    private fun decodeMusicPrefs(raw: IntArray): MusicPrefs =
        MusicPrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            source = musicSourceName(if (raw.size >= 2) raw[1] else 2),
            preferMissionSoundtrack = raw.size < 3 || raw[2] != 0,
            playOrder = (if (raw.size >= 4) raw[3] else 0).coerceIn(0, 2),
            volume = (if (raw.size >= 5) raw[4] else 8).coerceIn(0, 8),
        )

    fun readEnginePrefs(
        game: String,
        filesDir: String,
    ): EnginePrefs =
        decodeEnginePrefs(if (game == "d1") nativeReadEnginePrefsD1(filesDir) else nativeReadEnginePrefsD2(filesDir))

    fun readEnginePrefsForAll(
        preferredGame: String,
        filesDir: String,
    ): EnginePrefs {
        val preferred = readEnginePrefs(preferredGame, filesDir)
        if (preferred.hasPilotFile) return preferred
        return readEnginePrefs(if (preferredGame == "d1") "d2" else "d1", filesDir)
    }

    fun writeEnginePrefs(
        game: String,
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
        showRobotHostageCounts: Boolean,
        showBossHealthBar: Boolean,
        headlightActiveDefault: Boolean = false,
    ): Int =
        if (game == "d1") {
            nativeWriteEnginePrefsD1(
                filesDir,
                cockpitMode,
                autoLeveling,
                showRobotHostageCounts,
                showBossHealthBar,
                headlightActiveDefault,
            )
        } else {
            nativeWriteEnginePrefsD2(
                filesDir,
                cockpitMode,
                autoLeveling,
                showRobotHostageCounts,
                showBossHealthBar,
                headlightActiveDefault,
            )
        }

    fun writeEnginePrefsToAll(
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
        showRobotHostageCounts: Boolean,
        showBossHealthBar: Boolean,
        headlightActiveDefault: Boolean = false,
    ): Int =
        nativeWriteEnginePrefsD1(
            filesDir,
            cockpitMode,
            autoLeveling,
            showRobotHostageCounts,
            showBossHealthBar,
            headlightActiveDefault,
        ) +
            nativeWriteEnginePrefsD2(
                filesDir,
                cockpitMode,
                autoLeveling,
                showRobotHostageCounts,
                showBossHealthBar,
                headlightActiveDefault,
            )

    fun readVisualPrefs(
        game: String,
        filesDir: String,
    ): VisualPrefs =
        decodeVisualPrefs(if (game == "d1") nativeReadVisualPrefsD1(filesDir) else nativeReadVisualPrefsD2(filesDir))

    fun readOriginalHomingPrefs(
        game: String,
        filesDir: String,
    ): OriginalHomingPrefs =
        decodeOriginalHomingPrefs(
            if (game == "d1") {
                nativeReadOriginalHomingPrefsD1(filesDir)
            } else {
                nativeReadOriginalHomingPrefsD2(filesDir)
            },
        )

    fun readOriginalHomingPrefsForAll(
        preferredGame: String,
        filesDir: String,
    ): OriginalHomingPrefs {
        val preferred = readOriginalHomingPrefs(preferredGame, filesDir)
        if (preferred.hasPilotFile) return preferred
        return readOriginalHomingPrefs(if (preferredGame == "d1") "d2" else "d1", filesDir)
    }

    fun writeOriginalHomingPrefs(
        game: String,
        filesDir: String,
        enabled: Boolean,
    ): Int =
        if (game == "d1") {
            nativeWriteOriginalHomingPrefsD1(filesDir, enabled)
        } else {
            nativeWriteOriginalHomingPrefsD2(filesDir, enabled)
        }

    fun writeOriginalHomingPrefsToAll(
        filesDir: String,
        enabled: Boolean,
    ): Int =
        nativeWriteOriginalHomingPrefsD1(filesDir, enabled) +
            nativeWriteOriginalHomingPrefsD2(filesDir, enabled)

    fun readVisualPrefsForAll(
        preferredGame: String,
        filesDir: String,
    ): VisualPrefs {
        val preferred = readVisualPrefs(preferredGame, filesDir)
        if (preferred.hasPilotFile) return preferred
        return readVisualPrefs(if (preferredGame == "d1") "d2" else "d1", filesDir)
    }

    fun writeVisualPrefs(
        game: String,
        filesDir: String,
        alphaEffects: Boolean,
        dynLightColor: Boolean,
    ): Int =
        if (game == "d1") {
            nativeWriteVisualPrefsD1(filesDir, alphaEffects, dynLightColor)
        } else {
            nativeWriteVisualPrefsD2(filesDir, alphaEffects, dynLightColor)
        }

    fun writeVisualPrefsToAll(
        filesDir: String,
        alphaEffects: Boolean,
        dynLightColor: Boolean,
    ): Int =
        nativeWriteVisualPrefsD1(filesDir, alphaEffects, dynLightColor) +
            nativeWriteVisualPrefsD2(filesDir, alphaEffects, dynLightColor)

    fun readMusicPrefs(
        game: String,
        filesDir: String,
    ): MusicPrefs =
        decodeMusicPrefs(if (game == "d1") nativeReadMusicPrefsD1(filesDir) else nativeReadMusicPrefsD2(filesDir))

    fun readMusicPrefsForAll(
        preferredGame: String,
        filesDir: String,
    ): MusicPrefs {
        val preferred = readMusicPrefs(preferredGame, filesDir)
        if (preferred.hasPilotFile) return preferred
        return readMusicPrefs(if (preferredGame == "d1") "d2" else "d1", filesDir)
    }

    fun writeMusicPrefs(
        game: String,
        filesDir: String,
        source: String,
        preferMissionSoundtrack: Boolean,
        playOrder: Int,
        volume: Int,
    ): Int =
        if (game == "d1") {
            nativeWriteMusicPrefsD1(
                filesDir,
                musicSourceCode(source),
                preferMissionSoundtrack,
                playOrder.coerceIn(0, 2),
                volume.coerceIn(0, 8),
            )
        } else {
            nativeWriteMusicPrefsD2(
                filesDir,
                musicSourceCode(source),
                preferMissionSoundtrack,
                playOrder.coerceIn(0, 2),
                volume.coerceIn(0, 8),
            )
        }

    fun writeMusicPrefsToAll(
        filesDir: String,
        source: String,
        preferMissionSoundtrack: Boolean,
        playOrder: Int,
        volume: Int,
    ): Int =
        nativeWriteMusicPrefsD1(
            filesDir,
            musicSourceCode(source),
            preferMissionSoundtrack,
            playOrder.coerceIn(0, 2),
            volume.coerceIn(0, 8),
        ) +
            nativeWriteMusicPrefsD2(
                filesDir,
                musicSourceCode(source),
                preferMissionSoundtrack,
                playOrder.coerceIn(0, 2),
                volume.coerceIn(0, 8),
            )
}
