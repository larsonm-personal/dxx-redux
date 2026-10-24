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

    open fun bindService(
        intent: Intent,
        connection: ServiceConnection,
        flags: Int,
    ): Boolean = error("Unconfigured binding")

    open fun unbindService(connection: ServiceConnection) = Unit

    companion object {
        const val BIND_AUTO_CREATE = 1
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

class ComponentName

class Intent(
    context: Context,
    type: Class<*>,
)

interface ServiceConnection {
    fun onServiceConnected(
        name: ComponentName,
        binder: android.os.IBinder,
    )

    fun onServiceDisconnected(name: ComponentName)

    fun onBindingDied(name: ComponentName) = Unit

    fun onNullBinding(name: ComponentName) = Unit
}
