package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PlayGamesFeatureStateTest {
    @Test
    fun defaultOffAndRestoredConsentDoNotImplyAuthenticatedProfile() {
        val fresh = PlayGamesFeatureGate(true)
        assertFalse(fresh.state.enabled)
        assertFalse(fresh.state.ready)
        val restored = PlayGamesFeatureGate(true, true)
        assertTrue(restored.state.enabled)
        assertFalse(restored.state.ready)
        val unavailable = PlayGamesFeatureGate(false, true)
        unavailable.publish(unavailable.begin(), "profile")
        assertFalse(unavailable.state.available)
        assertFalse(unavailable.state.enabled)
        assertFalse(unavailable.state.ready)
    }

    @Test
    fun optOutAndNewProfileAttemptInvalidateEveryOlderCallback() {
        val gate = PlayGamesFeatureGate(true)
        val first = gate.begin()
        gate.publish(first, "profile-one")
        assertTrue(gate.state.ready)
        gate.disable()
        assertFalse(gate.isCurrent(first))
        gate.publish(first, "late-profile")
        assertEquals(null, gate.state.profileId)
        val second = gate.begin()
        gate.publish(first, "old-profile")
        assertEquals(null, gate.state.profileId)
        gate.publish(second, "profile-two")
        assertEquals("profile-two", gate.state.profileId)
        val refresh = gate.begin()
        assertFalse(gate.state.ready)
        gate.publish(second, null)
        gate.publish(refresh, " ")
        assertFalse(gate.state.ready)
    }
}
