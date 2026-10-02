package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject

internal data class RawControllerRange(
    val axis: Int,
    val source: Int,
    val name: String,
    val min: Float,
    val max: Float,
    val flat: Float,
    val fuzz: Float,
)

internal data class RawControllerDevice(
    val id: Int,
    val name: String,
    val sources: Int,
    val ranges: List<RawControllerRange> = emptyList(),
)

internal data class RawControllerAxis(
    val deviceId: Int,
    val source: Int,
    val axis: Int,
    val name: String,
    val range: RawControllerRange? = null,
    val value: Float? = null,
    val changedAtMs: Long? = null,
)

internal data class RawControllerButton(
    val deviceId: Int,
    val source: Int,
    val code: Int,
    val name: String,
    val pressed: Boolean,
    val changedAtMs: Long,
)

internal data class RawControllerEvent(
    val timeMs: Long,
    val deviceId: Int,
    val source: Int,
    val input: String,
    val value: String,
)

internal data class ControllerInputDiagnosticsSnapshot(
    val devices: List<RawControllerDevice> = emptyList(),
    val axes: List<RawControllerAxis> = emptyList(),
    val buttons: List<RawControllerButton> = emptyList(),
    val recentEvents: List<RawControllerEvent> = emptyList(),
) {
    fun toJson(): JSONObject =
        JSONObject()
            .put(
                "devices",
                JSONArray(
                    devices.map {
                        JSONObject().put("id", it.id).put("name", it.name).put("sources", it.sources)
                    },
                ),
            ).put(
                "axes",
                JSONArray(
                    axes.map {
                        JSONObject()
                            .put("device_id", it.deviceId)
                            .put("source", it.source)
                            .put("axis", it.axis)
                            .put("name", it.name)
                            .put("advertised", it.range != null)
                            .put("value", it.value ?: JSONObject.NULL)
                            .put("changed_at_ms", it.changedAtMs ?: JSONObject.NULL)
                            .put("min", it.range?.min ?: JSONObject.NULL)
                            .put("max", it.range?.max ?: JSONObject.NULL)
                            .put("flat", it.range?.flat ?: JSONObject.NULL)
                            .put("fuzz", it.range?.fuzz ?: JSONObject.NULL)
                    },
                ),
            ).put(
                "buttons",
                JSONArray(
                    buttons.map {
                        JSONObject()
                            .put("device_id", it.deviceId)
                            .put("source", it.source)
                            .put("code", it.code)
                            .put("name", it.name)
                            .put("pressed", it.pressed)
                            .put("changed_at_ms", it.changedAtMs)
                    },
                ),
            ).put(
                "recent_events",
                JSONArray(
                    recentEvents.map {
                        JSONObject()
                            .put("time_ms", it.timeMs)
                            .put("device_id", it.deviceId)
                            .put("source", it.source)
                            .put("input", it.input)
                            .put("value", it.value)
                    },
                ),
            )
}

/** Diagnostic state only; raw observations never create bindings or navigation */
internal class ControllerInputDiagnosticsState {
    private val devices = mutableMapOf<Int, RawControllerDevice>()
    private val axes = mutableMapOf<Triple<Int, Int, Int>, RawControllerAxis>()
    private val buttons = mutableMapOf<Triple<Int, Int, Int>, RawControllerButton>()
    private val events = ArrayDeque<RawControllerEvent>()

    fun device(device: RawControllerDevice) {
        val previous = devices.put(device.id, device)
        if (previous == device) return
        axes.keys.removeAll { it.first == device.id }
        buttons.keys.removeAll { it.first == device.id }
        for (range in device.ranges) {
            axes[Triple(device.id, range.source, range.axis)] =
                RawControllerAxis(device.id, range.source, range.axis, range.name, range)
        }
    }

    fun removeDevice(id: Int) {
        devices.remove(id)
        axes.keys.removeAll { it.first == id }
        buttons.keys.removeAll { it.first == id }
    }

    fun axis(
        deviceId: Int,
        source: Int,
        axis: Int,
        name: String,
        value: Float,
        timeMs: Long,
    ) {
        if (!value.isFinite()) return
        val range =
            devices[deviceId]?.ranges?.firstOrNull { it.axis == axis && it.source == source }
                ?: devices[deviceId]?.ranges?.firstOrNull { it.axis == axis && source and it.source == it.source }
        val key = Triple(deviceId, range?.source ?: source, axis)
        val previous = axes[key]
        // Zero alone does not establish support for an undeclared axis
        if (previous == null && range == null && value == 0f) return
        if (previous?.value == value) return
        axes[key] = RawControllerAxis(deviceId, key.second, axis, name, range, value, timeMs)
        if (previous?.value != null || value != 0f) {
            event(RawControllerEvent(timeMs, deviceId, source, "$name ($axis)", value.toString()))
        }
    }

    fun button(
        deviceId: Int,
        source: Int,
        code: Int,
        name: String,
        pressed: Boolean,
        timeMs: Long,
    ) {
        val key = Triple(deviceId, source, code)
        if (buttons[key]?.pressed == pressed) return
        buttons[key] = RawControllerButton(deviceId, source, code, name, pressed, timeMs)
        event(RawControllerEvent(timeMs, deviceId, source, "$name ($code)", if (pressed) "pressed" else "released"))
    }

    fun clearLiveState() {
        axes.replaceAll { _, axis -> axis.copy(value = null, changedAtMs = null) }
        buttons.clear()
    }

    fun snapshot(): ControllerInputDiagnosticsSnapshot =
        ControllerInputDiagnosticsSnapshot(
            devices.values.sortedBy { it.id },
            axes.values.sortedWith(compareBy({ it.deviceId }, { it.source }, { it.axis })),
            buttons.values.sortedWith(compareBy({ it.deviceId }, { it.source }, { it.code })),
            events.toList(),
        )

    private fun event(event: RawControllerEvent) {
        events.addLast(event)
        while (events.size > 24) events.removeFirst()
    }
}
