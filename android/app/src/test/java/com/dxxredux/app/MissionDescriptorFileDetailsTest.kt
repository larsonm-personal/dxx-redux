package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File

class MissionDescriptorFileDetailsTest {
    @Test
    fun parsesStandaloneMn2DetailsFromSetDirectory() {
        val setDir = File("build/test-mission-descriptor-details").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()
        File(setDir, "Uneasy4.mn2").writeText(
            """
            name = Uneasy 4
            type = normal
            num_levels = 1
            Uneasy4.rl2
            author = Blarget 2 and Nightsurfer
            editor = Inferno 1.0.22
            """.trimIndent(),
        )

        val details =
            missionDescriptorForStatus(
                FileStatus(
                    info = GameFileInfo("Uneasy4.mn2", "Descent 2 mission descriptor", required = false),
                    found = true,
                    foundName = "Uneasy4.mn2",
                ),
                setDir,
            )

        assertEquals("Uneasy 4", details?.displayName)
        assertEquals("normal", details?.type)
        assertEquals("Blarget 2 and Nightsurfer", details?.author)
        assertEquals("Inferno 1.0.22", details?.editor)
        assertEquals(listOf("Uneasy4.rl2"), details?.levelNames)
        assertEquals("d2", details?.game)
    }

    @Test
    fun ignoresNonMissionDescriptorFiles() {
        val setDir = File("build/test-mission-descriptor-details-ignore").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()

        val details =
            missionDescriptorForStatus(
                FileStatus(
                    info = GameFileInfo("descent2.hog", "Main game data", required = true),
                    found = true,
                    foundName = "descent2.hog",
                ),
                setDir,
            )

        assertNull(details)
    }
}
