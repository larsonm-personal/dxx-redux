package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class VideoInfoOverlayLayoutTest {
    @Test
    fun fullHeightKeepsDefaultSizes() {
        val layout =
            computeVideoInfoOverlayLayout(
                height = 720,
                density = 2f,
                baseTextSize = 22f,
                actionRows = 3,
            )

        assertEquals(22f, layout.infoTextSize)
        assertEquals(22f, layout.buttonTextSize)
        assertEquals(626f, layout.panelHeight)
        assertFits(720, layout)
    }

    @Test
    fun normalShortHeightShrinksInfoBeforeButtons() {
        val layout =
            computeVideoInfoOverlayLayout(
                height = 480,
                density = 2f,
                baseTextSize = 22f,
                actionRows = 3,
            )

        assertTrue(layout.infoTextSize < layout.buttonTextSize)
        assertEquals(22f, layout.buttonTextSize)
        assertFits(480, layout)
    }

    @Test
    fun debugShortHeightScalesButtonsAfterInfoRows() {
        val layout =
            computeVideoInfoOverlayLayout(
                height = 480,
                density = 2f,
                baseTextSize = 22f,
                actionRows = 7,
            )

        assertTrue(layout.infoTextSize < layout.buttonTextSize)
        assertTrue(layout.buttonTextSize < 22f)
        assertFits(480, layout)
    }

    @Test
    fun tinyHeightStillKeepsPanelInsideView() {
        val layout =
            computeVideoInfoOverlayLayout(
                height = 120,
                density = 3f,
                baseTextSize = 30f,
                actionRows = 7,
            )

        assertTrue(layout.infoTextSize > 0f)
        assertTrue(layout.buttonTextSize > 0f)
        assertFits(120, layout)
    }

    private fun assertFits(
        height: Int,
        layout: VideoInfoOverlayLayout,
    ) {
        val panelBottom = layout.outerPad + layout.panelHeight
        assertTrue(panelBottom <= height - layout.outerPad + 0.1f)
    }
}
