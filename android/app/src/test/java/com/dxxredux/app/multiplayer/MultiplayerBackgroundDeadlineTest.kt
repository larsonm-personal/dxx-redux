package com.dxxredux.app.multiplayer

import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MultiplayerBackgroundDeadlineTest {
    @Test
    fun foregroundInvalidatesOldDeadlineAndGrantsFullNewInterval() {
        val deadline = MultiplayerBackgroundDeadline(1_200_000)
        val first = deadline.background(100)!!

        deadline.foreground()
        assertFalse(deadline.expire(first.first, first.second))

        val second = deadline.background(1_000_000)!!
        assertFalse(deadline.expire(second.first, second.second - 1))
        assertTrue(deadline.expire(second.first, second.second))
    }

    @Test
    fun duplicateBackgroundDoesNotExtendDeadline() {
        val deadline = MultiplayerBackgroundDeadline(1_200_000)
        val first = deadline.background(100)

        assertNotNull(first)
        assertNull(deadline.background(500_000))
        assertTrue(deadline.expire(first!!.first, first.second))
    }

    @Test
    fun deadlineExpiresOnlyOnce() {
        val deadline = MultiplayerBackgroundDeadline(1_200_000)
        val token = deadline.background(100)!!.first

        assertTrue(deadline.expire(token, 1_200_100))
        assertFalse(deadline.expire(token, 2_400_100))
    }
}
