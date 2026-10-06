package com.dxxredux.app

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent

// Shared by device capability detection and gameplay key dispatch
internal val CONTROLLER_KEY_CODES =
    linkedMapOf(
        "A" to KeyEvent.KEYCODE_BUTTON_A,
        "B" to KeyEvent.KEYCODE_BUTTON_B,
        "X" to KeyEvent.KEYCODE_BUTTON_X,
        "Y" to KeyEvent.KEYCODE_BUTTON_Y,
        "L1" to KeyEvent.KEYCODE_BUTTON_L1,
        "R1" to KeyEvent.KEYCODE_BUTTON_R1,
        "Select" to KeyEvent.KEYCODE_BUTTON_SELECT,
        "Start" to KeyEvent.KEYCODE_BUTTON_START,
        "L3" to KeyEvent.KEYCODE_BUTTON_THUMBL,
        "R3" to KeyEvent.KEYCODE_BUTTON_THUMBR,
        "L2" to KeyEvent.KEYCODE_BUTTON_L2,
        "R2" to KeyEvent.KEYCODE_BUTTON_R2,
        "DUp" to KeyEvent.KEYCODE_DPAD_UP,
        "DDown" to KeyEvent.KEYCODE_DPAD_DOWN,
        "DLeft" to KeyEvent.KEYCODE_DPAD_LEFT,
        "DRight" to KeyEvent.KEYCODE_DPAD_RIGHT,
    )

internal fun connectedControllerDevices(): List<InputDevice> =
    InputDevice.getDeviceIds().toList().mapNotNull { InputDevice.getDevice(it) }.filter {
        !it.isVirtual &&
            (it.supportsSource(InputDevice.SOURCE_GAMEPAD) || it.supportsSource(InputDevice.SOURCE_JOYSTICK))
    }

internal data class ControllerInputAvailability(
    val connected: Boolean = false,
    val buttons: Set<Int> = emptySet(),
    // Physical SDL axis -> available signs (false = negative, true = positive)
    val axes: Map<Int, Set<Boolean>> = emptyMap(),
)

internal fun controllerInputAvailability(devices: List<InputDevice>): ControllerInputAvailability {
    val buttons = mutableSetOf<Int>()
    val axes = mutableMapOf<Int, MutableSet<Boolean>>()
    for (device in devices) {
        val keys = CONTROLLER_KEY_CODES.entries.toList()
        val present = device.hasKeys(*keys.map { it.value }.toIntArray())
        keys.forEachIndexed { index, key ->
            if (present[index]) (BUTTON_CONTROLS[key.key] ?: DPAD_CONTROLS[key.key])?.let(buttons::add)
        }
        if (!device.supportsSource(InputDevice.SOURCE_JOYSTICK)) continue
        for ((control, motionAxis) in CONTROLLER_MOTION_AXES) {
            val range = device.getMotionRange(motionAxis, InputDevice.SOURCE_JOYSTICK) ?: continue
            val signs = axes.getOrPut(AXIS_CONTROLS.getValue(control)) { mutableSetOf() }
            if (range.min < 0f) signs.add(false)
            if (range.max > 0f) signs.add(true)
            val pair = AXIS_BUTTON_SDL.getValue(control)
            if (range.min < 0f) buttons.add(pair.first)
            if (range.max > 0f) buttons.add(pair.second)
        }
        for ((axis, negative, positive) in listOf(
            Triple(MotionEvent.AXIS_HAT_X, "DLeft", "DRight"),
            Triple(MotionEvent.AXIS_HAT_Y, "DUp", "DDown"),
        )) {
            val range = device.getMotionRange(axis, InputDevice.SOURCE_JOYSTICK) ?: continue
            if (range.min < 0f) buttons.add(DPAD_CONTROLS.getValue(negative))
            if (range.max > 0f) buttons.add(DPAD_CONTROLS.getValue(positive))
        }
    }
    return ControllerInputAvailability(devices.isNotEmpty(), buttons, axes)
}

internal data class ControllerTouchCoverage(
    val connected: Boolean = false,
    val actions: Set<Int> = emptySet(),
    val directions: Set<Pair<String, Boolean>> = emptySet(),
)

