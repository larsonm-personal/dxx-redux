package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SetupIntrospectionPolicyTest {
    @Test
    fun concurrentRequestsAreCoalescedUntilCurrentRequestExits() {
        val singleFlight = SetupIntrospectionSingleFlight()

        assertTrue(singleFlight.tryEnter())
        assertFalse(singleFlight.tryEnter())
        singleFlight.exit()
        assertTrue(singleFlight.tryEnter())
    }
}
