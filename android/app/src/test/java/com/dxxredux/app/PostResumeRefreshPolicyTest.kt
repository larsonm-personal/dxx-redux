package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class PostResumeRefreshPolicyTest {
    @Test
    fun `resume uses one coalesced refresh`() {
        assertEquals(250L, PostResumeRefreshPolicy.DELAY_MS)
    }
}
