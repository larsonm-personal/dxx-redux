package com.dxxredux.app

import android.hardware.input.InputManager
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import java.util.Locale

internal fun rawControllerSource(source: Int): Boolean =
    source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
        source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
        source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD

internal class ControllerInputDiagnostics : InputManager.InputDeviceListener {
    private val state = ControllerInputDiagnosticsState()
    var snapshot by mutableStateOf(ControllerInputDiagnosticsSnapshot())
        private set

    fun refresh() {
        val devices =
            InputDevice
                .getDeviceIds()
                .toList()
                .mapNotNull { InputDevice.getDevice(it) }
                .filter { rawControllerSource(it.sources) }
        for (old in snapshot.devices) if (devices.none { it.id == old.id }) state.removeDevice(old.id)
        devices.forEach(::recordDevice)
        publish()
    }

    private fun recordDevice(device: InputDevice) {
        state.device(
            RawControllerDevice(
                device.id,
                device.name,
                device.sources,
                device.motionRanges.filter { rawControllerSource(it.source) }.map {
                    RawControllerRange(
                        it.axis,
                        it.source,
                        MotionEvent.axisToString(it.axis),
                        it.min,
                        it.max,
                        it.flat,
                        it.fuzz,
                    )
                },
            ),
        )
    }

    fun motion(event: MotionEvent) {
        if (!rawControllerSource(event.source) || event.actionMasked != MotionEvent.ACTION_MOVE) return
        event.device?.let(::recordDevice)
        // Android packs axis presence into a 64-bit mask; include undeclared vendor axes
        for (history in 0 until event.historySize) {
            for (axis in 0..63) {
                state.axis(
                    event.deviceId,
                    event.source,
                    axis,
                    MotionEvent.axisToString(axis),
                    event.getHistoricalAxisValue(axis, history),
                    event.getHistoricalEventTime(history),
                )
            }
        }
        for (axis in 0..63) {
            state.axis(
                event.deviceId,
                event.source,
                axis,
                MotionEvent.axisToString(axis),
                event.getAxisValue(axis),
                event.eventTime,
            )
        }
        publish()
    }

    fun key(event: KeyEvent) {
        if (!rawControllerSource(event.source) && event.device?.let { rawControllerSource(it.sources) } != true) return
        if (event.action != KeyEvent.ACTION_DOWN && event.action != KeyEvent.ACTION_UP) return
        event.device?.let(::recordDevice)
        state.button(
            event.deviceId,
            event.source,
            event.keyCode,
            KeyEvent.keyCodeToString(event.keyCode),
            event.action == KeyEvent.ACTION_DOWN,
            event.eventTime,
        )
        publish()
    }

    fun clearLiveState() {
        state.clearLiveState()
        publish()
    }

    private fun publish() {
        snapshot = state.snapshot()
    }

    override fun onInputDeviceAdded(deviceId: Int) = refresh()

    override fun onInputDeviceChanged(deviceId: Int) {
        state.removeDevice(deviceId)
        refresh()
    }

    override fun onInputDeviceRemoved(deviceId: Int) {
        state.removeDevice(deviceId)
        publish()
    }
}

private fun rawNumber(value: Float): String = String.format(Locale.ROOT, "%.3f", value)

private fun rawSource(source: Int): String = "0x${source.toString(16)}"

