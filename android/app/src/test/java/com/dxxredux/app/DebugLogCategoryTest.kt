package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class DebugLogCategoryTest {
    @Test
    fun categoryCountMatchesLabels() {
        assertEquals(DebugLogCategory.COUNT, DebugLogCategory.labels.size)
        assertEquals("Dormancy", DebugLogCategory.labels[DebugLogCategory.DORMANCY])
        assertEquals("Guide-Bot", DebugLogCategory.labels[DebugLogCategory.GUIDEBOT])
    }
}
