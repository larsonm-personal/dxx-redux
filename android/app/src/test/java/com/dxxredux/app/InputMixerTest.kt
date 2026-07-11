package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class InputMixerTest {
    @Test
    fun axesCombineClampAndDispatchOnlyChanges() {
        val events = mutableListOf<Triple<Int, Float, Boolean>>()
        val mixer = InputMixer({ _, _ -> }, { axis, value, touch -> events += Triple(axis, value, touch) })

        mixer.setAxis(6, "touch:stick", 0.6f)
        mixer.setAxis(6, "gyro", 0.7f)
        mixer.setAxis(6, "gyro", 0.7f)
        mixer.clearSources("touch")

        assertEquals(
            listOf(
                Triple(6, 0.6f, true),
                Triple(6, 1f, true),
                Triple(6, 0.7f, false),
            ),
            events,
        )
    }

    @Test
    fun releaseAllReleasesActiveButtonsAndAxes() {
        val buttons = mutableListOf<Pair<Int, Int>>()
        val axes = mutableListOf<Triple<Int, Float, Boolean>>()
        val mixer = InputMixer({ button, pressed -> buttons += button to pressed },
            { axis, value, touch -> axes += Triple(axis, value, touch) })

        mixer.setButton(12, "touch", true)
        mixer.setAxis(2, "ctrl", 0.5f)
        mixer.releaseAll()

        assertEquals(listOf(12 to 1, 12 to 0), buttons)
        assertEquals(listOf(Triple(2, 0.5f, false), Triple(2, 0f, false)), axes)
    }

    @Test
    fun axisSourcesRemainIsolated() {
        val events = mutableListOf<Triple<Int, Float, Boolean>>()
        val mixer = InputMixer({ _, _ -> }, { axis, value, touch -> events += Triple(axis, value, touch) })

        mixer.setAxis(0, "ctrl", 0.25f)
        mixer.setAxis(1, "ctrl", -0.5f)

        assertEquals(
            listOf(Triple(0, 0.25f, false), Triple(1, -0.5f, false)),
            events,
        )
    }

    @Test
    fun producerVectorsMixAndDispatchAsOneBatch() {
        val singles = mutableListOf<Triple<Int, Float, Boolean>>()
        val batches = mutableListOf<List<Triple<Int, Float, Boolean>>>()
        val mixer =
            InputMixer(
                { _, _ -> },
                { axis, value, touch -> singles += Triple(axis, value, touch) },
                { axes, values, touches ->
                    batches += axes.indices.map { Triple(axes[it], values[it], touches[it]) }
                },
            )

        mixer.setAxis(0, "touch:stick", 0.25f)
        mixer.setAxes("ctrl", mapOf(1 to -0.5f, 0 to 0.5f))
        mixer.setAxes("ctrl", mapOf(0 to 0.5f, 1 to -0.5f))
        mixer.clearSources("ctrl")

        assertEquals(listOf(Triple(0, 0.25f, true)), singles)
        assertEquals(
            listOf(
                listOf(Triple(0, 0.75f, true), Triple(1, -0.5f, false)),
                listOf(Triple(0, 0.25f, true), Triple(1, 0f, false)),
            ),
            batches,
        )
    }
}
