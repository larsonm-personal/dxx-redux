package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class MusicSourcePreferenceTest {
    private val d1 = MidiEnumerationBridge.SourceInfo("d1-builtin", "Descent 1", "d1")
    private val d2 = MidiEnumerationBridge.SourceInfo("d2-builtin", "Descent 2", "d2")
    private val mission = MidiEnumerationBridge.SourceInfo("d2-mission-test", "Test mission", "d2")
    private val sources = listOf(d2, mission, d1)

    @Test fun firstOpeningPrefersD1RegardlessOfEnumerationOrder() {
        assertEquals(d1, preferredMidiEditorSource(sources, null))
    }

    @Test fun savedSelectionSurvivesSettingsRoundTripAndSourceReordering() {
        for (source in sources) {
            val json = ConfigImportExport.exportPreferenceValues(mapOf(PREF_MIDI_EDITOR_SOURCE to source.id))
            val decoded = ConfigImportExport.decodePreferenceValues(json)
            assertNull(decoded.error)
            val saved = decoded.values.entries.single { it.key.key == PREF_MIDI_EDITOR_SOURCE }.value as String
            assertEquals(source, preferredMidiEditorSource(sources.reversed(), saved))
        }
    }

    @Test fun missingPreferenceFallsBackToInstalledSources() {
        assertEquals(d1, preferredMidiEditorSource(sources, "removed-mission"))
        assertEquals(d2, preferredMidiEditorSource(listOf(d2), "d1-builtin"))
        assertEquals(mission, preferredMidiEditorSource(listOf(mission), null))
        assertNull(preferredMidiEditorSource(emptyList(), "d1-builtin"))
    }
}
