package com.dxxredux.app

import androidx.core.content.ContextCompat
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class DynamicReceiverPolicyTest {
    @Test
    fun releaseRegistrationIsAppInternal() {
        assertEquals(
            ContextCompat.RECEIVER_NOT_EXPORTED,
            DynamicReceiverPolicy.appInternalOrDebugExternalFlags(debug = false),
        )
    }

    @Test
    fun debugRegistrationAllowsAdbAutomation() {
        assertEquals(
            ContextCompat.RECEIVER_EXPORTED,
            DynamicReceiverPolicy.appInternalOrDebugExternalFlags(debug = true),
        )
        assertEquals(
            ContextCompat.RECEIVER_EXPORTED,
            DynamicReceiverPolicy.debugExternalFlags(debug = true),
        )
    }

    @Test
    fun releaseOmitsDebugOnlyRegistration() {
        assertNull(DynamicReceiverPolicy.debugExternalFlags(debug = false))
    }
}
