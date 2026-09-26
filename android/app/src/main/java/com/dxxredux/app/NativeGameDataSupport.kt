package com.dxxredux.app

/** Edition policy belongs to the engine reader and does not require initializing game state */
internal object NativeGameDataSupport {
    init {
        System.loadLibrary("dxx-redux-d2")
    }

    fun d1InD2EditionError(pigSize: Long): String? = nativeD1InD2EditionError(pigSize)

    @JvmStatic
    private external fun nativeD1InD2EditionError(pigSize: Long): String?
}
