package com.dxxredux.app

import android.content.Context
import android.media.MediaPlayer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest

/** Normal-APK automation of the same imported music/catalog APIs used by the launcher */
internal suspend fun testHogLauncher(
    context: Context,
    step: JSONObject,
) = withContext(Dispatchers.IO) {
    check(BuildConfig.DEBUG)
    val manager = ModManager.forActiveSet(context.filesDir)
    val mod = manager.listMods().single { it.filename == step.getString("mod_file") }
    val source = File(manager.importDirectory(), mod.filename)
    val before = hogTestHash(source)
    val fileSets = FileSetManager(context.filesDir)
    val details = manager.getModDetails(mod, fileSets.getSetDir(fileSets.getActive()))
    val catalog = requireNotNull(details.missionZipMusic) { "Imported HOG has no music catalog" }
    val tracks = catalog.sources.flatMap { it.tracks }
    val midiName = step.getString("midi_name")
    val audioName = step.getString("audio_name")
    val midi = tracks.single { it.hogEntryName == midiName }
    val audio = tracks.single { it.hogEntryName == audioName }
    check(manager.hasEnabledMissionZipSoundtrack("d2"))
    val summary =
        requireNotNull(GameFileMetadata.summarizeZipConstituent(source, step.getString("hog_path"), "fixture.hog"))
    check(summary.problems.isEmpty()) { summary.problems.joinToString() }
    check(summary.contents.size == step.getInt("members"))
    check(summary.contents.any { it.name == midiName && it.sizeBytes == midi.sizeBytes })
    check(summary.contents.any { it.name == audioName && it.sizeBytes == audio.sizeBytes })

    val scratch = File(context.cacheDir, "hog-launcher-integration")
    check(!scratch.exists()) { "Previous HOG integration scratch requires inspection" }
    check(scratch.mkdir())
    try {
        val hog = File(scratch, "source.hog")
        ArchiveFiles.open(source).use { archive ->
            val entry = requireNotNull(archive.findEntry(step.getString("hog_path")))
            val budget = ExtractionBudget()
            budget.registerEntry(entry.sizeBytes, entry.compressedSizeBytes ?: 0, entry.path)
            archive.openInputStream(entry).use { input ->
                hog.outputStream().use { output ->
                    input.copyToBounded(output, budget, entry.compressedSizeBytes ?: -1, entry.path)
                }
            }
        }
        check(hogTestHash(hog) == step.getString("hog_sha256"))
        val stage = MissionZipMusicStageManager(scratch)
        val midiBytes = requireNotNull(stage.readMidiTrackBytes(catalog, midi))
        check(hogTestHash(midiBytes) == step.getString("midi_sha256"))
        val nativeMidi = requireNotNull(MidiPreviewBridge.readHogEntry(hog.absolutePath, midiName))
        check(nativeMidi.contentEquals(midiBytes)) { "C music reader and launcher selected different bytes" }
        check(
            MidiPreviewBridge.readHogEntry(hog.absolutePath, "alias.mid") == null,
        ) { "Short alias incorrectly selected" }
        val staged = requireNotNull(stage.stageCompressedAudioTrack(catalog, audio))
        check(hogTestHash(staged) == step.getString("audio_sha256"))
        check(stage.stageCompressedAudioTrack(catalog, audio) == staged)
        // Decode and start both production preview renderers; bytes alone do not prove playback
        check(MidiPreviewBridge.init(context))
        try {
            check(MidiPreviewBridge.start(midiBytes, false, MidiPreviewBridge.getNativeSampleRate(context)))
            delay(150)
            check(MidiPreviewBridge.getState().state == MidiPreviewBridge.STATE_PLAYING)
        } finally {
            MidiPreviewBridge.stop()
        }
        val player = MediaPlayer()
        try {
            player.setDataSource(staged.absolutePath)
            player.prepare()
            check(player.duration > 0)
            player.start()
            delay(150)
            check(player.isPlaying)
        } finally {
            player.release()
        }
        stage.cleanupOldFiles(maxAgeMs = 0, maxBytes = 0)
        check(!staged.exists())
        check(hogTestHash(source) == before)
        val receipt =
            JSONObject()
                .put("status", "passed")
                .put("members", summary.contents.size)
                .put("archive_sha256", before)
                .put("hog_sha256", hogTestHash(hog))
                .put("midi_sha256", hogTestHash(midiBytes))
                .put("audio_sha256", step.getString("audio_sha256"))
                .put("native_midi_matches", true)
                .put("midi_playback", true)
                .put("audio_playback", true)
                .put("preview_cleanup", true)
        AtomicFilePublication.writeUtf8(File(context.filesDir, "hog_launcher_result.json"), receipt.toString(2) + "\n")
    } finally {
        scratch.deleteRecursively()
    }
}

private fun hogTestHash(bytes: ByteArray): String =
    MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }

private fun hogTestHash(file: File): String {
    val digest = MessageDigest.getInstance("SHA-256")
    file.inputStream().use { input ->
        val buffer = ByteArray(64 * 1024)
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            digest.update(buffer, 0, count)
        }
    }
    return digest.digest().joinToString("") { "%02x".format(it) }
}
