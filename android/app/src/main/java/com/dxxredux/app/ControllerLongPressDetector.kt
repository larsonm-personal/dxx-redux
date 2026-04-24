package com.dxxredux.app

import kotlin.math.abs

internal class ControllerLongPressDetector(
    private val longPressMs: Long = 2000L,
    private val axisSelectThreshold: Float = 0.80f,
    private val axisOtherThreshold: Float = 0.30f,
) {
    sealed class Trigger {
        data class Axis(
            val axisIndex: Int,
            val positive: Boolean,
        ) : Trigger()

        data class Button(
            val buttonName: String,
        ) : Trigger()
    }

    private val axisStartMs = LongArray(AXIS_COUNT) { STATE_INACTIVE }
    private val axisDirection = IntArray(AXIS_COUNT)
    private val dpadStartMs = LongArray(DPAD_AXIS_COUNT) { STATE_INACTIVE }
    private val dpadDirection = IntArray(DPAD_AXIS_COUNT)
    private val buttonStartMs = mutableMapOf<String, Long>()

    fun update(
        nowMs: Long,
        axes: FloatArray,
        dpadAxes: FloatArray = floatArrayOf(),
        pressedButtons: List<String>,
        gated: Boolean,
    ): Trigger? {
        if (gated) {
            reset()
            return null
        }

        val heldButtons = pressedButtons.distinct()

        for (axisIndex in 0 until AXIS_COUNT) {
            val value = axisValue(axes, axisIndex)
            val sign = value.sign()
            val magnitude = abs(value)
            val active =
                sign != 0 &&
                    magnitude >= axisSelectThreshold &&
                    heldButtons.isEmpty() &&
                    noOtherAxesActive(axes, axisIndex)

            if (!active) {
                axisStartMs[axisIndex] = STATE_INACTIVE
                axisDirection[axisIndex] = 0
                continue
            }

            val startMs = axisStartMs[axisIndex]
            when {
                startMs == STATE_TRIGGERED && axisDirection[axisIndex] == sign -> continue
                startMs == STATE_INACTIVE || axisDirection[axisIndex] != sign -> {
                    axisStartMs[axisIndex] = nowMs
                    axisDirection[axisIndex] = sign
                }
                nowMs - startMs >= longPressMs -> {
                    clearAxes()
                    axisStartMs[axisIndex] = STATE_TRIGGERED
                    axisDirection[axisIndex] = sign
                    clearButtons()
                    return Trigger.Axis(axisIndex = axisIndex, positive = sign > 0)
                }
            }
        }

        for (axisIndex in 0 until DPAD_AXIS_COUNT) {
            val value = axisValue(dpadAxes, axisIndex)
            val sign = value.sign()
            val magnitude = abs(value)
            val active =
                sign != 0 &&
                    magnitude >= axisSelectThreshold &&
                    heldButtons.isEmpty() &&
                    !hasAnyAxisAboveThreshold(axes, axisOtherThreshold) &&
                    noOtherDpadAxesActive(dpadAxes, axisIndex)

            if (!active) {
                dpadStartMs[axisIndex] = STATE_INACTIVE
                dpadDirection[axisIndex] = 0
                continue
            }

            val startMs = dpadStartMs[axisIndex]
            when {
                startMs == STATE_TRIGGERED && dpadDirection[axisIndex] == sign -> continue
                startMs == STATE_INACTIVE || dpadDirection[axisIndex] != sign -> {
                    dpadStartMs[axisIndex] = nowMs
                    dpadDirection[axisIndex] = sign
                }
                nowMs - startMs >= longPressMs -> {
                    clearDpadAxes()
                    dpadStartMs[axisIndex] = STATE_TRIGGERED
                    dpadDirection[axisIndex] = sign
                    clearAxes()
                    clearButtons()
                    return Trigger.Button(dpadButtonName(axisIndex, sign))
                }
            }
        }

        val axisBusy = hasAnyAxisAboveThreshold(axes, axisOtherThreshold)
        val dpadBusy = hasAnyAxisAboveThreshold(dpadAxes, axisOtherThreshold)
        val activeButton = heldButtons.singleOrNull()
        if (activeButton != null && !axisBusy && !dpadBusy) {
            val startMs = buttonStartMs[activeButton] ?: STATE_INACTIVE
            when {
                startMs == STATE_TRIGGERED -> Unit
                startMs == STATE_INACTIVE -> buttonStartMs[activeButton] = nowMs
                nowMs - startMs >= longPressMs -> {
                    clearButtons()
                    buttonStartMs[activeButton] = STATE_TRIGGERED
                    clearAxes()
                    clearDpadAxes()
                    return Trigger.Button(activeButton)
                }
            }
        }

        val iterator = buttonStartMs.iterator()
        while (iterator.hasNext()) {
            val (buttonName, _) = iterator.next()
            if (buttonName != activeButton || axisBusy || dpadBusy) {
                iterator.remove()
            }
        }

        return null
    }

    fun reset() {
        clearAxes()
        clearDpadAxes()
        clearButtons()
    }

    private fun clearAxes() {
        axisStartMs.fill(STATE_INACTIVE)
        axisDirection.fill(0)
    }

    private fun clearDpadAxes() {
        dpadStartMs.fill(STATE_INACTIVE)
        dpadDirection.fill(0)
    }

    private fun clearButtons() {
        buttonStartMs.clear()
    }

    private fun noOtherAxesActive(
        axes: FloatArray,
        activeAxisIndex: Int,
    ): Boolean {
        for (axisIndex in 0 until AXIS_COUNT) {
            if (axisIndex == activeAxisIndex) continue
            if (abs(axisValue(axes, axisIndex)) >= axisOtherThreshold) return false
        }
        return true
    }

    private fun noOtherDpadAxesActive(
        dpadAxes: FloatArray,
        activeAxisIndex: Int,
    ): Boolean {
        for (axisIndex in 0 until DPAD_AXIS_COUNT) {
            if (axisIndex == activeAxisIndex) continue
            if (abs(axisValue(dpadAxes, axisIndex)) >= axisOtherThreshold) return false
        }
        return true
    }

    private fun hasAnyAxisAboveThreshold(
        axes: FloatArray,
        threshold: Float,
    ): Boolean {
        for (axisIndex in 0 until AXIS_COUNT) {
            if (abs(axisValue(axes, axisIndex)) >= threshold) return true
        }
        return false
    }

    private fun axisValue(
        axes: FloatArray,
        axisIndex: Int,
    ): Float =
        if (axisIndex in
            axes.indices
        ) {
            axes[axisIndex]
        } else {
            0f
        }

    private fun Float.sign(): Int =
        when {
            this > 0f -> 1
            this < 0f -> -1
            else -> 0
        }

    private fun dpadButtonName(
        axisIndex: Int,
        sign: Int,
    ): String =
        when {
            axisIndex == 0 && sign < 0 -> "D-Left"
            axisIndex == 0 && sign > 0 -> "D-Right"
            axisIndex == 1 && sign < 0 -> "D-Up"
            else -> "D-Down"
        }

    private companion object {
        private const val AXIS_COUNT = 6
        private const val DPAD_AXIS_COUNT = 2
        private const val STATE_INACTIVE = -1L
        private const val STATE_TRIGGERED = -2L
    }
}
