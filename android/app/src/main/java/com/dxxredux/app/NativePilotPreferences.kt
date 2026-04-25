package com.dxxredux.app

/**
 * JNI wrapper for launcher-side cockpit mode and auto-level preferences.
 *
 * D1 and D2 store these fields differently, so the native layer keeps the
 * player-file parsing and patching in playsave.c.
 */
object NativePilotPreferences {
    data class EnginePrefs(
        val hasPilotFile: Boolean,
        val cockpitMode: Int,
        val autoLeveling: Boolean,
    )

    data class VisualPrefs(
        val hasPilotFile: Boolean,
        val alphaEffects: Boolean,
        val dynLightColor: Boolean,
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
    ): Int

    @JvmStatic external fun nativeWriteEnginePrefsD2(
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
    ): Int

    @JvmStatic external fun nativeReadVisualPrefsD1(filesDir: String): IntArray

    @JvmStatic external fun nativeReadVisualPrefsD2(filesDir: String): IntArray

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

    private fun decodeEnginePrefs(raw: IntArray): EnginePrefs =
        EnginePrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            cockpitMode = if (raw.size >= 2) raw[1] else 0,
            autoLeveling = raw.size >= 3 && raw[2] != 0,
        )

    private fun decodeVisualPrefs(raw: IntArray): VisualPrefs =
        VisualPrefs(
            hasPilotFile = raw.size >= 1 && raw[0] != 0,
            alphaEffects = raw.size >= 2 && raw[1] != 0,
            dynLightColor = raw.size >= 3 && raw[2] != 0,
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
    ): Int =
        if (game == "d1") {
            nativeWriteEnginePrefsD1(filesDir, cockpitMode, autoLeveling)
        } else {
            nativeWriteEnginePrefsD2(filesDir, cockpitMode, autoLeveling)
        }

    fun writeEnginePrefsToAll(
        filesDir: String,
        cockpitMode: Int,
        autoLeveling: Boolean,
    ): Int =
        nativeWriteEnginePrefsD1(filesDir, cockpitMode, autoLeveling) +
            nativeWriteEnginePrefsD2(filesDir, cockpitMode, autoLeveling)

    fun readVisualPrefs(
        game: String,
        filesDir: String,
    ): VisualPrefs =
        decodeVisualPrefs(if (game == "d1") nativeReadVisualPrefsD1(filesDir) else nativeReadVisualPrefsD2(filesDir))

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
}
