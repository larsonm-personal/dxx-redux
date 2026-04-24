package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ControllerLongPressDetectorTest {
    @Test
    fun axisHeldAloneTriggersOncePerHold() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        axes[0] = 0.85f

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 1999, axes = axes, pressedButtons = emptyList(), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Axis(axisIndex = 0, positive = true),
            detector.update(nowMs = 2000, axes = axes, pressedButtons = emptyList(), gated = false),
        )
        assertNull(detector.update(nowMs = 2500, axes = axes, pressedButtons = emptyList(), gated = false))
    }

    @Test
    fun axisSignFlipResetsTimer() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        axes[0] = 0.9f

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = emptyList(), gated = false))
        axes[0] = -0.9f
        assertNull(detector.update(nowMs = 1000, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 2999, axes = axes, pressedButtons = emptyList(), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Axis(axisIndex = 0, positive = false),
            detector.update(nowMs = 3000, axes = axes, pressedButtons = emptyList(), gated = false),
        )
    }

    @Test
    fun secondAxisAboveThresholdBlocksAxisTrigger() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        axes[0] = 0.9f
        axes[1] = 0.31f

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 2500, axes = axes, pressedButtons = emptyList(), gated = false))
        axes[1] = 0f
        assertNull(detector.update(nowMs = 2501, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 4499, axes = axes, pressedButtons = emptyList(), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Axis(axisIndex = 0, positive = true),
            detector.update(nowMs = 4501, axes = axes, pressedButtons = emptyList(), gated = false),
        )
    }

    @Test
    fun buttonHeldAloneTriggersOncePerHold() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = listOf("A"), gated = false))
        assertNull(detector.update(nowMs = 1999, axes = axes, pressedButtons = listOf("A"), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Button("A"),
            detector.update(nowMs = 2000, axes = axes, pressedButtons = listOf("A"), gated = false),
        )
        assertNull(detector.update(nowMs = 2100, axes = axes, pressedButtons = listOf("A"), gated = false))
    }

    @Test
    fun dpadHatHeldAloneTriggersOncePerHold() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        val dpadAxes = floatArrayOf(-1f, 0f)

        assertNull(
            detector.update(
                nowMs = 0,
                axes = axes,
                dpadAxes = dpadAxes,
                pressedButtons = emptyList(),
                gated = false,
            ),
        )
        assertEquals(
            ControllerLongPressDetector.Trigger.Button("D-Left"),
            detector.update(
                nowMs = 2000,
                axes = axes,
                dpadAxes = dpadAxes,
                pressedButtons = emptyList(),
                gated = false,
            ),
        )
        assertNull(
            detector.update(
                nowMs = 2100,
                axes = axes,
                dpadAxes = dpadAxes,
                pressedButtons = emptyList(),
                gated = false,
            ),
        )
    }

    @Test
    fun buttonHoldBlockedByOtherActivity() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        axes[0] = 0.31f

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = listOf("A"), gated = false))
        assertNull(detector.update(nowMs = 2500, axes = axes, pressedButtons = listOf("A"), gated = false))
        axes[0] = 0f
        assertNull(detector.update(nowMs = 2501, axes = axes, pressedButtons = listOf("A", "B"), gated = false))
        assertNull(detector.update(nowMs = 2502, axes = axes, pressedButtons = listOf("A"), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Button("A"),
            detector.update(nowMs = 4502, axes = axes, pressedButtons = listOf("A"), gated = false),
        )
    }

    @Test
    fun gatedStateClearsPendingTrigger() {
        val detector = ControllerLongPressDetector()
        val axes = FloatArray(6)
        axes[0] = 0.9f

        assertNull(detector.update(nowMs = 0, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 1500, axes = axes, pressedButtons = emptyList(), gated = true))
        assertNull(detector.update(nowMs = 1501, axes = axes, pressedButtons = emptyList(), gated = false))
        assertNull(detector.update(nowMs = 3499, axes = axes, pressedButtons = emptyList(), gated = false))
        assertEquals(
            ControllerLongPressDetector.Trigger.Axis(axisIndex = 0, positive = true),
            detector.update(nowMs = 3501, axes = axes, pressedButtons = emptyList(), gated = false),
        )
    }
}