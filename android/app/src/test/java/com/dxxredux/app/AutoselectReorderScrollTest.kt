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

    @Test
    fun grabbedItemMovesByDirectionWithinList() {
        assertEquals(3, autoselectGrabbedItemMoveTarget(grabbedIndex = 2, itemCount = 5, direction = 1))
        assertEquals(1, autoselectGrabbedItemMoveTarget(grabbedIndex = 2, itemCount = 5, direction = -1))
    }

    @Test
    fun grabbedItemStaysInsideListAtEdges() {
        assertEquals(0, autoselectGrabbedItemMoveTarget(grabbedIndex = 0, itemCount = 5, direction = -1))
        assertEquals(4, autoselectGrabbedItemMoveTarget(grabbedIndex = 4, itemCount = 5, direction = 1))
    }

    @Test
    fun grabbedItemMoveTargetIgnoresMissingGrab() {
        assertNull(autoselectGrabbedItemMoveTarget(grabbedIndex = -1, itemCount = 5, direction = 1))
    }

    @Test
    fun cancelMoveReturnsGrabbedItemToOriginalIndex() {
        assertEquals(1, autoselectCancelMoveTarget(grabbedIndex = 3, originalIndex = 1, itemCount = 5))
        assertEquals(4, autoselectCancelMoveTarget(grabbedIndex = 3, originalIndex = 9, itemCount = 5))
    }

    @Test
    fun cancelMoveTargetIgnoresMissingGrab() {
        assertNull(autoselectCancelMoveTarget(grabbedIndex = -1, originalIndex = 1, itemCount = 5))
        assertNull(autoselectCancelMoveTarget(grabbedIndex = 3, originalIndex = -1, itemCount = 5))
    }
}
