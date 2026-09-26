package com.dxxredux.app

import android.os.SystemClock
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import org.json.JSONObject

internal fun controllerAutomationKeyCode(name: String): Int =
    when (name.uppercase()) {
        "A" -> KeyEvent.KEYCODE_BUTTON_A
        "B" -> KeyEvent.KEYCODE_BUTTON_B
        "X" -> KeyEvent.KEYCODE_BUTTON_X
        "Y" -> KeyEvent.KEYCODE_BUTTON_Y
        "L1" -> KeyEvent.KEYCODE_BUTTON_L1
        "R1" -> KeyEvent.KEYCODE_BUTTON_R1
        "SELECT" -> KeyEvent.KEYCODE_BUTTON_SELECT
        "START" -> KeyEvent.KEYCODE_BUTTON_START
        "DUP" -> KeyEvent.KEYCODE_DPAD_UP
        "DDOWN" -> KeyEvent.KEYCODE_DPAD_DOWN
        "DLEFT" -> KeyEvent.KEYCODE_DPAD_LEFT
        "DRIGHT" -> KeyEvent.KEYCODE_DPAD_RIGHT
        else -> error("Unknown controller key: $name")
    }

/** Exercise Activity dispatch, including configured bindings, instead of bypassing it through SDL */
internal fun dispatchControllerAutomationInput(
    step: JSONObject,
    sendKey: (KeyEvent) -> Boolean,
    sendMotion: (MotionEvent) -> Boolean,
) {
    val now = SystemClock.uptimeMillis()
    if (step.has("key")) {
        val keyCode = controllerAutomationKeyCode(step.getString("key"))
        val actions = if (step.has("pressed")) listOf(step.getBoolean("pressed")) else listOf(true, false)
        for (down in actions) {
            sendKey(
                KeyEvent(
                    now,
                    now,
                    if (down) KeyEvent.ACTION_DOWN else KeyEvent.ACTION_UP,
                    keyCode,
                    step.optInt("repeat", 0),
                    0,
                    -1,
                    0,
                    0,
                    InputDevice.SOURCE_GAMEPAD,
                ),
            )
        }
    }
    step.optJSONObject("axes")?.let { axes ->
        val ids =
            mapOf(
                "LS_X" to MotionEvent.AXIS_X,
                "LS_Y" to MotionEvent.AXIS_Y,
                "RS_X" to MotionEvent.AXIS_Z,
                "RS_Y" to MotionEvent.AXIS_RZ,
                "HAT_X" to MotionEvent.AXIS_HAT_X,
                "HAT_Y" to MotionEvent.AXIS_HAT_Y,
            )
        val properties = MotionEvent.PointerProperties().apply { id = 0 }
        val coordinates = MotionEvent.PointerCoords()
        for (name in axes.keys()) {
            val axis = ids[name] ?: error("Unknown controller axis: $name")
            coordinates.setAxisValue(axis, axes.getDouble(name).toFloat())
        }
        val event =
            MotionEvent.obtain(
                now,
                now,
                MotionEvent.ACTION_MOVE,
                1,
                arrayOf(properties),
                arrayOf(coordinates),
                0,
                0,
                1f,
                1f,
                -1,
                0,
                InputDevice.SOURCE_JOYSTICK,
                0,
            )
        try {
            sendMotion(event)
        } finally {
            event.recycle()
        }
    }
}
