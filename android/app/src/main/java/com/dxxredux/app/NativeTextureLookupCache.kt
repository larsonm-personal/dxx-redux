package com.dxxredux.app

/* Runtime-only negative cache for replacement texture misses.
 * Keep invalidation explicit so imported files and mod changes are visible
 * on the next launch without needing an app-data wipe. */
object NativeTextureLookupCache {
    init {
        try {
            System.loadLibrary("dxx-redux-d1")
        } catch (_: UnsatisfiedLinkError) {
        }
        try {
            System.loadLibrary("dxx-redux-d2")
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    @JvmStatic
    fun clear() {
        try {
            nativeClearD1()
        } catch (_: UnsatisfiedLinkError) {
        }
        try {
            nativeClearD2()
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    @JvmStatic
    private external fun nativeClearD1()

    @JvmStatic
    private external fun nativeClearD2()
}
