package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TopEdgeActionButtonLayoutTest {
    @Test
    fun transientGameplayActionsStayOnTopEdgeWithoutOverlap() {
        listOf(1280 to 960, 1920 to 1080, 2400 to 1080).forEach { (width, height) ->
            val warp = TopEdgeActionButtonLayout.warp(width, height)
            val acceptJoin = TopEdgeActionButtonLayout.acceptJoin(width, height)

            assertEquals(warp.top, acceptJoin.top, 0.001f)
            assertTrue(warp.bottom < height * 0.15f)
            assertTrue(acceptJoin.bottom < height * 0.15f)
            assertTrue(warp.right < acceptJoin.left)
            assertEquals(width / 2f, (acceptJoin.left + acceptJoin.right) / 2f, 0.001f)
        }
    }

    @Test
    fun warpLeavesTheExistingExitButtonCornerClear() {
        listOf(1280 to 960, 1920 to 1080, 2400 to 1080).forEach { (width, height) ->
            val shortEdge = minOf(width, height).toFloat()
            val warp = TopEdgeActionButtonLayout.warp(width, height)

            assertTrue(warp.left >= shortEdge * 0.1f)
        }
    }
}
