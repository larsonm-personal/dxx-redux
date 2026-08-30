package com.dxxredux.app.lobby

import com.dxxredux.app.multiplayer.MissionCompatibilityStatus
import com.dxxredux.app.multiplayer.MissionStatusReport
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LobbyMissionRefreshTest {
    @Test
    fun `refresh does not resolve over an active transfer`() {
        val activeStatuses =
            listOf(
                MissionCompatibilityStatus.QUEUED,
                MissionCompatibilityStatus.DOWNLOADING,
                MissionCompatibilityStatus.PAUSED,
                MissionCompatibilityStatus.RETRYING,
                MissionCompatibilityStatus.FAILED_RESUMABLE,
                MissionCompatibilityStatus.VERIFYING,
                MissionCompatibilityStatus.FINALIZING,
            )

        activeStatuses.forEach { status ->
            assertFalse(LobbyService.shouldResolveMissionAfterRefresh(report(status)))
        }
    }

    @Test
    fun `refresh resolves a mission that is still missing`() {
        assertTrue(
            LobbyService.shouldResolveMissionAfterRefresh(
                report(MissionCompatibilityStatus.MISSING),
            ),
        )
    }

    @Test
    fun `host accepts phase transitions and terminal failure after full verification`() {
        val verifying = report(MissionCompatibilityStatus.VERIFYING, verifiedBytes = 100L)
        val finalizing = report(MissionCompatibilityStatus.FINALIZING, verifiedBytes = 100L)

        assertTrue(
            LobbyService.shouldAcceptMissionStatus(
                verifying,
                report(MissionCompatibilityStatus.FINALIZING, verifiedBytes = 0L),
            ),
        )
        assertTrue(
            LobbyService.shouldAcceptMissionStatus(
                verifying,
                report(MissionCompatibilityStatus.FAILED_RESUMABLE, verifiedBytes = 0L),
            ),
        )
        assertTrue(
            LobbyService.shouldAcceptMissionStatus(
                finalizing,
                report(MissionCompatibilityStatus.MATCH, verifiedBytes = 100L),
            ),
        )
        assertFalse(
            LobbyService.shouldAcceptMissionStatus(
                report(MissionCompatibilityStatus.MATCH, verifiedBytes = 100L),
                finalizing,
            ),
        )
        assertFalse(
            LobbyService.shouldAcceptMissionStatus(
                verifying,
                report(MissionCompatibilityStatus.VERIFYING, verifiedBytes = 99L),
            ),
        )
    }

    private fun report(
        status: MissionCompatibilityStatus,
        verifiedBytes: Long = 0L,
    ) = MissionStatusReport(
        revision = "revision",
        status = status,
        verifiedBytes = verifiedBytes,
        totalBytes = 100L,
        transferId = "transfer",
        attempt = 1,
    )
}
