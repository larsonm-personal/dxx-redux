package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.RandomAccessFile

class MissionLevelAdmissionTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    private val mission =
        MissionScanner.MissionInfo(
            filename = "sample",
            displayName = "Sample Mission",
            levelCount = 3,
        )

    @Test
    fun `selection must resolve uniquely to the current scan`() {
        assertEquals(mission, resolveMissionSelection(listOf(mission), "SAMPLE"))
        assertNull(resolveMissionSelection(listOf(mission), "missing"))
        assertNull(resolveMissionSelection(listOf(mission, mission.copy(displayName = "Duplicate")), "sample"))
    }

    @Test
    fun `level must be inside a known nonempty mission range`() {
        assertTrue(selectedMissionLevelIsValid(mission, 1))
        assertTrue(selectedMissionLevelIsValid(mission, 3))
        assertFalse(selectedMissionLevelIsValid(mission, 0))
        assertFalse(selectedMissionLevelIsValid(mission, 4))
        assertFalse(selectedMissionLevelIsValid(mission, Int.MAX_VALUE))
        assertFalse(selectedMissionLevelIsValid(mission.copy(levelCount = 0), 1))
        assertFalse(selectedMissionLevelIsValid(null, 1))
        assertFalse(selectedMissionLevelIsValid(mission, null))
    }

    @Test
    fun `builtin ranges match full and demo mission variants`() {
        val fullDir = temporaryFolder.newFolder("full")
        assertEquals(24, MissionScanner.builtins(fullDir, "d2").first().levelCount)

        val demoDir = temporaryFolder.newFolder("demo")
        RandomAccessFile(demoDir.resolve("d2demo.hog"), "rw").use { it.setLength(2_292_566L) }
        val demo = MissionScanner.builtins(demoDir, "d2").first()
        assertEquals("d2demo", demo.filename)
        assertEquals(3, demo.levelCount)
    }
}
