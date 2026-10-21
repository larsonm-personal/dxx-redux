@file:Suppress("UNUSED_PARAMETER")

package android.content

// Host boundary only; service control-flow tests do not exercise Android storage
open class Context {
    open val noBackupFilesDir: java.io.File get() = error("Android private storage outside host test scope")
    val applicationContext: Context get() = this

    fun getSharedPreferences(
        name: String,
        mode: Int,
    ): HostPreferences = error("Android preferences outside host test scope")

    companion object {
        const val MODE_PRIVATE = 0
    }
}

class HostPreferences {
    fun getString(
        key: String,
        fallback: String?,
    ): String? = fallback

    fun getInt(
        key: String,
        fallback: Int,
    ): Int = fallback

    fun getBoolean(
        key: String,
        fallback: Boolean,
    ): Boolean = fallback

    fun edit(): HostPreferences = this

    fun putString(
        key: String,
        value: String?,
    ): HostPreferences = this

    fun putInt(
        key: String,
        value: Int,
    ): HostPreferences = this

    fun putBoolean(
        key: String,
        value: Boolean,
    ): HostPreferences = this

    fun apply() = Unit
}
