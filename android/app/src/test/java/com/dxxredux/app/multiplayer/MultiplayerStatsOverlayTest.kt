package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Test

class MultiplayerStatsOverlayTest {
    @Test
    fun coopContentIsExcludedOutsideCoopGames() {
        assertEquals(
            0,
            MultiplayerStatsOverlay.coopContentLineCount(
                gameMode = 0,
                playerCount = 2,
                connected = intArrayOf(1, 1),
                hasEscort = false,
            ),
        )
    }

    @Test
    fun coopContentUsesTwoLinesPerConnectedPlayerAndEscortLine() {
        assertEquals(
            9,
            MultiplayerStatsOverlay.coopContentLineCount(
                gameMode = 16,
                playerCount = 3,
                connected = intArrayOf(1, 1, 0, 1),
                hasEscort = true,
            ),
        )
    }
}
