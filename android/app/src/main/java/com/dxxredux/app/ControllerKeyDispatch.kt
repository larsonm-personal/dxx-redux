package com.dxxredux.app

/** Keep releases with the destination that accepted the press, even when it opens a menu */
internal class ControllerKeyDispatch(
    private val repeatScheduler: NavigationRepeatScheduler? = null,
    private val repeatTiming: NavigationRepeatTiming = navigationRepeatTiming,
) {
    private data class HeldKey(
        val send: (Boolean) -> Unit,
        val repeatable: Boolean,
        var repeat: Runnable? = null,
    )

    private val held = mutableMapOf<Int, HeldKey>()

    fun heldKeyCodes(): Set<Int> = held.keys.toSet()

    fun press(
        keyCode: Int,
        repeatCount: Int,
        repeatable: Boolean,
        destination: () -> (Boolean) -> Unit,
    ) {
        val previous = held[keyCode]
        if (previous != null) {
            if (repeatScheduler == null && repeatCount > 0 && previous.repeatable) previous.send(true)
            return
        }
        if (repeatCount > 0) return
        val key = HeldKey(destination(), repeatable)
        held[keyCode] = key
        key.send(true)
        if (repeatable && repeatScheduler != null && held[keyCode] === key) {
            val repeat =
                object : Runnable {
                    override fun run() {
                        if (held[keyCode] !== key) return
                        key.send(true)
                        if (held[keyCode] === key) repeatScheduler.postDelayed(this, repeatTiming.repeatIntervalMs)
                    }
                }
            key.repeat = repeat
            repeatScheduler.postDelayed(repeat, repeatTiming.initialDelayMs)
        }
    }

    fun release(keyCode: Int): Boolean {
        val key = held.remove(keyCode) ?: return false
        key.repeat?.let { repeatScheduler?.removeCallbacks(it) }
        key.send(false)
        return true
    }

    fun releaseAll() {
        val keys = held.values.toList()
        held.clear()
        keys.forEach {
            it.repeat?.let { repeat -> repeatScheduler?.removeCallbacks(repeat) }
            it.send(false)
        }
    }
}
