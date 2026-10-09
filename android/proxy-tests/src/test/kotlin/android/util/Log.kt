@file:Suppress("UNUSED_PARAMETER")

package android.util

// Host-only logcat sink; production source still uses Android Log
object Log {
    fun i(
        tag: String,
        message: String,
    ): Int = 0

    fun w(
        tag: String,
        message: String,
    ): Int = 0

    fun e(
        tag: String,
        message: String,
        error: Throwable? = null,
    ): Int = 0
}
