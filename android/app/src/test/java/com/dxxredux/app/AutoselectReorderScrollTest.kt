package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AutoselectReorderScrollTest {
    @Test
    fun movedItemAlreadyVisibleDoesNotScroll() {
        assertNull(autoselectMovedItemScrollAnchor(targetIndex = 4, visibleIndices = listOf(2, 3, 4, 5)))
    }

    @Test
    fun movedItemAboveViewportScrollsToItem() {
        assertEquals(1, autoselectMovedItemScrollAnchor(targetIndex = 1, visibleIndices = listOf(2, 3, 4, 5)))
    }

    @Test
    fun movedItemBelowViewportKeepsItemAtBottom() {
        assertEquals(3, autoselectMovedItemScrollAnchor(targetIndex = 6, visibleIndices = listOf(2, 3, 4, 5)))
    }
}
