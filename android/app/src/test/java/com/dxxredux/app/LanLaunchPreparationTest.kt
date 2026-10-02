package com.dxxredux.app

import com.dxxredux.app.lobby.LanLaunchPreparation
import com.dxxredux.app.lobby.LanLaunchStage
import com.dxxredux.app.lobby.acceptLanLaunchUpdate
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LanLaunchPreparationTest {
    private fun state(
        attempt: Long,
        stage: LanLaunchStage,
    ) = LanLaunchPreparation("lobby", "host", attempt, stage)

    @Test
    fun lossReorderingAndRetriesCannotUndoDecisions() {
        val prepare = state(1, LanLaunchStage.PREPARING)
        val commit = state(1, LanLaunchStage.COMMITTED)
        val abort = state(1, LanLaunchStage.CANCELLED)
        assertTrue(acceptLanLaunchUpdate(null, commit)) // Lost PREPARE
        assertTrue(acceptLanLaunchUpdate(null, abort)) // Abort arrives before PREPARE
        assertFalse(acceptLanLaunchUpdate(prepare, prepare))
        assertTrue(acceptLanLaunchUpdate(prepare, commit))
        assertTrue(acceptLanLaunchUpdate(prepare, abort))
        assertFalse(acceptLanLaunchUpdate(commit, prepare))
        assertFalse(acceptLanLaunchUpdate(commit, abort))
        assertFalse(acceptLanLaunchUpdate(abort, prepare))
        assertFalse(acceptLanLaunchUpdate(abort, commit))
        assertTrue(acceptLanLaunchUpdate(abort, state(2, LanLaunchStage.PREPARING)))
        assertFalse(acceptLanLaunchUpdate(state(2, LanLaunchStage.PREPARING), commit))
        assertTrue(acceptLanLaunchUpdate(commit, prepare.copy(lobbyId = "new-lobby")))
    }
}