@Composable
internal fun ControllerRawInputSummary(snapshot: ControllerInputDiagnosticsSnapshot) {
    val standardAxes =
        setOf(
            MotionEvent.AXIS_X,
            MotionEvent.AXIS_Y,
            MotionEvent.AXIS_Z,
            MotionEvent.AXIS_RZ,
            MotionEvent.AXIS_LTRIGGER,
            MotionEvent.AXIS_RTRIGGER,
            MotionEvent.AXIS_HAT_X,
            MotionEvent.AXIS_HAT_Y,
        )
    val extras = snapshot.axes.filter { it.axis !in standardAxes }
    if (extras.isNotEmpty()) {
        Text(
            "Other raw axes: " +
                extras.joinToString { "dev=${it.deviceId} ${it.name}=${it.value?.let(::rawNumber) ?: "not sampled"}" },
            fontSize = 11.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
    val held = snapshot.buttons.filter { it.pressed }
    Text(
        "Raw pressed buttons: " + held.joinToString { "dev=${it.deviceId} ${it.name} (${it.code})" }.ifEmpty { "none" },
        fontSize = 11.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

@Composable
internal fun ControllerRawInputReadout(
    snapshot: ControllerInputDiagnosticsSnapshot,
    requestFocus: Boolean = false,
) {
    val scroll = rememberScrollState()
    val scope = rememberCoroutineScope()
    val focus = remember { FocusRequester() }
    LaunchedEffect(requestFocus) {
        if (requestFocus) {
            withFrameNanos { }
            focus.requestFocus()
        }
    }
    Column(Modifier.fillMaxWidth()) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(
                onClick = { scope.launch { scroll.animateScrollTo((scroll.value - 180).coerceAtLeast(0)) } },
                modifier = Modifier.focusRequester(focus).tvFocusBorder(),
            ) { Text("Scroll up") }
            TextButton(
                onClick = {
                    scope.launch {
                        scroll.animateScrollTo(
                            (scroll.value + 180).coerceAtMost(scroll.maxValue),
                        )
                    }
                },
                modifier = Modifier.tvFocusBorder(),
            ) { Text("Scroll down") }
        }
        Column(Modifier.fillMaxWidth().heightIn(max = 260.dp).verticalScroll(scroll)) {
            Text("Raw controller inputs", fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
            Text(
                "Physical reports before mapping. Values are raw; nonzero does not always mean pressed.",
                fontSize = 11.sp,
            )
            for (device in snapshot.devices) {
                Text("Device ${device.id}: ${device.name} sources=${rawSource(device.sources)}", fontSize = 11.sp)
            }
            if (snapshot.axes.isEmpty()) Text("No controller axes advertised or observed", fontSize = 11.sp)
            for (axis in snapshot.axes) {
                val range = axis.range
                val detail =
                    if (range == null) {
                        "observed only"
                    } else {
                        "range=${rawNumber(
                            range.min,
                        )}..${rawNumber(range.max)} flat=${rawNumber(range.flat)} fuzz=${rawNumber(range.fuzz)}"
                    }
                Text(
                    "dev=${axis.deviceId} src=${rawSource(axis.source)} ${axis.name} (${axis.axis}): " +
                        "${axis.value?.let(::rawNumber) ?: "not sampled"} $detail",
                    fontSize = 11.sp,
                    color =
                        if (snapshot.recentEvents.takeLast(4).any {
                                it.deviceId == axis.deviceId &&
                                    it.input == "${axis.name} (${axis.axis})"
                            }
                        ) {
                            Color(0xFF4CAF50)
                        } else {
                            MaterialTheme.colorScheme.onSurfaceVariant
                        },
                )
            }
            Text("Raw buttons (including unknown codes)", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
            if (snapshot.buttons.isEmpty()) Text("No controller key events observed", fontSize = 11.sp)
            for (button in snapshot.buttons) {
                Text(
                    "dev=${button.deviceId} src=${rawSource(button.source)} ${button.name} (${button.code}): " +
                        if (button.pressed) "pressed" else "released",
                    fontSize = 11.sp,
                    color = if (button.pressed) Color(0xFF4CAF50) else MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Text("Recent physical changes (uptime ms)", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
            for (event in snapshot.recentEvents.asReversed()) {
                Text(
                    "${event.timeMs} dev=${event.deviceId} src=${rawSource(
                        event.source,
                    )} ${event.input}: ${event.value}",
                    fontSize = 11.sp,
                )
            }
        }
    }
}
