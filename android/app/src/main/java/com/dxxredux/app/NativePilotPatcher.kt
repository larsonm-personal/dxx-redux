package com.dxxredux.app

/**
 * Thin JNI wrapper around the C pilot-file patching code in
 * android_gamepad_config.cpp.  All .plr binary format knowledge
 * stays in C alongside playsave.c.
 */
object NativePilotPatcher {
    init { System.loadLibrary("d2x-redux") }

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
        controlType: Int
    ): Int
}
