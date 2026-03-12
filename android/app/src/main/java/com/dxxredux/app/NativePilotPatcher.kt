package com.dxxredux.app

/**
 * Thin JNI wrapper around the C pilot-file patching code in
 * android_gamepad_config.cpp.  All .plr binary format knowledge
 * stays in C alongside playsave.c.
 */
object NativePilotPatcher {
    init {
        System.loadLibrary("dxx-redux-d2")
    }

    /**
     * Patch all .plr files in [filesDir] (and its Players/ subdirectory)
     * with the given KeySettings byte arrays and control type.
     *
     * @return number of files successfully patched
     */
    @JvmStatic
    external fun nativePatchPilotFiles(
        filesDir: String,
        joystickSettings: ByteArray,
        keyboardSettings: ByteArray,
        controlType: Int,
    ): Int

    /**
     * Build a 60-byte joystick KeySettings array from (kc_index, value) pairs.
     * The C side initializes invert slots to 0, others to 0xFF, then fills
     * the specified indices.  Indices are kc_joystick[] positions shared with
     * BUTTON_KC_INDEX / AXIS_KC_INDEX in ControllerConfigPage.kt.
     */
    @JvmStatic
    external fun nativeBuildJoySettings(
        indices: IntArray,
        values: IntArray,
    ): ByteArray

    /**
     * Build a 60-byte keyboard KeySettings array from (kc_index, scancode) pairs.
     * Indices are kc_keyboard[] positions shared with KB_KC_INDEX in
     * ControllerConfigPage.kt.
     */
    @JvmStatic
    external fun nativeBuildKbSettings(
        indices: IntArray,
        values: IntArray,
    ): ByteArray

    /**
     * Reset all .plr files in [filesDir] to engine default key settings
     * (keyboard + joystick + mouse) with touch overlay offsets applied.
     * Sets control_type to CONTROL_USING_JOYSTICK (1).
     *
     * @return number of files successfully patched
     */
    @JvmStatic
    external fun nativeResetToDefaults(filesDir: String): Int
}
