@file:Suppress("UNUSED_PARAMETER")

package android.os

class Looper {
    companion object {
        fun getMainLooper() = Looper()
    }
}

class Handler(
    looper: Looper,
) {
    fun post(action: Runnable): Boolean {
        action.run()
        return true
    }
}
