package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class MissionArchiveProgressFormatTest {
    @Test
    fun countProgressDoesNotUseByteUnits() {
        assertEquals(
            "4 / 8",
            formatMissionArchiveProgressAmount("Identifying music tracks: Mission (game04.ogg)", 4L, 8L),
        )
    }

    @Test
    fun byteProgressStillUsesSizeUnits() {
        assertEquals(
            "4 B / 8 B",
            formatMissionArchiveProgressAmount("Copying level pack: Mission.zip", 4L, 8L),
        )
    }
}
