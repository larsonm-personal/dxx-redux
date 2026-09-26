package com.dxxredux.app

/** Keep releases with the destination that accepted the press, even when it opens a menu */
internal class ControllerKeyDispatch {
    private data class HeldKey(
        val send: (Boolean) -> Unit,
        val repeatable: Boolean,
    )

    private val held = mutableMapOf<Int, HeldKey>()

    fun press(
        keyCode: Int,
        repeatCount: Int,
        repeatable: Boolean,
        destination: () -> (Boolean) -> Unit,
    ) {
        val previous = held[keyCode]
        if (previous != null) {
            if (repeatCount > 0 && previous.repeatable) previous.send(true)
            return
        }
        if (repeatCount > 0) return
        val key = HeldKey(destination(), repeatable)
        held[keyCode] = key
        key.send(true)
    }

    fun release(keyCode: Int): Boolean {
        val key = held.remove(keyCode) ?: return false
        key.send(false)
        return true
    }

    fun releaseAll() {
        val keys = held.values.toList()
        held.clear()
        keys.forEach { it.send(false) }
    }
}
