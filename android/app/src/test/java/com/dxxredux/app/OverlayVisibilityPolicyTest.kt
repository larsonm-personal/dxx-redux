package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class OverlayVisibilityPolicyTest {
    @Test
    fun transientScreensOverrideEveryOverlayVisibilitySource() {
        assertFalse(
            shouldShowTouchOverlay(
                inGame = true,
                overlayEnabled = true,
                transientScreen = true,
                automap = true,
                controllerMenuOpen = true,
                settingsTrayVisible = true,
                gamePaused = true,
            ),
        )
        assertFalse(
            shouldShowPausedWarning(
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.UI),
                transientScreen = true,
                automap = false,
            ),
        )
    }

    @Test
    fun automapKeepsNavigationWithoutPausedWarning() {
        val warning =
            shouldShowPausedWarning(
                PauseState(simulationPaused = true, reasons = PauseState.MENU or PauseState.OPERATION),
                transientScreen = false,
                automap = true,
            )
        assertFalse(warning)
        assertTrue(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = false,
                transientScreen = false,
                automap = true,
                controllerMenuOpen = false,
                settingsTrayVisible = false,
                gamePaused = warning,
            ),
        )
    }

    @Test
    fun nativeMenusAndOtherModalPresentationsDoNotShowGameplayPauseWarning() {
        val states =
            listOf(
                PauseState(simulationPaused = true, reasons = PauseState.MENU),
                PauseState(simulationPaused = true, reasons = PauseState.MENU or PauseState.OPERATION),
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.GRAPHICS),
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.BACKGROUND),
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.COOP),
                PauseState(simulationPaused = false, gameFront = true),
            )
        for (pause in states) {
            assertFalse(pause.toString(), shouldShowPausedWarning(pause, transientScreen = false, automap = false))
        }
    }

    @Test
    fun explicitUserPausesStillShowWarning() {
        val states =
            listOf(
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.UI),
                PauseState(simulationPaused = true, reasons = PauseState.USER),
                PauseState(simulationPaused = true, gameFront = true, reasons = PauseState.OPERATION),
            )
        for (pause in states) {
            assertTrue(pause.toString(), shouldShowPausedWarning(pause, transientScreen = false, automap = false))
        }
    }

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
                transientScreen = false,
                automap = false,
                controllerMenuOpen = false,
                settingsTrayVisible = true,
                gamePaused = true,
            ),
        )
    }

    @Test
    fun controllerMenuKeepsOverlayVisibleWhenLauncherOverlayIsDisabled() {
        assertTrue(
            shouldShowTouchOverlay(
                inGame = true,
                overlayEnabled = false,
                transientScreen = false,
                automap = false,
                controllerMenuOpen = true,
                settingsTrayVisible = false,
                gamePaused = false,
            ),
        )
    }

    @Test
    fun settingsTrayKeepsOverlayVisibleWhenLauncherOverlayIsDisabled() {
        assertTrue(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = false,
                transientScreen = false,
                automap = false,
                controllerMenuOpen = false,
                settingsTrayVisible = true,
                gamePaused = true,
            ),
        )
    }

    @Test
    fun pausedWithoutTrayStillHidesGameplayOverlay() {
        assertFalse(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = true,
                transientScreen = false,
                automap = false,
                controllerMenuOpen = false,
                settingsTrayVisible = false,
                gamePaused = false,
            ),
        )
    }

    @Test
    fun pausedStatusKeepsOverlayVisibleWhenGameplayControlsAreDisabled() {
        assertTrue(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = false,
                transientScreen = false,
                automap = false,
                controllerMenuOpen = false,
                settingsTrayVisible = false,
                gamePaused = true,
            ),
        )
    }
}