internal fun controllerTouchCoverage(
    bindings: Map<String, String>,
    inverts: Set<String>,
    available: ControllerInputAvailability,
    gameVariant: String,
): ControllerTouchCoverage {
    if (!available.connected) return ControllerTouchCoverage()
    val actions = mutableSetOf<Int>()
    val directions = mutableSetOf<Pair<String, Boolean>>()
    // Use the dispatch builder, with D2's canonical touch IDs, including its half-axis priority rules
    val mixer = buildMixerButtonMap(bindings, "d2")
    for (button in available.buttons) {
        val entries = mixer.optJSONArray(button.toString()) ?: continue
        for (index in 0 until entries.length()) actions.add(entries.getInt(index))
    }
    for ((control, label) in bindings) {
        val meta = TouchBindings.metaActionIdForLabel(label)
        if (meta < 0) continue
        val axis = AXIS_BUTTON_SDL[control.removeSuffix("_neg").removeSuffix("_pos")]
        val button =
            BUTTON_CONTROLS[control] ?: DPAD_CONTROLS[control] ?: when {
                control.endsWith("_neg") -> axis?.first
                control.endsWith("_pos") -> axis?.second
                else -> null
            }
        if (button in available.buttons) actions.add(meta)
    }
    // Resolve the same final axis assignments and combiners written to the engine
    val pairs = buildJoyPairs(bindings, inverts, gameVariant)
    val values = pairs.indices.zip(pairs.values).toMap()
    for ((function, index) in AXIS_KC_INDEX) {
        val source = values[index] ?: continue
        val combiner = pairs.combiners.firstOrNull { it.first == source }
        val signs =
            if (combiner == null) {
                available.axes[source].orEmpty()
            } else {
                buildSet {
                    if (true in available.axes[combiner.second].orEmpty()) add(true)
                    if (true in available.axes[combiner.third].orEmpty()) add(false)
                }
            }
        for (positive in signs) directions.add(function to (positive xor (values[index + 1] == 1)))
    }
    for ((label, direction) in HALF_AXIS_MAP) {
        val action = TouchBindings.nameToBinding(label)
        if (action in actions) directions.add(direction)
        if (direction in directions && action != null) actions.add(action)
    }
    if (gameVariant == "d1") actions.removeAll(TouchBindings.D2_ONLY_BUTTONS + TouchBindings.D2_ONLY_META_ACTIONS)
    return ControllerTouchCoverage(true, actions, directions)
}

private val touchAxisFunctions =
    mapOf(
        TouchBindings.AXIS_LEFT_X to "Slide L/R",
        TouchBindings.AXIS_LEFT_Y to "Throttle",
        TouchBindings.AXIS_RIGHT_X to "Turn L/R",
        TouchBindings.AXIS_RIGHT_Y to "Pitch U/D",
        TouchBindings.AXIS_BANK to "Bank L/R",
        TouchBindings.AXIS_SLIDE_UD to "Slide U/D",
    )

private val preservedTouchMenuBindings =
    setOf(
        TouchBindings.META_MENU_CYCLE,
        TouchBindings.META_GAME_MENU,
        TouchBindings.META_GUIDE_BOT_MENU,
        TouchBindings.META_PAUSE,
        TouchBindings.META_RETURN_TO_LAUNCHER,
    )

internal fun controllerFilteredTouchLayout(
    layout: TouchLayout,
    coverage: ControllerTouchCoverage,
    gameVariant: String,
): TouchLayout {
    if (!layout.hideControllerBoundControls || !coverage.connected) return layout

    fun covered(binding: Int): Boolean =
        binding !in preservedTouchMenuBindings &&
            (
                binding in coverage.actions ||
                    (
                        gameVariant == "d1" &&
                            binding in TouchBindings.D2_ONLY_BUTTONS + TouchBindings.D2_ONLY_META_ACTIONS
                    )
            )

    fun axisCovered(axis: Int): Boolean {
        val function = touchAxisFunctions[axis] ?: return false
        return (function to false) in coverage.directions && (function to true) in coverage.directions
    }
    return layout.copy(
        buttons =
            layout.buttons.filterNot {
                covered(it.binding) && (!it.longPressEnabled || it.longPressBinding < 0 || covered(it.longPressBinding))
            },
        sticks =
            layout.sticks.filterNot {
                val gyroActivation =
                    layout.gyro.enabled && layout.gyro.activation == GyroActivation.TOUCH_STICK &&
                        (it.axisX == layout.gyro.axisX || it.axisY == layout.gyro.axisY)
                !gyroActivation &&
                    (
                        if (it.buttonMode) {
                            listOf(it.negXBinding, it.posXBinding, it.negYBinding, it.posYBinding).all(::covered)
                        } else {
                            axisCovered(it.axisX) && axisCovered(it.axisY)
                        }
                    ) &&
                    (it.doubleTapBinding < 0 || covered(it.doubleTapBinding)) &&
                    it.extremeActions.filter { action -> action.enabled }.all { action -> covered(action.binding) }
            },
        sliders = layout.sliders.filterNot { axisCovered(it.axis) },
        axisRegions = layout.axisRegions.filterNot { axisCovered(it.axis) },
        dpads =
            layout.dpads.filterNot {
                listOf(it.upBinding, it.downBinding, it.leftBinding, it.rightBinding).all(::covered)
            },
    )
}
