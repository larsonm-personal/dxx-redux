package com.dxxredux.app

/**
 * JNI wrapper for weapon autoselect ordering read/write in pilot files.
 *
 * Both libdxx-redux-d1.so and libdxx-redux-d2.so are loaded at init.
 * Each exports game-specific JNI symbols (suffixed D1/D2) so ART
 * resolves them to the correct library without caching conflicts.
 *
 * Kotlin callers use the dispatcher functions (readAutoselect, etc.)
 * which route to the correct native method based on a game parameter.
 */
object NativeAutoselectPatcher {
    const val SEPARATOR = 255

    init {
        System.loadLibrary("dxx-redux-d1")
        System.loadLibrary("dxx-redux-d2")
    }

    // -- D1 native methods (bound to libdxx-redux-d1.so) --
    @JvmStatic external fun nativeReadAutoselectD1(filesDir: String): IntArray

    @JvmStatic external fun nativeWriteAutoselectD1(
        filesDir: String,
        primaryOrder: IntArray,
        secondaryOrder: IntArray,
    ): Int

    @JvmStatic external fun nativeGetPrimaryWeaponEntriesD1(): Array<String>

    @JvmStatic external fun nativeGetSecondaryWeaponEntriesD1(): Array<String>

    // -- D2 native methods (bound to libdxx-redux-d2.so) --
    @JvmStatic external fun nativeReadAutoselectD2(filesDir: String): IntArray

    @JvmStatic external fun nativeWriteAutoselectD2(
        filesDir: String,
        primaryOrder: IntArray,
        secondaryOrder: IntArray,
    ): Int

    @JvmStatic external fun nativeGetPrimaryWeaponEntriesD2(): Array<String>

    @JvmStatic external fun nativeGetSecondaryWeaponEntriesD2(): Array<String>

    // -- Dispatchers --
    fun readAutoselect(
        game: String,
        filesDir: String,
    ): IntArray = if (game == "d1") nativeReadAutoselectD1(filesDir) else nativeReadAutoselectD2(filesDir)

    fun writeAutoselect(
        game: String,
        filesDir: String,
        primary: IntArray,
        secondary: IntArray,
    ): Int =
        if (game == "d1") {
            nativeWriteAutoselectD1(filesDir, primary, secondary)
        } else {
            nativeWriteAutoselectD2(filesDir, primary, secondary)
        }

    fun getPrimaryWeaponEntries(game: String): Array<String> =
        if (game == "d1") nativeGetPrimaryWeaponEntriesD1() else nativeGetPrimaryWeaponEntriesD2()

    fun getSecondaryWeaponEntries(game: String): Array<String> =
        if (game == "d1") nativeGetSecondaryWeaponEntriesD1() else nativeGetSecondaryWeaponEntriesD2()

    /** Parse paired [indexStr, name, ...] into a Map<weaponIndex, displayName>. */
    fun parseWeaponEntries(entries: Array<String>): Map<Int, String> {
        val map = LinkedHashMap<Int, String>(entries.size / 2)
        for (i in entries.indices step 2) {
            map[entries[i].toInt()] = entries[i + 1]
        }
        return map
    }
}
