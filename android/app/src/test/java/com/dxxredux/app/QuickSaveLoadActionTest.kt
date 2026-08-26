package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class QuickSaveLoadActionTest {
    @Test
    fun quickLoadPromptUsesOnlyRequestedCopy() {
        assertEquals("load quick save?", PauseOverlayStyle.QUICK_LOAD_QUESTION)
        assertEquals("yes", PauseOverlayStyle.QUICK_LOAD_YES)
        assertEquals("no", PauseOverlayStyle.QUICK_LOAD_NO)
        assertEquals(0.44f, PauseOverlayStyle.WIDTH_RATIO)
    }

    @Test
    fun quickActionsOnlyRunOnButtonPress() {
        assertTrue(isQuickSavePress(TouchBindings.META_QUICK_SAVE, pressed = true))
        assertFalse(isQuickSavePress(TouchBindings.META_QUICK_SAVE, pressed = false))
        assertFalse(isQuickSavePress(TouchBindings.META_QUICK_LOAD, pressed = true))

        assertTrue(isQuickLoadPress(TouchBindings.META_QUICK_LOAD, pressed = true))
        assertFalse(isQuickLoadPress(TouchBindings.META_QUICK_LOAD, pressed = false))
        assertFalse(isQuickLoadPress(TouchBindings.META_QUICK_SAVE, pressed = true))
    }

    @Test
    fun quickLoadPromptKeepsSharedOverlayPauseUntilDismissed() {
        assertTrue(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = false,
                quickLoadPromptVisible = true,
            ),
        )
        assertTrue(
            shouldKeepOverlayPause(
                adminTrayOpen = true,
                musicPanelVisible = false,
                quickLoadPromptVisible = false,
            ),
        )
        assertTrue(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = true,
                quickLoadPromptVisible = false,
            ),
        )
        assertFalse(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = false,
                quickLoadPromptVisible = false,
            ),
        )
    }

    @Test
    fun closedSettingsTrayReleasesSharedOverlayPause() {
        assertFalse(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = false,
                quickLoadPromptVisible = false,
            ),
        )
        assertTrue(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = true,
                quickLoadPromptVisible = false,
            ),
        )
        assertTrue(
            shouldKeepOverlayPause(
                adminTrayOpen = false,
                musicPanelVisible = false,
                quickLoadPromptVisible = true,
            ),
        )
    }
}
