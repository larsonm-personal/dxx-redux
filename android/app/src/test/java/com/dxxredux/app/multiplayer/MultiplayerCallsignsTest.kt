package com.dxxredux.app.multiplayer

import org.junit.Assert.assertEquals
import org.junit.Test
import java.nio.file.Files

class MultiplayerCallsignsTest {
    @Test
    fun scanFindsPilotFilesInSharedAndActiveLocations() {
        val root = Files.createTempDirectory("callsigns").toFile()
        val filesDir = root.resolve("files")
        val activeSetDir = root.resolve("imported/sets/default")

        filesDir.resolve("d2x-redux/Players/ace.plr").writeTestFile()
        filesDir.resolve("d1x-redux/Players/Beta.PLR").writeTestFile()
        filesDir.resolve("d2x-redux/Players/coopsave.plr").writeTestFile()
        filesDir.resolve("d2x-redux/Players/toolongname.plr").writeTestFile()
        filesDir.resolve("d2x-redux/Players/bad.name.plr").writeTestFile()
        activeSetDir.resolve("Players/gamma.plr").writeTestFile()

        assertEquals(
            listOf("ace", "Beta", "gamma"),
            MultiplayerCallsigns.scan(filesDir, activeSetDir),
        )
    }

    @Test
    fun sanitizeNewCallsignKeepsEngineAllowedCharsAndLength() {
        assertEquals("Ace_12-3", MultiplayerCallsigns.sanitizeNewCallsign("Ace_12-3!!!"))
        assertEquals("TooLong1", MultiplayerCallsigns.sanitizeNewCallsign("TooLong123"))
    }

    @Test
    fun pickInitialCallsignPrefersSavedExistingCallsign() {
        val options = listOf("ace", "beta")

        assertEquals("ace", MultiplayerCallsigns.pickInitialCallsign("ACE", options))
        assertEquals("ace", MultiplayerCallsigns.pickInitialCallsign("missing", options))
        assertEquals("", MultiplayerCallsigns.pickInitialCallsign("missing", emptyList()))
    }

    @Test
    fun pickerDisplayTextPromptsCreateOnlyWhenEmpty() {
        assertEquals("create", MultiplayerCallsigns.pickerDisplayText("", emptyList()))
        assertEquals(true, MultiplayerCallsigns.pickerShowsCreatePrompt("", emptyList()))

        assertEquals("Select callsign", MultiplayerCallsigns.pickerDisplayText("", listOf("ace")))
        assertEquals(false, MultiplayerCallsigns.pickerShowsCreatePrompt("", listOf("ace")))

        assertEquals("ace", MultiplayerCallsigns.pickerDisplayText("ace", emptyList()))
        assertEquals(false, MultiplayerCallsigns.pickerShowsCreatePrompt("ace", emptyList()))
    }

    private fun java.io.File.writeTestFile() {
        parentFile?.mkdirs()
        writeText("x")
    }
}
