package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GamepadButtonRoutingPolicyTest {
    @Test
    fun gameplayDoesNotForwardRawJoystickButtons() {
        assertFalse(shouldForwardRawJoystickButtonToUi(isInGame = true))
    }

    @Test
    fun nonGameplayUiDoesForwardRawJoystickButtons() {
        assertTrue(shouldForwardRawJoystickButtonToUi(isInGame = false))
    }
}