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
     * Primary weapon entries: paired [indexStr, name, indexStr, name, ...].
     * Every weapon index that can appear in a primary ordering is included,
     * with its display name.  Separator (255) is included.
     * Order length = result.size / 2.
     */
    @JvmStatic
    external fun nativeGetPrimaryWeaponEntries(): Array<String>

    /**
     * Secondary weapon entries: same paired format as primary.
     */
    @JvmStatic
    external fun nativeGetSecondaryWeaponEntries(): Array<String>

    /** Parse paired [indexStr, name, ...] into a Map<weaponIndex, displayName>. */
    fun parseWeaponEntries(entries: Array<String>): Map<Int, String> {
        val map = LinkedHashMap<Int, String>(entries.size / 2)
        for (i in entries.indices step 2) {
            map[entries[i].toInt()] = entries[i + 1]
        }
        return map
    }
}
