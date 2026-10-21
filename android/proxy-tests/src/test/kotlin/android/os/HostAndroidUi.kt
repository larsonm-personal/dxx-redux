@file:Suppress("UNUSED_PARAMETER")

package android.os

class Looper {
    companion object {
        fun getMainLooper() = Looper()
    }
}

class Handler(
    looper: Looper,
    val callback: ((Message) -> Boolean)? = null,
) {
    fun post(action: Runnable): Boolean {
        action.run()
        return true
    }

    fun postDelayed(
        action: Runnable,
        delay: Long,
    ): Boolean = true

    fun removeCallbacks(action: Runnable) = Unit
}

class Bundle(
    source: Bundle? = null,
) {
    private val values: MutableMap<String, Any?> = source?.values?.toMutableMap() ?: mutableMapOf<String, Any?>()

    fun putString(
        key: String,
        value: String?,
    ) {
        values[key] = value
    }

    fun getString(key: String): String? = values[key] as? String

    fun putBoolean(
        key: String,
        value: Boolean,
    ) {
        values[key] = value
    }

    fun getBoolean(key: String): Boolean = values[key] as? Boolean ?: false

    fun putInt(
        key: String,
        value: Int,
    ) {
        values[key] = value
    }

    fun getInt(key: String): Int = values[key] as? Int ?: 0

    fun putLong(
        key: String,
        value: Long,
    ) {
        values[key] = value
    }

    fun getLong(key: String): Long = values[key] as? Long ?: 0L

    fun putIntArray(
        key: String,
        value: IntArray,
    ) {
        values[key] = value
    }

    fun getIntArray(key: String): IntArray? = values[key] as? IntArray

    companion object {
        val EMPTY = Bundle()
    }
}

class Message(
    var what: Int,
) {
    var data = Bundle()
    var replyTo: Messenger? = null

    companion object {
        fun obtain(
            handler: Handler?,
            what: Int,
        ) = Message(what)
    }
}

interface IBinder

class HostBinder(
    val receiver: (Message) -> Unit,
) : IBinder

class Messenger(
    val binder: IBinder,
) {
    constructor(handler: Handler) : this(HostBinder { handler.callback?.invoke(it) })

    fun send(message: Message) {
        (binder as HostBinder).receiver(message)
    }
}

class RemoteException : Exception()

object SystemClock {
    fun elapsedRealtime(): Long = System.nanoTime() / 1_000_000
}
