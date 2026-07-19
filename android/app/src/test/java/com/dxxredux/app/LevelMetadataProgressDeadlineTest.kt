package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LevelMetadataProgressDeadlineTest {
    private fun update(
        percent: Int,
        activityId: String = "route:1",
    ) = LevelMetadataCheckpointUpdate(
        MetadataLoadProgress("Planning route", percent, 100),
        activityId,
    )

    @Test
    fun fixedDeadlineStillExpiresStalledAnalysis() {
        val deadline = LevelMetadataProgressDeadline(0, 30_000, 10_000, 5)

        deadline.observe(update(0), 1_000)

        assertFalse(deadline.isExpired(29_999))
        assertTrue(deadline.isExpired(30_000))
    }

    @Test
    fun fivePercentForwardProgressExtendsDeadline() {
        val deadline = LevelMetadataProgressDeadline(0, 30_000, 10_000, 5)

        deadline.observe(update(0), 20_000)
        deadline.observe(update(5), 25_000)
        assertFalse(deadline.isExpired(34_999))

        deadline.observe(update(10), 34_000)
        assertFalse(deadline.isExpired(43_999))
        assertTrue(deadline.isExpired(44_000))
    }

    @Test
    fun lessThanFivePercentDoesNotExtendDeadline() {
        val deadline = LevelMetadataProgressDeadline(0, 30_000, 10_000, 5)

        deadline.observe(update(0), 1_000)
        deadline.observe(update(4), 29_000)

        assertTrue(deadline.isExpired(30_000))
    }

    @Test
    fun progressThatIsTooSlowDoesNotExtendDeadline() {
        val deadline = LevelMetadataProgressDeadline(0, 30_000, 10_000, 5)

        deadline.observe(update(0), 1_000)
        deadline.observe(update(5), 25_000)

        assertTrue(deadline.isExpired(30_000))
    }

    @Test
    fun completingOneTaskStartsAProgressWindowForTheNext() {
        val deadline = LevelMetadataProgressDeadline(0, 30_000, 10_000, 5)

        deadline.observe(update(100), 25_000)
        deadline.observe(update(0, "route:2"), 34_000)

        assertFalse(deadline.isExpired(43_999))
        assertTrue(deadline.isExpired(44_000))
    }
}
