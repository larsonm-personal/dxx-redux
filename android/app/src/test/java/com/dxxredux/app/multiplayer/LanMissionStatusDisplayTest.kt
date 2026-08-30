package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Test

class LanMissionStatusDisplayTest {
    @Test
    fun `self row shows local match while host still echoes finalizing`() {
        val local = report(MissionCompatibilityStatus.MATCH)
        val host = report(MissionCompatibilityStatus.FINALIZING)

        assertEquals(local, missionStatusForPlayerDisplay(true, local, host))
        assertEquals(host, missionStatusForPlayerDisplay(false, local, host))
    }

    private fun report(status: MissionCompatibilityStatus) =
        MissionStatusReport(
            revision = "revision",
            status = status,
            verifiedBytes = 100L,
            totalBytes = 100L,
            transferId = "transfer",
            attempt = 1,
        )
}
