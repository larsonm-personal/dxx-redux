package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Test

class MissionVariantPolicyTest {
    @Test
    fun precedenceAndSupportAreSharedWithDesktop() {
        assertEquals(listOf("rebirth", "dos", "d2x", "d2xxl"), MISSION_VARIANT_MASK_PRECEDENCE.map { it.id })
        assertFalse(MISSION_VARIANT_MASK_PRECEDENCE.last().supportedByRedux)
    }

    @Test
    fun duplicatePreferredVariantIsAmbiguous() {
        val selected = selectPreferredMissionVariant(listOf("REBIRTH", "rebirth"), ::missionVariantForDirectoryName)
        assertNull(selected)
    }
}
