package com.dxxredux.app

import android.app.Instrumentation
import android.os.SystemClock
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp

/** Held input reaches real Compose focus navigation and production slider controls */
internal class NavigationRepeatChecks(
    private val instrumentation: Instrumentation,
) {
    private fun <T> onMain(block: () -> T): T {
        var result: Result<T>? = null
        instrumentation.runOnMainSync { result = runCatching(block) }
        return result!!.getOrThrow()
    }

    fun run(launcher: SetupActivity) {
        val timing = navigationRepeatTiming
        var focused = -1
        var value by mutableFloatStateOf(50f)
        var dialog by mutableIntStateOf(0)
        var dialogView: View? = null
        var dialogFocused = -1
        var dialogMoves = 0
        val rootFocus = FocusRequester()
        val sliderFocus = FocusRequester()
        val dialogFocus = FocusRequester()

        @Composable
        fun rows(
            inDialog: Boolean,
            ordered: Boolean,
        ) {
            val modifier = if (ordered) Modifier.repeatVerticalDpadFocus(useTraversalOrder = true) else Modifier
            Column(modifier.heightIn(max = 280.dp).verticalScroll(rememberScrollState())) {
                repeat(20) { index ->
                    TextButton(
                        {},
                        Modifier
                            .then(
                                if (index ==
                                    0
                                ) {
                                    Modifier.focusRequester(if (inDialog) dialogFocus else rootFocus)
                                } else {
                                    Modifier
                                },
                            ).onFocusChanged {
                                if (it.isFocused) {
                                    if (inDialog) {
                                        dialogFocused = index
                                        dialogMoves++
                                    } else {
                                        focused = index
                                    }
                                }
                            },
                    ) { Text("Row $index") }
                }
            }
        }

        onMain {
            launcher.setContent {
                MaterialTheme {
                    NavigationRepeatRoot {
                        Column {
                            rows(false, false)
                            // Exercise the same float slider used by Touch Editor
                            Column(Modifier.focusRequester(sliderFocus)) {
                                LabeledSlider("Repeat", value, 0f, 100f) { value = it }
                            }
                            when (dialog) {
                                1 -> {
                                    NavigationAlertDialog(
                                        onDismissRequest = { dialog = 0 },
                                        confirmButton = { TextButton({ dialog = 0 }) { Text("Close") } },
                                        text = {
                                            dialogView = LocalView.current
                                            rows(true, true)
                                        },
                                    )
                                }

                                2 -> {
                                    NavigationDialog({ dialog = 0 }) {
                                        dialogView = LocalView.current
                                        rows(true, false)
                                    }
                                }

                                3 -> {
                                    NavigationDropdownMenu(true, { dialog = 0 }) {
                                        dialogView = LocalView.current
                                        rows(true, false)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        Thread.sleep(600)

        fun focus(requester: FocusRequester) {
            onMain { requester.requestFocus() }
            Thread.sleep(100)
        }

        fun edge(
            code: Int,
            down: Boolean,
            repeat: Int = 0,
            target: View? = null,
        ) {
            onMain {
                val now = SystemClock.uptimeMillis()
                val event =
                    KeyEvent(
                        now,
                        now,
                        if (down) KeyEvent.ACTION_DOWN else KeyEvent.ACTION_UP,
                        code,
                        repeat,
                        0,
                        -1,
                        0,
                        0,
                        InputDevice.SOURCE_GAMEPAD,
                    )
                val handled = if (target != null) target.dispatchKeyEvent(event) else launcher.dispatchKeyEvent(event)
                Log.i(
                    "DXX-SliderTest",
                    "edge key=$code down=$down repeat=$repeat handled=$handled " +
                        "target=${target?.javaClass?.simpleName} window=${target?.let(System::identityHashCode)} " +
                        "focused=${target?.hasWindowFocus()} row=$dialogFocused moves=$dialogMoves",
                )
            }
        }

        fun holdTime(ready: (() -> Boolean)? = null) {
            val started = SystemClock.uptimeMillis()
            // Keep the nominal observation window so duplicate timers still fail
            Thread.sleep(timing.initialDelayMs + 3 * timing.repeatIntervalMs + 50)
            if (ready != null) {
                // Compose layout and emulator scheduling can postpone main-thread ticks
                while (!onMain(ready) && SystemClock.uptimeMillis() - started < 3000) {
                    Thread.sleep(20)
                }
                Log.i("DXX-SliderTest", "Held repeat observation elapsed_ms=${SystemClock.uptimeMillis() - started}")
            }
        }

        fun settle() = Thread.sleep(timing.repeatIntervalMs * 2 + 100)

        // Enter keyboard input mode before seeding focus
        edge(KeyEvent.KEYCODE_DPAD_UP, true)
        edge(KeyEvent.KEYCODE_DPAD_UP, false)
        focus(rootFocus)
        edge(KeyEvent.KEYCODE_DPAD_DOWN, true)
        check(onMain { focused == 1 }) { "Initial launcher direction did not move once: $focused" }
        edge(KeyEvent.KEYCODE_DPAD_DOWN, true, repeat = 1)
        check(onMain { focused == 1 }) { "Hardware repeat bypassed shared timing" }
        holdTime { focused >= 4 }
        edge(KeyEvent.KEYCODE_DPAD_DOWN, false)
        val stopped = onMain { focused }
        check(stopped >= 4) { "Held launcher direction did not scroll: $stopped" }
        settle()
        check(onMain { focused == stopped }) { "Launcher kept repeating after release" }

        // A single HAT event must keep navigating without further motion packets
        focus(rootFocus)

        fun hat(value: Float) {
            onMain {
                val now = SystemClock.uptimeMillis()
                val properties =
                    MotionEvent.PointerProperties().apply {
                        id = 0
                        toolType =
                            MotionEvent.TOOL_TYPE_UNKNOWN
                    }
                val coords = MotionEvent.PointerCoords().apply { setAxisValue(MotionEvent.AXIS_HAT_Y, value) }
                val event =
                    MotionEvent.obtain(
                        now,
                        now,
                        MotionEvent.ACTION_MOVE,
                        1,
                        arrayOf(properties),
                        arrayOf(coords),
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
                    launcher.dispatchGenericMotionEvent(event)
                } finally {
                    event.recycle()
                }
            }
        }
        hat(1f)
        holdTime { focused >= 4 }
        hat(0f)
        check(onMain { focused >= 4 }) { "Held HAT direction did not repeat" }

        focus(sliderFocus)
        edge(KeyEvent.KEYCODE_DPAD_RIGHT, true)
        holdTime { value >= 54f }
        edge(KeyEvent.KEYCODE_DPAD_RIGHT, false)
        val adjusted = onMain { value }
        check(adjusted >= 54f) { "Held right did not adjust the slider: $adjusted" }
        settle()
        check(onMain { value == adjusted }) { "Slider kept changing after release" }
        edge(KeyEvent.KEYCODE_DPAD_LEFT, true)
        holdTime { value < adjusted - 2 }
        edge(KeyEvent.KEYCODE_DPAD_LEFT, false)
        check(onMain { value < adjusted - 2 }) { "Held left did not reverse slider adjustment" }

        for (kind in 1..3) {
            Log.i(
                "DXX-SliderTest",
                "Dialog repeat case=$kind hold_ms=${timing.initialDelayMs + 3 * timing.repeatIntervalMs + 50}",
            )
            // Losing window focus must cancel a pending launcher hold
            focus(rootFocus)
            edge(KeyEvent.KEYCODE_DPAD_DOWN, true)
            onMain { dialog = kind }
            Thread.sleep(350)
            focus(dialogFocus)
            val target = onMain { checkNotNull(dialogView) }
            edge(KeyEvent.KEYCODE_DPAD_DOWN, true, target = target)
            holdTime { dialogFocused >= 4 }
            edge(KeyEvent.KEYCODE_DPAD_DOWN, false, target = target)
            val selected = onMain { dialogFocused }
            check(selected >= 4) { "Dialog $kind did not repeat: $selected" }
            // Ordered focus must not run a second timer alongside the window root
            check(selected <= 7) { "Dialog $kind repeated too quickly: $selected" }
            val moves = onMain { dialogMoves }
            settle()
            check(onMain { dialogMoves == moves }) { "Dialog $kind kept repeating after release" }
            edge(KeyEvent.KEYCODE_DPAD_UP, true, target = target)
            onMain { dialog = 0 }
            Thread.sleep(350)
            focus(rootFocus)
            holdTime()
            check(onMain { focused == 0 }) { "Dismissed dialog $kind leaked a hold into launcher" }
            edge(KeyEvent.KEYCODE_DPAD_DOWN, false)
        }
    }
}
