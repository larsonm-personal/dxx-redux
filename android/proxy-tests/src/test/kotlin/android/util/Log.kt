@file:Suppress("UNUSED_PARAMETER")

package android.util

// Host-only logcat sink; production source still uses Android Log
object Log {
    val entries = java.util.concurrent.CopyOnWriteArrayList<String>()

    fun d(
        tag: String,
        message: String,
    ): Int {
        entries.add(message)
        return 0
    }

    fun i(
        tag: String,
        message: String,
    ): Int {
        entries.add(message)
        return 0
    }

    fun w(
        tag: String,
        message: String,
        error: Throwable? = null,
    ): Int {
        entries.add(message)
        return 0
    }

    fun e(
        tag: String,
        message: String,
        error: Throwable? = null,
    ): Int {
        entries.add(message)
        if (error != null) entries.add(error.stackTraceToString())
        return 0
    }
}
