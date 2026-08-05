package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class SetupAutomationScrollTest {
    @Test
    fun largestScrollNodeIdsIncludesEqualLauncherPanes() {
        assertEquals(
            listOf(11, 27),
            largestScrollNodeIds(
                listOf(
                    4 to 200,
                    11 to 800,
                    27 to 800,
                    31 to 0,
                ),
            ),
        )
    }

    @Test
    fun largestScrollNodeIdsRejectsZeroAreaNodes() {
        assertEquals(emptyList<Int>(), largestScrollNodeIds(listOf(11 to 0, 27 to 0)))
    }

    @Test
    fun landscapeGesturesCoverBothLauncherPanes() {
        assertEquals(listOf(0.25f, 0.75f), scrollGestureXFractions(isLandscape = true))
        assertEquals(listOf(0.5f), scrollGestureXFractions(isLandscape = false))
    }

    @Test
    fun accessibilityScanIncludesVirtualChildrenBeyondSemanticsIds() {
        assertEquals(544, accessibilityScanMaxId(setOf(2, 44)))
        assertEquals(16383, accessibilityScanMaxId(emptySet()))
    }
}
