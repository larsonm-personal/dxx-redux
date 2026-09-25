package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MusicEditorArchiveSourcesTest {
    @get:Rule val temporary = TemporaryFolder()

    @Test fun importedMidiCanBeSelectedAndReadWithoutIncludingAudioOnlySources() {
        val archive = temporary.newFile("mission.zip")
        val midi =
            byteArrayOf(
                77,
                84,
                104,
                100,
                0,
                0,
                0,
                6,
                0,
                0,
                0,
                1,
                0,
                96,
                77,
                84,
                114,
                107,
                0,
                0,
                0,
                4,
                0,
                -1,
                47,
                0,
            )
        ZipOutputStream(archive.outputStream()).use { zip ->
            for ((name, bytes) in listOf("music/level.mid" to midi, "music/song.ogg" to byteArrayOf(1))) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(bytes)
                zip.closeEntry()
            }
        }
        val catalog = requireNotNull(MissionZipMusic.inspect(archive))
        val mod = ModManager.ModInfo("mission.zip", "Test mission", false, 0, archive.length(), "d1", 0)
        val source = musicEditorArchiveSources(mod, catalog).single()
        val track = source.tracks.values.single()
        assertEquals("level.mid", track.displayName)
        assertFalse(source.info.tracks.any { it.filename.endsWith("ogg") })
        assertArrayEquals(
            midi,
            MissionZipMusicStageManager(File(temporary.root, "cache")).readMidiTrackBytes(catalog, track),
        )
        val d1 = MidiEnumerationBridge.SourceInfo("d1-builtin", "Descent 1", "d1")
        val d2 = MidiEnumerationBridge.SourceInfo("d2-builtin", "Descent 2", "d2")
        val ordered = orderedMidiEditorSources(listOf(d2, source.info, d1))
        assertEquals(listOf(d1, d2, source.info), ordered)
        assertEquals(source.info, preferredMidiEditorSource(ordered, source.info.id))
        val audioOnly =
            catalog.copy(
                sources =
                    catalog.sources.map {
                        it.copy(
                            tracks =
                                it.tracks.filter { t ->
                                    t.kind !=
                                        MissionZipMusic.KIND_MIDI
                                },
                        )
                    },
            )
        assertEquals(emptyList<MusicEditorArchiveSource>(), musicEditorArchiveSources(mod, audioOnly))
    }
}
