package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class GogAudioSourceRegistrationTest {
    @Test
    fun registersGogInstPairWithActualFilenames() {
        val filesDir = File("build/test-gog-audio-register-pair").absoluteFile
        val setDir = File(filesDir, "sets/default")
        filesDir.deleteRecursively()
        setDir.mkdirs()
        File(setDir, "CUSTOM_DISC.GOG").writeText("bin")
        File(setDir, "CUSTOM_DISC.INST").writeText(
            "FILE \"CUSTOM_DISC.GOG\" BINARY\n" +
                "  TRACK 01 MODE1/2352\n" +
                "    INDEX 01 00:00:00\n" +
                "  TRACK 02 AUDIO\n" +
                "    INDEX 01 00:01:00\n",
        )

        val source = buildGogAudioSource(filesDir, setDir)!!
        assertEquals("gog-custom_disc", source.id)
        assertEquals("sets/default/CUSTOM_DISC.INST", source.cuePath.replace('\\', '/'))
        assertEquals(listOf("sets/default/CUSTOM_DISC.GOG"), source.binPaths.map { it.replace('\\', '/') })
        assertEquals(2, source.trackCount)
        assertEquals(1, source.audioTrackCount)
    }

    @Test
    fun createsKnownD2CueWhenMacPkgOnlyHasGogImage() {
        val filesDir = File("build/test-gog-audio-register-synthetic").absoluteFile
        val setDir = File(filesDir, "sets/default")
        filesDir.deleteRecursively()
        setDir.mkdirs()
        File(setDir, "DESCENT_II.gog").writeText("bin")

        val generatedCue = File(setDir, "DESCENT_II.inst")
        assertNull(findGogPair(setDir))
        val source = buildGogAudioSource(filesDir, setDir)!!
        assertTrue(generatedCue.isFile)
        assertTrue(generatedCue.readText().contains("FILE \"DESCENT_II.gog\" BINARY"))
        assertEquals("d2-gog-v1.2", source.id)
        assertEquals("sets/default/DESCENT_II.inst", source.cuePath.replace('\\', '/'))
        assertEquals(listOf("sets/default/DESCENT_II.gog"), source.binPaths.map { it.replace('\\', '/') })
        assertEquals(9, source.trackCount)
        assertEquals(8, source.audioTrackCount)
    }
}
