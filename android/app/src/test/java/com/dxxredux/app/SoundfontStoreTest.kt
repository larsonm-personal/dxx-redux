package com.dxxredux.app

import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.io.IOException

class SoundfontStoreTest {
    @get:Rule val temporary = TemporaryFolder()

    @Test fun importSelectionSurvivesNewStoreAndDeduplicates() {
        val store = SoundfontStore(temporary.root)
        assertEquals("", store.selectedPath())
        val bytes = byteArrayOf(1, 2, 3)
        val font = store.import(bytes.inputStream(), "My old computer") { it.readBytes().contentEquals(bytes) }
        assertEquals(font, store.import(bytes.inputStream(), "Duplicate") { true })
        assertEquals(1, store.read().fonts.size)
        assertEquals("", store.selectedPath())
        store.select(font.id) { File(it).readBytes().contentEquals(bytes) }
        val reopened = SoundfontStore(temporary.root)
        assertEquals(font.id, reopened.read().selected)
        assertTrue(File(reopened.selectedPath()).isFile)
        reopened.select("") { it.isEmpty() }
        assertEquals("", store.selectedPath())
        assertEquals(1, store.read().fonts.size)
    }

    @Test fun invalidImportAndFailedActivationPreserveSelection() {
        val store = SoundfontStore(temporary.root)
        val font = store.import(byteArrayOf(1).inputStream(), "Working") { true }
        store.select(font.id) { true }
        try {
            store.import(byteArrayOf(2).inputStream(), "Invalid") { false }
            fail("Invalid font accepted")
        } catch (_: IOException) {
        }
        try {
            store.select("") { false }
            fail("Failed activation persisted")
        } catch (_: IOException) {
        }
        assertEquals(font.id, store.read().selected)
        assertEquals(listOf(font), store.read().fonts)
        assertFalse(File(temporary.root, "soundfonts").listFiles()!!.any { it.name.startsWith("import-") })
    }

    @Test fun interruptedCopyLeavesExistingFontAndManifestIntact() {
        val store = SoundfontStore(temporary.root)
        val font = store.import(byteArrayOf(7).inputStream(), "Existing") { true }
        val failing =
            object : java.io.InputStream() {
                override fun read(): Int = throw IOException("Provider disconnected")
            }
        try {
            store.import(failing, "Partial") { true }
            fail("Failed copy published")
        } catch (_: IOException) {
        }
        assertEquals(listOf(font), store.read().fonts)
        assertEquals(2, File(temporary.root, "soundfonts").listFiles()!!.size)
    }
}
