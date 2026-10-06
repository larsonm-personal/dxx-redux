package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class GraphicsCapabilitiesTest {
    @get:Rule val temp = TemporaryFolder()

    private fun report(
        depth: Int = 0,
        aniso: Int = 8,
        two: Int = 4,
        four: Int = 4,
    ): File {
        val file = File(temp.root, "graphics-capabilities-$depth.json")
        file.writeText(
            """{"schema":1,"fingerprint":"device-driver","color_depth":$depth,
            "aniso_max":$aniso,"msaa_2":$two,"msaa_4":$four,
            "aniso_reason":"AF is not supported","msaa_reason":"No shared sample count","renderer":"Test GPU"}""",
        )
        return file
    }

    @Test
    fun contextRecoveryHasBoundedExtraTimeWithoutChangingLiveRecovery() {
        val state =
            JSONObject(
                """{"accepted":{"ResolutionX":640,"ResolutionY":480,"ColorDepth":0},
            "candidate":{"ResolutionX":640,"ResolutionY":480,"ColorDepth":0,"MsaaLevel":4}}""",
            )
        assertEquals(3000L, graphicsRestoreTimeoutMs(state))
        state.getJSONObject("candidate").put("ResolutionX", 800)
        assertEquals(10000L, graphicsRestoreTimeoutMs(state))
        state.getJSONObject("candidate").put("ResolutionX", 640).put("ColorDepth", 1)
        assertEquals(10000L, graphicsRestoreTimeoutMs(state))
        state.getJSONObject("candidate").put("ColorDepth", 0).put("AspectX", 3)
        assertEquals(10000L, graphicsRestoreTimeoutMs(state))
        assertEquals(3000L, graphicsRestoreTimeoutMs(JSONObject()))
    }

    @Test fun unknownOrStaleSupportDoesNotDisableOptions() {
        assertNull(GraphicsCapabilities.read(temp.root, 0, "device-driver"))
        report()
        assertNull(GraphicsCapabilities.read(temp.root, 1, "device-driver"))
        assertNull(GraphicsCapabilities.read(temp.root, 0, "updated-driver"))
        report().writeText("partial")
        assertNull(GraphicsCapabilities.read(temp.root, 0, "device-driver"))
    }

    @Test fun roundedSamplesAndPartialLimitsAreExplained() {
        report()
        val caps = GraphicsCapabilities.read(temp.root, 0, "device-driver")!!
        assertTrue(caps.supportsMsaa(2))
        assertTrue(caps.msaaDetail().contains("2x uses 4x"))
        assertTrue(caps.supportsAniso(8))
        assertFalse(caps.supportsAniso(16))
        assertTrue(caps.anisoDetail().contains("up to 8x"))
        assertEquals(listOf(0, 2, 4), GraphicsOptionChoices.msaa(caps))
        assertEquals(listOf(0, 2, 4, 8), GraphicsOptionChoices.anisotropy(caps.anisoMax))
        assertEquals(0, GraphicsOptionChoices.next(GraphicsOptionChoices.msaa(caps), 4))
        assertEquals(listOf(0), GraphicsOptionChoices.msaa(null))
        assertEquals(listOf(0, 4), GraphicsOptionChoices.msaa(caps.copy(msaa2 = 0)))
        assertEquals(listOf(0, 2, 4, 8, 16), GraphicsOptionChoices.anisotropy(64))
    }

    @Test fun unsupportedFeaturesRetainOffAndExplainWhy() {
        report(aniso = 1, two = 0, four = 0)
        val caps = GraphicsCapabilities.read(temp.root, 0, "device-driver")!!
        assertFalse(caps.supportsMsaa(2))
        assertFalse(caps.supportsAniso(2))
        assertTrue(caps.supportsMsaa(0))
        assertTrue(caps.supportsAniso(0))
        assertEquals("No shared sample count", caps.msaaDetail())
        assertEquals("AF is not supported", caps.anisoDetail())
        assertEquals(listOf(VideoInfoControllerAction.TEX_FILT), videoInfoControllerActions(false, false, false))
        assertFalse(videoInfoControllerActions(true, false, false).contains(VideoInfoControllerAction.MSAA))
    }
}
