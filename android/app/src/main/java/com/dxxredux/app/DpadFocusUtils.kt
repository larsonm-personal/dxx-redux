package com.dxxredux.app

import androidx.compose.foundation.border
import androidx.compose.foundation.focusable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private const val NAV_REPEAT_INITIAL_DELAY_FALLBACK_MS = 500L
private const val NAV_REPEAT_INTERVAL_FALLBACK_MS = 125L

private data class PickerNavRepeatTiming(
    val initialDelayMs: Long,
    val repeatIntervalMs: Long,
)

private fun androidStaticRepeatTiming(methodName: String): Long? =
    runCatching {
        when (val value = android.view.ViewConfiguration::class.java.getMethod(methodName).invoke(null)) {
            is Int -> value.toLong()
            is Number -> value.toLong()
            else -> null
        }
    }.getOrNull()

private val pickerNavRepeatTiming: PickerNavRepeatTiming by lazy {
    PickerNavRepeatTiming(
        initialDelayMs = androidStaticRepeatTiming("getKeyRepeatTimeout") ?: NAV_REPEAT_INITIAL_DELAY_FALLBACK_MS,
        repeatIntervalMs = androidStaticRepeatTiming("getKeyRepeatDelay") ?: NAV_REPEAT_INTERVAL_FALLBACK_MS,
    )
}

/** Bright border shown on focused elements for TV/controller navigation. */
val tvFocusBorderColor = Color(0xFF00E676)
private val tvFocusBorderShape = RoundedCornerShape(6.dp)
private val tvFocusBorderWidth = 3.dp

internal fun shouldSeedLauncherControllerFocus(
    isAndroidTv: Boolean,
    controllerNavigationActive: Boolean,
): Boolean = isAndroidTv || controllerNavigationActive

internal fun shouldShowControllerFocusHighlight(
    hasTouchscreen: Boolean,
    controllerNavigationActive: Boolean,
): Boolean = !hasTouchscreen || controllerNavigationActive

/** Adds the shared bright TV focus border to any composable. */
fun Modifier.tvFocusBorder(): Modifier =
    composed {
        var focused by remember { mutableStateOf(false) }
        this
            .onFocusChanged { focused = it.isFocused }
            .then(
                if (focused) {
                    Modifier.border(tvFocusBorderWidth, tvFocusBorderColor, tvFocusBorderShape)
                } else {
                    Modifier
                },
            )
    }

/**
 * Makes a composable focusable with a visible highlight border when focused.
 * Use on custom clickable surfaces that aren't Material3 buttons (which
 * already have built-in focus indicators).
 */
fun Modifier.tvFocusable(): Modifier =
    composed {
        var focused by remember { mutableStateOf(false) }
        this
            .onFocusChanged { focused = it.isFocused }
            .then(
                if (focused) {
                    Modifier.border(tvFocusBorderWidth, tvFocusBorderColor, tvFocusBorderShape)
                } else {
                    Modifier
                },
            ).focusable()
    }

fun Modifier.repeatVerticalDpadFocus(downFocusRequester: FocusRequester? = null): Modifier =
    composed {
        val focusManager = LocalFocusManager.current
        val coroutineScope = rememberCoroutineScope()
        var heldDirection by remember { mutableIntStateOf(0) }
        var repeatJob by remember { mutableStateOf<Job?>(null) }

        fun stopRepeat() {
            repeatJob?.cancel()
            repeatJob = null
            heldDirection = 0
        }

        fun moveFocus(direction: Int) {
            if (direction < 0) {
                focusManager.moveFocus(FocusDirection.Up)
            } else {
                val moved = focusManager.moveFocus(FocusDirection.Down)
                if (!moved) downFocusRequester?.requestFocus()
            }
        }

        DisposableEffect(downFocusRequester) {
            onDispose { stopRepeat() }
        }

        this.onPreviewKeyEvent { event ->
            val direction =
                when (event.key) {
                    Key.DirectionUp -> -1
                    Key.DirectionDown -> 1
                    else -> 0
                }
            if (direction == 0) {
                return@onPreviewKeyEvent false
            }

            when (event.type) {
                KeyEventType.KeyDown -> {
                    if (heldDirection != direction) {
                        stopRepeat()
                        heldDirection = direction
                        moveFocus(direction)
                        val repeatTiming = pickerNavRepeatTiming
                        repeatJob =
                            coroutineScope.launch {
                                delay(repeatTiming.initialDelayMs)
                                while (heldDirection == direction) {
                                    moveFocus(direction)
                                    delay(repeatTiming.repeatIntervalMs)
                                }
                            }
                    }
                    true
                }

                KeyEventType.KeyUp -> {
                    if (heldDirection == direction) {
                        stopRepeat()
                    }
                    true
                }

                else -> {
                    false
                }
            }
        }
    }
