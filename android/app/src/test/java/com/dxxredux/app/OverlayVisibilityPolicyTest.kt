package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class OverlayVisibilityPolicyTest {
    @Test
    fun trayVisibilityIncludesCloseGraceWhilePauseIsUnwinding() {
        assertTrue(
            settingsTrayVisibleForOverlay(
                adminTrayOpen = false,
                adminTrayPausedGame = false,
                adminTrayCloseGraceActive = true,
            ),
        )
        assertTrue(
            settingsTrayVisibleForOverlay(
                adminTrayOpen = false,
                adminTrayPausedGame = true,
                adminTrayCloseGraceActive = false,
            ),
        )
        assertTrue(
            settingsTrayVisibleForOverlay(
                adminTrayOpen = true,
                adminTrayPausedGame = false,
                adminTrayCloseGraceActive = false,
            ),
        )
        assertFalse(
            settingsTrayVisibleForOverlay(
                adminTrayOpen = false,
                adminTrayPausedGame = false,
                adminTrayCloseGraceActive = false,
            ),
        )
    }

    @Test
    fun netEventsControlRequiresMultiplayerOrPendingLaunch() {
        assertFalse(
            shouldEnableNetEventsControl(
                isMultiplayerGame = false,
                hasPendingLaunchInfo = false,
            ),
        )
        assertTrue(
            shouldEnableNetEventsControl(
                isMultiplayerGame = true,
                hasPendingLaunchInfo = false,
            ),
        )
        assertTrue(
            shouldEnableNetEventsControl(
                isMultiplayerGame = false,
                hasPendingLaunchInfo = true,
            ),
        )
    }

    @Test
    fun netStatsControlRequiresMultiplayerOrPendingLaunch() {
        assertFalse(
            shouldEnableNetStatsControl(
                isMultiplayerGame = false,
                hasPendingLaunchInfo = false,
            ),
        )
        assertTrue(
            shouldEnableNetStatsControl(
                isMultiplayerGame = true,
                hasPendingLaunchInfo = false,
            ),
        )
        assertTrue(
            shouldEnableNetStatsControl(
                isMultiplayerGame = false,
                hasPendingLaunchInfo = true,
            ),
        )
    }

    @Test
    fun standaloneAdminOverlaysHideOnlyAfterLeavingGameplayWithoutTray() {
        assertFalse(shouldHideStandaloneAdminOverlays(inGame = true, settingsTrayVisible = false))
        assertFalse(shouldHideStandaloneAdminOverlays(inGame = false, settingsTrayVisible = true))
        assertTrue(shouldHideStandaloneAdminOverlays(inGame = false, settingsTrayVisible = false))
    }

    @Test
    fun closeGracePreventsStandaloneOverlayHideDuringTrayDismissTransition() {
        val settingsTrayVisible =
            settingsTrayVisibleForOverlay(
                adminTrayOpen = false,
                adminTrayPausedGame = false,
                adminTrayCloseGraceActive = true,
            )

        assertFalse(
            shouldHideStandaloneAdminOverlays(
                inGame = false,
                settingsTrayVisible = settingsTrayVisible,
            ),
        )
    }

    @Test
    fun settingsTrayKeepsOverlayVisibleWhilePauseWindowIsFront() {
        assertTrue(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = true,
                playerDead = false,
                endlevel = false,
                automap = false,
                settingsTrayVisible = true,
            ),
        )
    }

    @Test
    fun pausedWithoutTrayStillHidesGameplayOverlay() {
        assertFalse(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = true,
                playerDead = false,
                endlevel = false,
                automap = false,
                settingsTrayVisible = false,
            ),
        )
    }
}