package com.dxxredux.app

internal class GamepadButtonEdgeTracker {
    private val downButtons = mutableSetOf<Int>()

    fun shouldDispatchDown(
        keyCode: Int,
        repeatCount: Int,
    ): Boolean {
        if (repeatCount > 0) return false
        return downButtons.add(keyCode)
    }

    fun shouldDispatchUp(keyCode: Int): Boolean = downButtons.remove(keyCode)

    fun clear() {
        downButtons.clear()
    }
}
