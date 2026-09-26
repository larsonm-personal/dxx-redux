package com.dxxredux.app

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Test

class ControllerMenuAxesTest {
    @Test
    fun eitherStickNavigatesAndNeutralReleasesWithoutDuplicateSteps() {
        val axes = ControllerMenuAxes()
        val edges = mutableListOf<Pair<Int, Boolean>>()
        val send: (Int, Boolean) -> Unit = { key, down -> edges.add(key to down) }
        axes.update(0f, 0.4f, 0f, 0f, 0f, 0f, send)
        axes.update(0f, 0.8f, 0f, 0f, 0f, 0f, send)
        axes.update(0f, 1f, 0f, 0f, 0f, 0f, send)
        axes.update(0f, 0f, 0f, -1f, 0f, 0f, send)
        axes.reset(send)
        assertEquals(
            listOf(KeyEvent.KEYCODE_DPAD_DOWN to true, KeyEvent.KEYCODE_DPAD_DOWN to false,
                KeyEvent.KEYCODE_DPAD_UP to true, KeyEvent.KEYCODE_DPAD_UP to false),
            edges,
        )
    }

    @Test
    fun hatWinsOverOpposingSticks() {
        val axes = ControllerMenuAxes()
        val edges = mutableListOf<Pair<Int, Boolean>>()
        axes.update(-1f, 0f, -1f, 0f, 1f, 0f) { key, down -> edges.add(key to down) }
        assertEquals(listOf(KeyEvent.KEYCODE_DPAD_RIGHT to true), edges)
    }
}
