package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class DiscImportHoistTest {
    @Test
    fun portableFilenameCollisionsAndGogPairsAreDeterministic() {
        assertEquals(
            "descent2.hog",
            ambiguousLogicalImportName(listOf("root/DESCENT2.HOG", "nested/descent2.hog")),
        )
        assertEquals("groupa.pig", ambiguousLogicalImportName(listOf("a\\GROUPA.PIG", "b/groupa.pig")))
        assertNull(ambiguousLogicalImportName(listOf("a/descent2.hog", "b/groupa.pig")))

        val first = findGogPair(listOf("beta.inst", "alpha.gog", "alpha.inst", "beta.gog"))
        val second = findGogPair(listOf("beta.gog", "alpha.inst", "beta.inst", "alpha.gog"))
        assertEquals(first, second)
        assertEquals("alpha", first?.baseName)
        assertNull(findGogPair(listOf("DISC.GOG", "disc.gog", "DISC.INST")))
    }

    @Test
    fun hoistsKnownGameFilesFromNestedCdFolders() {
        val setDir = createTempDirectory("disc-import-hoist").toFile()
        val nestedDir = File(setDir, "d2data").apply { mkdirs() }
        File(nestedDir, "DESCENT2.HOG").writeBytes(byteArrayOf(1, 2, 3))
        File(nestedDir, "GROUPA.PIG").writeBytes(byteArrayOf(4, 5, 6))
        File(File(setDir, "winsetup").apply { mkdirs() }, "readme.txt").writeText("ignore")

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(2, hoisted)
        assertNotNull(setDir.listFiles()?.firstOrNull { it.name.equals("descent2.hog", ignoreCase = true) })
        assertNotNull(setDir.listFiles()?.firstOrNull { it.name.equals("groupa.pig", ignoreCase = true) })
        assertFalse(File(setDir, "d2data").exists())
        assertTrue(File(setDir, "winsetup").exists())
    }

    @Test
    fun leavesUnknownNestedFilesInPlace() {
        val setDir = createTempDirectory("disc-import-ignore").toFile()
        val nestedDir = File(setDir, "missions").apply { mkdirs() }
        val missionFile = File(nestedDir, "readme.txt").apply { writeBytes(byteArrayOf(7, 8, 9)) }

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(0, hoisted)
        assertTrue(missionFile.exists())
        assertTrue(nestedDir.exists())
    }

    @Test
    fun hoistsCustomMissionFilesFromNestedCdFolders() {
        val setDir = createTempDirectory("disc-import-missions").toFile()
        val nestedDir = File(setDir, "missions").apply { mkdirs() }
        File(nestedDir, "CUSTOM.HOG").writeBytes(byteArrayOf(1, 2, 3))
        File(nestedDir, "CUSTOM.MSN").writeText("name = Custom")
        File(nestedDir, "CUSTOM.MN2").writeText("name = Custom 2")

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(3, hoisted)
        assertTrue(File(setDir, "CUSTOM.HOG").isFile)
        assertTrue(File(setDir, "CUSTOM.MSN").isFile)
        assertTrue(File(setDir, "CUSTOM.MN2").isFile)
        assertFalse(nestedDir.exists())
    }

    @Test
    fun rejectsRootAndNestedCaseFoldedCollisionsBeforeMutation() {
        val setDir = createTempDirectory("disc-import-replace").toFile()
        File(setDir, "DESCENT2.HOG").writeBytes(byteArrayOf(1))
        val nestedDir = File(setDir, "d2data").apply { mkdirs() }
        val nestedFile = File(nestedDir, "descent2.hog").apply { writeBytes(byteArrayOf(1, 2, 3, 4)) }

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(-1, hoisted)
        assertTrue(nestedFile.exists())
        assertEquals(1, File(setDir, "DESCENT2.HOG").length())
        assertTrue(nestedDir.exists())
    }

    @Test
    fun deduplicatesIdenticalNestedMissionFiles() {
        val setDir = createTempDirectory("disc-import-identical").toFile()
        val firstDir = File(setDir, "dlotw").apply { mkdirs() }
        val secondDir = File(setDir, "levels").apply { mkdirs() }
        val content = byteArrayOf(1, 2, 3, 4)
        File(firstDir, "RATRACE.HOG").writeBytes(content)
        File(secondDir, "ratrace.hog").writeBytes(content)

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(1, hoisted)
        assertTrue(File(setDir, "RATRACE.HOG").isFile)
        assertFalse(firstDir.exists())
        assertFalse(secondDir.exists())
    }

    @Test
    fun tracksImportCompletionAndFailure() {
        SetupImportTracker.reset()
        assertEquals("idle", SetupImportTracker.snapshot().status)

        SetupImportTracker.begin("cd")
        assertEquals(SetupImportSnapshot(kind = "cd", status = "running"), SetupImportTracker.snapshot())

        SetupImportTracker.complete("cd", 17)
        assertEquals(
            SetupImportSnapshot(kind = "cd", status = "complete", resultCount = 17),
            SetupImportTracker.snapshot(),
        )

        SetupImportTracker.begin("iso")
        SetupImportTracker.complete("iso", -1)
        assertEquals("failed", SetupImportTracker.snapshot().status)
        assertEquals("extract_failed", SetupImportTracker.snapshot().error)
    }
}
