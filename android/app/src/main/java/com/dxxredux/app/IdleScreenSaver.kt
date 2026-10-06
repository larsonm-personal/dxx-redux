package com.dxxredux.app

import android.app.Activity
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import android.widget.Toast
import kotlin.math.abs
import kotlin.math.max

internal const val PREF_IDLE_SAVER_MINUTES = "idle_screen_saver_minutes"
internal val IDLE_SAVER_MINUTES_OPTIONS = listOf(0, 1, 5, 10, 15)

internal fun idleSaverMinutes(value: Int): Int = if (value in IDLE_SAVER_MINUTES_OPTIONS) value else 5

/** Display/input owner; native code owns eligibility, elapsed time, save completion, and audio */
internal class IdleScreenSaver(
    private val activity: Activity,
    private val ready: () -> Boolean,
    private val multiplayer: () -> Boolean,
    private val readState: () -> Int,
    private val setTimeout: (Int) -> Unit,
    private val noteActivity: () -> Unit,
    private val visibilityChanged: (Boolean) -> Unit,
) {
    // Keep in sync with android_idle_saver.h
    private val handler = Handler(Looper.getMainLooper())
    private var configurationChecked = false
    private var configuredMinutes: Int? = null
    private var touchHeld = false
    private var axesHeld = false
    private val heldKeys = mutableSetOf<Int>()
    private var previousState = -1
    private var dismissPending = false
    private var consumeTouch = false
    private var consumeAxes = false
    private val consumedKeys = mutableSetOf<Int>()
    private var savedBrightness = WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE
    private var overlay: TextView? = null

    val active: Boolean get() = overlay != null

    private val poll =
        object : Runnable {
            override fun run() {
                if (ready()) {
                    if (!configurationChecked) {
                        val prefs = activity.getSharedPreferences("dxx_prefs", Activity.MODE_PRIVATE)
                        val minutes = idleSaverMinutes(prefs.getInt(PREF_IDLE_SAVER_MINUTES, 5))
                        if (configuredMinutes != minutes) {
                            setTimeout(minutes * 60_000)
                            configuredMinutes = minutes
                        }
                        configurationChecked = true
                    }
                    if (touchHeld || axesHeld || heldKeys.isNotEmpty()) noteActivity()
                    refresh()
                }
                handler.postDelayed(this, 250)
            }
        }

    fun resume() {
        configurationChecked = false
        handler.removeCallbacks(poll)
        // Preserve native sleep and music state across OS screen-off/resume
        handler.post(poll)
    }

    fun releaseInputs() {
        touchHeld = false
        axesHeld = false
        heldKeys.clear()
        consumedKeys.clear()
        consumeTouch = false
        consumeAxes = false
    }

    fun stop() {
        releaseInputs()
        handler.removeCallbacks(poll)
    }

    fun dispose() {
        stop()
        hide()
    }

    fun refresh() {
        val state = readState()
        val hidden = state == 3 || state == 4
        if (!hidden) dismissPending = false
        if (hidden && !dismissPending) {
            show(state == 4)
        } else {
            hide()
        }
        if (state != previousState) {
            when (state) {
                2 -> {
                    Toast
                        .makeText(
                            activity,
                            if (multiplayer()) {
                                "Screen saver starting soon. Multiplayer will continue"
                            } else {
                                "Saving and sleeping soon. Press a button to stay awake"
                            },
                            Toast.LENGTH_LONG,
                        ).show()
                }

                5 -> {
                    Toast
                        .makeText(
                            activity,
                            "Could not save this game. Automatic sleep was cancelled",
                            Toast.LENGTH_LONG,
                        ).show()
                }
            }
        }
        previousState = state
    }

    private fun show(multiplayer: Boolean) {
        if (overlay != null) {
            visibilityChanged(true)
            return
        }
        savedBrightness = activity.window.attributes.screenBrightness
        overlay =
            TextView(activity).apply {
                setBackgroundColor(Color.BLACK)
                setTextColor(Color.DKGRAY)
                gravity = Gravity.CENTER
                textSize = 16f
                text =
                    if (multiplayer) {
                        "Multiplayer is still running\nTouch or press a button to return"
                    } else {
                        "Game saved - Paused\nTouch or press a button to return"
                    }
                isClickable = true
                importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_YES
                activity.addContentView(
                    this,
                    FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT,
                    ),
                )
            }
        activity.window.attributes = activity.window.attributes.apply { screenBrightness = 0f }
        if (multiplayer) {
            activity.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        } else {
            activity.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
        visibilityChanged(true)
    }

    private fun hide() {
        val view = overlay ?: return
        (view.parent as? android.view.ViewGroup)?.removeView(view)
        overlay = null
        activity.window.attributes = activity.window.attributes.apply { screenBrightness = savedBrightness }
        activity.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        visibilityChanged(false)
    }

    private fun activity(): Boolean {
        if (!ready()) return false
        val hidden = active || readState() in 3..4
        if (hidden) dismissPending = true
        noteActivity()
        if (hidden) hide()
        return hidden
    }

    fun touch(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            consumeTouch = activity()
            touchHeld = !consumeTouch
        } else if (!consumeTouch && event.actionMasked == MotionEvent.ACTION_MOVE) {
            activity()
        }
        val consumed = consumeTouch
        if (event.actionMasked == MotionEvent.ACTION_UP || event.actionMasked == MotionEvent.ACTION_CANCEL) {
            consumeTouch = false
            touchHeld = false
        }
        return consumed
    }

    fun key(event: KeyEvent): Boolean {
        // System volume/power keys keep their normal behavior
        if (event.keyCode in
            setOf(KeyEvent.KEYCODE_VOLUME_UP, KeyEvent.KEYCODE_VOLUME_DOWN, KeyEvent.KEYCODE_POWER)
        ) {
            return false
        }
        if (event.action == KeyEvent.ACTION_DOWN && activity()) consumedKeys.add(event.keyCode)
        val consumed = event.keyCode in consumedKeys
        if (event.action == KeyEvent.ACTION_DOWN && !consumed) heldKeys.add(event.keyCode)
        if (event.action == KeyEvent.ACTION_UP) {
            consumedKeys.remove(event.keyCode)
            heldKeys.remove(event.keyCode)
        }
        return consumed
    }

    fun motion(event: MotionEvent): Boolean {
        if (event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
            val meaningful =
                listOf(
                    MotionEvent.AXIS_X,
                    MotionEvent.AXIS_Y,
                    MotionEvent.AXIS_Z,
                    MotionEvent.AXIS_RZ,
                    MotionEvent.AXIS_HAT_X,
                    MotionEvent.AXIS_HAT_Y,
                    MotionEvent.AXIS_LTRIGGER,
                    MotionEvent.AXIS_RTRIGGER,
                    MotionEvent.AXIS_BRAKE,
                    MotionEvent.AXIS_GAS,
                ).any { axis ->
                    abs(event.getAxisValue(axis)) >
                        max(0.25f, event.device?.getMotionRange(axis, event.source)?.flat ?: 0f)
                }
            if (meaningful && activity()) consumeAxes = true
            val consumed = consumeAxes || active
            axesHeld = meaningful && !consumed
            if (!meaningful) consumeAxes = false
            return consumed
        }
        if (event.isFromSource(InputDevice.SOURCE_MOUSE)) return activity()
        return active
    }
}
