package com.dxxredux.app

/**
 * JNI wrapper for weapon autoselect ordering read/write in pilot files.
 *
 * D1 pilot files store weapon ordering as text INI in .plx files.
 * D2 pilot files store weapon ordering as binary in .plr files.
 * All format details are in C (android_autoselect.cpp / playsave.c).
 *
 * Weapon names are hardcoded English strings matching the in-game
 * "Reorder Primary" / "Reorder Secondary" menus exactly.
 *
 * Shared constants for array layout:
 *   D1 primary order: 7 entries (5 weapons + separator + Quad Lasers)
 *   D1 secondary order: 6 entries (5 weapons + separator)
 *   D2 primary order: 11 entries (10 weapons + separator)
 *   D2 secondary order: 11 entries (10 weapons + separator)
 *   Separator value: 255
 *   D1 Quad Lasers index: 16
 */
object NativeAutoselectPatcher {
    const val SEPARATOR = 255
    const val D1_QUAD_LASERS_INDEX = 16

    /**
     * Read autoselect ordering from the first pilot file found.
     * Returns flat int array: [primary..., secondary...]
     * Returns empty array if no pilot file exists.
     */
    @JvmStatic
    external fun nativeReadAutoselect(filesDir: String): IntArray

    /**
     * Write autoselect ordering to ALL pilot files for this game.
     * @return number of files patched
     */
    @JvmStatic
    external fun nativeWriteAutoselect(
        filesDir: String,
        primaryOrder: IntArray,
        secondaryOrder: IntArray,
    ): Int

    /**
     * Get weapon name strings for the current game build.
     * D1: 5 primary names + "Quad Lasers" + 5 secondary names = 11
     * D2: 10 primary names + 10 secondary names = 20
     */
    @JvmStatic
    external fun nativeGetWeaponNames(): Array<String>

    /**
     * Get default autoselect ordering for the current game build.
     * Same flat format as nativeReadAutoselect.
     */
    @JvmStatic
    external fun nativeGetDefaultAutoselect(): IntArray

    /**
     * Get [primaryOrderLength, secondaryOrderLength] for the current game build.
     */
    @JvmStatic
    external fun nativeGetOrderLengths(): IntArray
}
