package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class AboutBuildLineTest {
    @Test
    fun debugBuildAddsMarker() {
        assertEquals("Dev Build debug", formatAboutBuildLine("dev", "dev", "dev", true))
        assertEquals("Build 123 (abc4567) internal debug", formatAboutBuildLine("internal", "123", "abc4567", true))
    }

    @Test
    fun releaseBuildLineIsUnchanged() {
        assertEquals("Build 123 (abc4567) release", formatAboutBuildLine("release", "123", "abc4567", false))
    }
}
