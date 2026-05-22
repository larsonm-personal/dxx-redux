package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class DiscImportHoistTest {
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
        val missionFile = File(nestedDir, "level1.rl2").apply { writeBytes(byteArrayOf(7, 8, 9)) }

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(0, hoisted)
        assertTrue(missionFile.exists())
        assertTrue(nestedDir.exists())
    }

    @Test
    fun replacesSmallerRootGameFilesWithLargerNestedCopies() {
        val setDir = createTempDirectory("disc-import-replace").toFile()
        File(setDir, "DESCENT2.HOG").writeBytes(byteArrayOf(1))
        val nestedDir = File(setDir, "d2data").apply { mkdirs() }
        val nestedFile = File(nestedDir, "descent2.hog").apply { writeBytes(byteArrayOf(1, 2, 3, 4)) }

        val hoisted = hoistNestedImportedGameFiles(setDir)

        assertEquals(1, hoisted)
        assertFalse(nestedFile.exists())
        assertEquals(4, File(setDir, "DESCENT2.HOG").length())
        assertFalse(nestedDir.exists())
    }
}