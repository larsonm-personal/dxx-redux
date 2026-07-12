package com.dxxredux.app

/**
 * Thin JNI wrapper around the C pilot-file patching code in
 * android_gamepad_config.cpp.  All .plr binary format knowledge
 * stays in C alongside playsave.c.
 */
object NativePilotPatcher {
    init {
        System.loadLibrary("dxx-redux-d1")
        System.loadLibrary("dxx-redux-d2")
    }

    /**
     * Patch all .plr files for the given [game] ("d1" or "d2") in [filesDir]
     * with the given KeySettings byte arrays and control type.
     * Only scans the matching game's pref directory (d1x-redux/ or d2x-redux/).
     *
     * @return number of files successfully patched
     */
    @JvmStatic
    external fun nativePatchPilotFilesD1(
        filesDir: String,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int,
        game: String,
    ): Int

    @JvmStatic
    external fun nativePatchPilotFilesD2(
        filesDir: String,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int,
        game: String,
    ): Int

    fun nativePatchPilotFiles(
        filesDir: String,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int,
        game: String,
    ): Int =
        if (game == "d1") {
            nativePatchPilotFilesD1(filesDir, joystickSettings, keyboardSettings, controlType, game)
        } else {
            nativePatchPilotFilesD2(filesDir, joystickSettings, keyboardSettings, controlType, game)
        }

    /**
     * Build the selected game's joystick KeySettings array from (kc_index, value) pairs.
     * The C side initializes invert slots to 0, others to 0xFF, then fills
     * the specified indices.  Indices are kc_joystick[] positions shared with
     * BUTTON_KC_INDEX / AXIS_KC_INDEX in ControllerConfigPage.kt.
     */
    @JvmStatic
    external fun nativeBuildJoySettingsD1(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray

    @JvmStatic
    external fun nativeBuildJoySettingsD2(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray

    fun nativeBuildJoySettings(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray =
        if (game == "d1") {
            nativeBuildJoySettingsD1(indices, values, game)
        } else {
            nativeBuildJoySettingsD2(indices, values, game)
        }

    /**
     * Build the selected game's keyboard KeySettings array from (kc_index, scancode) pairs.
     * Indices are kc_keyboard[] positions shared with KB_KC_INDEX in
     * ControllerConfigPage.kt.
     */
    @JvmStatic
    external fun nativeBuildKbSettingsD1(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray

    @JvmStatic
    external fun nativeBuildKbSettingsD2(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray

    fun nativeBuildKbSettings(
        indices: IntArray,
        values: IntArray,
        game: String,
    ): ByteArray =
        if (game == "d1") {
            nativeBuildKbSettingsD1(indices, values, game)
        } else {
            nativeBuildKbSettingsD2(indices, values, game)
        }

    /**
     * Reset all .plr files for the given [game] ("d1" or "d2") in [filesDir]
     * to engine default key settings (keyboard + joystick + mouse) with touch
     * overlay offsets applied.  Sets control_type to CONTROL_USING_JOYSTICK (1).
     *
     * @return number of files successfully patched
     */
    @JvmStatic
    external fun nativeResetToDefaultsD1(
        filesDir: String,
        game: String,
    ): Int

    @JvmStatic
    external fun nativeResetToDefaultsD2(
        filesDir: String,
        game: String,
    ): Int

    fun nativeResetToDefaults(
        filesDir: String,
        game: String,
    ): Int =
        if (game == "d1") {
            nativeResetToDefaultsD1(filesDir, game)
        } else {
            nativeResetToDefaultsD2(filesDir, game)
        }
}
