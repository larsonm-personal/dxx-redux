package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class MusicDiagnosticGeometryTest {
    @Test
    fun savedCenterIsCenteredOverPreviousAndNextButtons() {
        val geometry = musicDiagnosticGeometry(500f, 300f, 1000f, 1000f, 1f)

        assertEquals(500f, (geometry.previousButtonX + geometry.nextButtonX) / 2f, 0.001f)
        assertEquals(430f, geometry.buttonGroupLeft, 0.001f)
        assertEquals(570f, geometry.buttonGroupRight, 0.001f)
        assertEquals(300f, geometry.buttonY, 0.001f)
    }

    @Test
    fun fullTargetSurfaceKeepsPreviewAndRuntimeSizesEqual() {
        val preview = musicDiagnosticGeometry(500f, 300f, 1920f, 1080f, 1f)
        val runtime = musicDiagnosticGeometry(960f, 540f, 1920f, 1080f, 1f)

        assertEquals(runtime.buttonRadius, preview.buttonRadius, 0.001f)
        assertEquals(runtime.arrowTextSize, preview.arrowTextSize, 0.001f)
        assertEquals(runtime.labelTextSize, preview.labelTextSize, 0.001f)
    }
}
