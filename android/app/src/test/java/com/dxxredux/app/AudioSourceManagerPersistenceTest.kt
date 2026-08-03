package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import org.json.JSONObject
import java.io.File

class AudioSourceManagerPersistenceTest {
    @Test
    fun savesMultipleBinContentUris() {
        val filesDir = File("build/test-audiosrc-persistence-multibin").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = AudioSourceManager(filesDir)
        val source =
            AudioSourceManager.AudioSource(
                id = "multi-bin-saf",
                cuePath = "disc.cue",
                binPaths = listOf("disc-a.bin", "disc-b.bin"),
                discLabel = "Multi",
                discId = "unknown",
                trackCount = 12,
                audioTrackCount = 11,
                audioTrackNumbers = (2..12).toList(),
                legacyDiscId = 0L,
                binContentUris = listOf("content://disc-a", "content://disc-b"),
                cueContentUri = "content://disc-cue",
            )
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(manager, mutableListOf(source))
        }
        AudioSourceManager::class.java.getDeclaredMethod("save").apply {
            isAccessible = true
            invoke(manager)
        }

        val saved = JSONObject(File(filesDir, "audio_sources.json").readText())
        val savedSource = saved.getJSONArray("sources").getJSONObject(0)
        val savedUris = savedSource.getJSONArray("bin_content_uris")
        val savedTrackNumbers = savedSource.getJSONArray("audio_track_numbers")

        assertEquals(listOf("content://disc-a", "content://disc-b"), (0 until savedUris.length()).map(savedUris::getString))
        assertEquals((2..12).toList(), (0 until savedTrackNumbers.length()).map(savedTrackNumbers::getInt))
        assertEquals("content://disc-a", savedSource.getString("bin_content_uri"))
        assertEquals("content://disc-cue", savedSource.getString("cue_content_uri"))
        assertEquals((2..12).toList(), AudioSourceManager(filesDir).getSources().single().audioTrackNumbers)
    }
}
