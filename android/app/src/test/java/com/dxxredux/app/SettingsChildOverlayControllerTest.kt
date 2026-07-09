package com.dxxredux.app

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Test

class SettingsChildOverlayControllerTest {
    @Test
    fun moveLinearSelection_staysWithinBounds() {
        assertEquals(0, moveLinearSelection(0, 3, KeyEvent.KEYCODE_DPAD_UP))
        assertEquals(1, moveLinearSelection(0, 3, KeyEvent.KEYCODE_DPAD_DOWN))
        assertEquals(1, moveLinearSelection(2, 3, KeyEvent.KEYCODE_DPAD_LEFT))
        assertEquals(2, moveLinearSelection(2, 3, KeyEvent.KEYCODE_DPAD_RIGHT))
    }

    @Test
    fun scrollOffsetToKeepRowVisible_scrollsOnlyWhenNeeded() {
        assertEquals(0f, scrollOffsetToKeepRowVisible(0f, 0, 20f, 60f), 0.001f)
        assertEquals(20f, scrollOffsetToKeepRowVisible(0f, 3, 20f, 60f), 0.001f)
        assertEquals(20f, scrollOffsetToKeepRowVisible(40f, 1, 20f, 60f), 0.001f)
    }

    @Test
    fun videoInfoControllerActions_omitDebugControlsWhenDisabled() {
        assertEquals(
            listOf(
                VideoInfoControllerAction.TEX_FILT,
                VideoInfoControllerAction.ANISO,
                VideoInfoControllerAction.MSAA,
            ),
            videoInfoControllerActions(showDebugControls = false),
        )
    }

    @Test
    fun videoInfoControllerActions_includeDebugControlsWhenEnabled() {
        assertEquals(
            listOf(
                VideoInfoControllerAction.TEX_FILT,
                VideoInfoControllerAction.ANISO,
                VideoInfoControllerAction.MSAA,
                VideoInfoControllerAction.MERGED_WALL,
                VideoInfoControllerAction.MERGED_WALL_TAP,
                VideoInfoControllerAction.LABELS,
            ),
            videoInfoControllerActions(showDebugControls = true),
        )
    }

    @Test
    fun nextTouchEditorToolbarFocusIndex_skipsDisabledSaveSlot() {
        assertEquals(0, nextTouchEditorToolbarFocusIndex(7, saveEnabled = false, direction = 1))
        assertEquals(7, nextTouchEditorToolbarFocusIndex(0, saveEnabled = false, direction = -1))
        assertEquals(8, nextTouchEditorToolbarFocusIndex(7, saveEnabled = true, direction = 1))
    }

    @Test
    fun shouldCloseControllerSettingsStackForMenu_requiresPressedSettingsSurface() {
        assertEquals(
            true,
            shouldCloseControllerSettingsStackForMenu(
                pressed = true,
                settingsRootVisible = true,
                musicPanelVisible = false,
                videoInfoVisible = false,
            ),
        )
        assertEquals(
            true,
            shouldCloseControllerSettingsStackForMenu(
                pressed = true,
                settingsRootVisible = false,
                musicPanelVisible = true,
                videoInfoVisible = false,
            ),
        )
        assertEquals(
            true,
            shouldCloseControllerSettingsStackForMenu(
                pressed = true,
                settingsRootVisible = false,
                musicPanelVisible = false,
                videoInfoVisible = true,
            ),
        )
        assertEquals(
            false,
            shouldCloseControllerSettingsStackForMenu(
                pressed = false,
                settingsRootVisible = true,
                musicPanelVisible = true,
                videoInfoVisible = true,
            ),
        )
        assertEquals(
            false,
            shouldCloseControllerSettingsStackForMenu(
                pressed = true,
                settingsRootVisible = false,
                musicPanelVisible = false,
                videoInfoVisible = false,
            ),
        )
    }
}
