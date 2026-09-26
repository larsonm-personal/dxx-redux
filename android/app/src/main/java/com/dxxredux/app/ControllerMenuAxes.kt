package com.dxxredux.app

import android.view.KeyEvent
import kotlin.math.abs

/** Both sticks navigate menus using their raw deflection, independent of gameplay curves */
internal class ControllerMenuAxes {
    private var horizontal = 0
    private var vertical = 0

    fun update(
        leftX: Float,
        leftY: Float,
        rightX: Float,
        rightY: Float,
        hatX: Float,
        hatY: Float,
        send: (Int, Boolean) -> Unit,
    ) {
        fun direction(
            first: Float,
            second: Float,
            hat: Float,
        ): Int {
            val value =
                if (abs(hat) > 0.5f) {
                    hat
                } else if (abs(first) >= abs(second)) {
                    first
                } else {
                    second
                }
            return when {
                value < -0.5f -> -1
                value > 0.5f -> 1
                else -> 0
            }
        }
        val nextHorizontal = direction(leftX, rightX, hatX)
        val nextVertical = direction(leftY, rightY, hatY)
        if (horizontal != nextHorizontal) {
            if (horizontal != 0) send(horizontalKey(horizontal), false)
            horizontal = nextHorizontal
            if (horizontal != 0) send(horizontalKey(horizontal), true)
        }
        if (vertical != nextVertical) {
            if (vertical != 0) send(verticalKey(vertical), false)
            vertical = nextVertical
            if (vertical != 0) send(verticalKey(vertical), true)
        }
    }

    fun reset(send: (Int, Boolean) -> Unit) {
        update(0f, 0f, 0f, 0f, 0f, 0f, send)
    }

    private fun horizontalKey(direction: Int): Int =
        if (direction < 0) KeyEvent.KEYCODE_DPAD_LEFT else KeyEvent.KEYCODE_DPAD_RIGHT

    private fun verticalKey(direction: Int): Int =
        if (direction < 0) KeyEvent.KEYCODE_DPAD_UP else KeyEvent.KEYCODE_DPAD_DOWN
}
