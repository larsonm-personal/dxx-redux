package com.dxxredux.app

import android.hardware.input.InputManager
import android.os.SystemClock
import android.util.Log
import android.view.KeyEvent
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.platform.LocalWindowInfo

internal val LocalNavigationRepeatRoot = staticCompositionLocalOf { false }

/** One timer owner per Compose window, including dialogs and popup menus */
@Composable
internal fun NavigationRepeatRoot(content: @Composable () -> Unit) {
    CompositionLocalProvider(LocalNavigationRepeatRoot provides true) {
        Box(Modifier.repeatDpadKeys(), propagateMinConstraints = true) { content() }
    }
}

internal fun Modifier.repeatDpadKeys(): Modifier =
    composed {
        val view = LocalView.current
        val windowFocused = LocalWindowInfo.current.isWindowFocused
        val dispatcher =
            remember(view) {
                ControllerKeyDispatch(
                    object : NavigationRepeatScheduler {
                        override fun postDelayed(
                            action: Runnable,
                            delayMs: Long,
                        ) {
                            view.postDelayed(action, delayMs)
                        }

                        override fun removeCallbacks(action: Runnable) {
                            view.removeCallbacks(action)
                        }
                    },
                )
            }
        var redispatching by remember(view) { mutableStateOf(false) }
        DisposableEffect(view) {
            val inputs = view.context.getSystemService(InputManager::class.java)
            val listener =
                object : InputManager.InputDeviceListener {
                    override fun onInputDeviceAdded(deviceId: Int) = Unit

                    override fun onInputDeviceChanged(deviceId: Int) = Unit

                    override fun onInputDeviceRemoved(deviceId: Int) = dispatcher.releaseAll()
                }
            inputs.registerInputDeviceListener(listener, null)
            onDispose { inputs.unregisterInputDeviceListener(listener) }
        }
        DisposableEffect(view, windowFocused) {
            if (BuildConfig.DEBUG) {
                Log.d(
                    "DXX-NavRepeat",
                    "window=${System.identityHashCode(view)} focused=$windowFocused",
                )
            }
            if (!windowFocused) dispatcher.releaseAll()
            onDispose {
                if (BuildConfig.DEBUG) {
                    Log.d(
                        "DXX-NavRepeat",
                        "window=${System.identityHashCode(view)} dispose focused=$windowFocused",
                    )
                }
                dispatcher.releaseAll()
            }
        }
        this
            .pointerInput(dispatcher) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)
                    dispatcher.releaseAll()
                }
            }.onPreviewKeyEvent { event ->
                if (redispatching) return@onPreviewKeyEvent false
                val native = event.nativeKeyEvent
                if (native.keyCode !in KeyEvent.KEYCODE_DPAD_UP..KeyEvent.KEYCODE_DPAD_RIGHT) {
                    if (native.action == KeyEvent.ACTION_DOWN) dispatcher.releaseAll()
                    return@onPreviewKeyEvent false
                }
                if (BuildConfig.DEBUG) {
                    Log.d(
                        "DXX-NavRepeat",
                        "window=${System.identityHashCode(view)} action=${native.action} key=${native.keyCode} " +
                            "repeat=${native.repeatCount} held=${dispatcher.heldKeyCodes()}",
                    )
                }
                when (native.action) {
                    KeyEvent.ACTION_DOWN -> {
                        val duplicate = native.keyCode in dispatcher.heldKeyCodes() || native.repeatCount > 0
                        dispatcher.press(native.keyCode, native.repeatCount, true) {
                            val original = KeyEvent(native)
                            var repeatCount = 0
                            val send: (Boolean) -> Unit = { down ->
                                if (down && repeatCount++ > 0) {
                                    if (BuildConfig.DEBUG) {
                                        Log.d(
                                            "DXX-NavRepeat",
                                            "window=${System.identityHashCode(view)} tick=$repeatCount " +
                                                "attached=${view.isAttachedToWindow} focused=${view.hasWindowFocus()}",
                                        )
                                    }
                                    if (view.isAttachedToWindow && view.hasWindowFocus()) {
                                        redispatching = true
                                        try {
                                            view.dispatchKeyEvent(
                                                KeyEvent.changeTimeRepeat(
                                                    original,
                                                    SystemClock.uptimeMillis(),
                                                    repeatCount - 1,
                                                ),
                                            )
                                        } finally {
                                            redispatching = false
                                        }
                                    } else {
                                        dispatcher.releaseAll()
                                    }
                                }
                            }
                            send
                        }
                        duplicate
                    }

                    KeyEvent.ACTION_UP -> {
                        dispatcher.release(native.keyCode)
                        false
                    }

                    else -> {
                        false
                    }
                }
            }
    }
