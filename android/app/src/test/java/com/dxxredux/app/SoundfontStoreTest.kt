package com.dxxredux.app

import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.io.IOException

class SoundfontStoreTest {
    @get:Rule val temporary = TemporaryFolder()
    private val preferences = memoryPreferences()

    @Test fun deletingInactiveFontKeepsActiveFontAndDoesNotRestartPlayback() {
        val store = SoundfontStore(temporary.root, preferences)
        val active = store.import(byteArrayOf(1).inputStream(), "Active") { true }
        val inactive = store.import(byteArrayOf(2).inputStream(), "Inactive") { true }
        store.select(active.id) { true }
        store.delete(inactive.id) { fail("Inactive deletion must not activate a synth"); false }
        assertEquals(listOf(active), store.read().fonts)
        assertEquals(active.id, store.read().selected)
        assertFalse(File(temporary.root, "soundfonts/${inactive.id}.sf2").exists())
        try {
            store.delete("") { true }
            fail("Bundled font must not be deletable")
        } catch (_: IllegalArgumentException) {
        }
    }

    @Test fun deletingActiveFontPersistsBundledSelectionAndRetainsRenderer() {
        val store = SoundfontStore(temporary.root, preferences)
        for (renderer in SoundfontStore.RENDERERS) {
            val font = store.import(byteArrayOf(8).inputStream(), "Selected") { true }
            store.select(font.id) { true }
            store.selectRenderer(renderer) { _, _ -> true }
            var activated = false
            store.delete(font.id) { path ->
                assertEquals("", path)
                assertTrue(File(temporary.root, "soundfonts/${font.id}.sf2").exists())
                activated = true
                true
            }
            assertTrue(activated)
            val reopened = SoundfontStore(temporary.root, preferences)
            assertEquals("", reopened.read().selected)
            assertEquals(renderer, reopened.read().renderer)
            assertTrue(reopened.read().fonts.isEmpty())
            assertFalse(File(temporary.root, "soundfonts/${font.id}.sf2").exists())
        }
    }

    @Test fun failedFallbackPreventsActiveFontDeletion() {
        val store = SoundfontStore(temporary.root, preferences)
        val font = store.import(byteArrayOf(5).inputStream(), "Keep me") { true }
        store.select(font.id) { true }
        try {
            store.delete(font.id) { false }
            fail("Deleted font despite failing to activate fallback")
        } catch (_: IOException) {
        }
        assertEquals(listOf(font), store.read().fonts)
        assertEquals(font.id, store.read().selected)
        assertTrue(File(store.selectedPath()).isFile)
    }

    @Test fun defaultAndBothPresetsUseAdlibWithoutDeletingImportedFonts() {
        val store = SoundfontStore(temporary.root, preferences)
        assertEquals("ymfm", store.read().renderer)
        val font = store.import(byteArrayOf(9).inputStream(), "Remembered sound") { true }
        for (preset in GameSettingsPreset.entries) {
            store.select(font.id) { true }
            store.selectRenderer("sf2") { _, _ -> true }
            preset.resetMidiPreferences(store) { path, fm -> path.isEmpty() && fm }
            val reopened = SoundfontStore(temporary.root, preferences)
            assertEquals("ymfm", reopened.read().renderer)
            assertEquals("", reopened.read().selected)
            assertEquals(listOf(font), reopened.read().fonts)
            assertTrue(File(temporary.root, "soundfonts/${font.id}.sf2").isFile)
            assertEquals("ymfm", preferences.getString(SoundfontStore.PREF_RENDERER, null))
        }
    }

    @Test fun exportedSelectionsRoundTripAndMissingAssetsFallBack() {
        val store = SoundfontStore(temporary.root, preferences)
        val font = store.import(byteArrayOf(4).inputStream(), "Saved bank") { true }
        store.select(font.id) { true }
        store.selectRenderer("sf2") { _, _ -> true }
        val exported = ConfigImportExport.exportPreferenceValues(preferences.all)
        val decoded = ConfigImportExport.decodePreferenceValues(exported)
        assertNull(decoded.error)
        val restored = memoryPreferences()
        decoded.values.forEach { (key, value) -> restored.edit().putString(key.key, value as String).commit() }
        assertEquals(store.read(), SoundfontStore(temporary.root, restored).read())
        val missing = TemporaryFolder().also { it.create() }
        try {
            assertEquals("", SoundfontStore(missing.root, restored).selectedPath())
            assertEquals("sf2", SoundfontStore(missing.root, restored).read().renderer)
        } finally {
            missing.delete()
        }
        exported.put(SoundfontStore.PREF_RENDERER, "unknown")
        assertNotNull(ConfigImportExport.decodePreferenceValues(exported).error)
        exported.put(SoundfontStore.PREF_RENDERER, "sf2").put(SoundfontStore.PREF_SOUNDFONT, "../unsafe")
        assertNotNull(ConfigImportExport.decodePreferenceValues(exported).error)
    }

    @Test fun importSelectionSurvivesNewStoreAndDeduplicates() {
        val store = SoundfontStore(temporary.root, preferences)
        assertEquals("", store.selectedPath())
        val bytes = byteArrayOf(1, 2, 3)
        val font = store.import(bytes.inputStream(), "My old computer") { it.readBytes().contentEquals(bytes) }
        assertEquals(font, store.import(bytes.inputStream(), "Duplicate") { true })
        assertEquals(1, store.read().fonts.size)
        assertEquals("", store.selectedPath())
        store.select(font.id) { File(it).readBytes().contentEquals(bytes) }
        val reopened = SoundfontStore(temporary.root, preferences)
        assertEquals(font.id, reopened.read().selected)
        assertTrue(File(reopened.selectedPath()).isFile)
        reopened.select("") { it.isEmpty() }
        assertEquals("", store.selectedPath())
        assertEquals(1, store.read().fonts.size)
    }

    @Test fun invalidImportAndFailedActivationPreserveSelection() {
        val store = SoundfontStore(temporary.root, preferences)
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

    @Test fun rendererSelectionRetainsSoundfontAndSurvivesRestart() {
        val store = SoundfontStore(temporary.root, preferences)
        val font = store.import(byteArrayOf(7).inputStream(), "My bank") { true }
        store.select(font.id) { true }
        store.selectRenderer("ymfm") { path, fm -> fm && File(path).exists() }
        val reopened = SoundfontStore(temporary.root, preferences)
        assertEquals("ymfm", reopened.read().renderer)
        assertEquals(font.id, reopened.read().selected)
        try {
            reopened.selectRenderer("sf2") { _, _ -> false }
            fail("Failed renderer activation persisted")
        } catch (_: IllegalStateException) {
        }
        assertEquals("ymfm", store.read().renderer)
        reopened.selectRenderer("sf2") { _, fm -> !fm }
        assertEquals(font.id, store.read().selected)
    }

    @Test fun interruptedCopyLeavesExistingFontAndManifestIntact() {
        val store = SoundfontStore(temporary.root, preferences)
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
