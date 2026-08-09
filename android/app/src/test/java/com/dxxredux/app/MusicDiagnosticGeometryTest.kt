package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class MusicDiagnosticGeometryTest {
    @Test
    fun savedCenterIsCenteredOverPreviousAndNextButtons() {
        val geometry = musicDiagnosticGeometry(500f, 300f, 1000f, 1f)

        assertEquals(500f, (geometry.previousButtonX + geometry.nextButtonX) / 2f, 0.001f)
        assertEquals(430f, geometry.buttonGroupLeft, 0.001f)
        assertEquals(570f, geometry.buttonGroupRight, 0.001f)
        assertEquals(300f, geometry.buttonY, 0.001f)
    }
}
