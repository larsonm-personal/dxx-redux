package com.dxxredux.app

import android.content.Context
import android.media.AudioManager
import android.media.MediaPlayer
import android.view.KeyEvent
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.io.DataOutputStream
import java.io.File
import kotlin.math.abs

/** Device integration test invoked by the launcher automation runner */
internal suspend fun testPreviewMediaControls(
    context: Context,
    audioPath: String,
) = withContext(Dispatchers.Main) {
    check(BuildConfig.DEBUG)
    val audioFile = File(context.filesDir, audioPath)
    check(audioFile.isFile) { "Missing preview test audio: $audioPath" }
    val scratch = File(context.cacheDir, "preview-media-test").apply { mkdirs() }
    val bin = File(scratch, "test.bin")
    val cue = File(scratch, "test.cue")
    withContext(Dispatchers.IO) {
        bin.outputStream().use { output -> repeat(45 * 75) { output.write(ByteArray(2352)) } }
        cue.writeText("FILE \"test.bin\" BINARY\n  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n")
        check(MidiPreviewBridge.init(context)) { "MIDI renderer initialization failed" }
    }
    val midi =
        ByteArrayOutputStream()
            .also { output ->
                DataOutputStream(output).use { data ->
                    data.writeBytes("MThd")
                    data.writeInt(6)
                    data.writeShort(0)
                    data.writeShort(1)
                    data.writeShort(480)
                    // 45 seconds at the default 500000 us/quarter tempo
                    val events =
                        byteArrayOf(
                            0,
                            0x90.toByte(),
                            60,
                            30,
                            0x82.toByte(),
                            0xd1.toByte(),
                            0x40,
                            0x80.toByte(),
                            60,
                            0,
                            0,
                            0xff.toByte(),
                            0x2f,
                            0,
                        )
                    data.writeBytes("MTrk")
                    data.writeInt(events.size)
                    data.write(events)
                }
            }.toByteArray()
    var filePlayer: MediaPlayer? = null
    val players =
        listOf(
            "MIDI" to
                PreviewTransport(
                    snapshot = {
                        MidiPreviewBridge.getState().let {
                            nativePreviewSnapshot(
                                it.state,
                                it.positionMs,
                                it.durationMs,
                            )
                        }
                    },
                    start = {
                        if (MidiPreviewBridge.getState().state == -1) {
                            MidiPreviewBridge.resume()
                        } else {
                            check(MidiPreviewBridge.start(midi, false, MidiPreviewBridge.getNativeSampleRate(context)))
                        }
                    },
                    pause = { MidiPreviewBridge.pause() },
                    stop = { MidiPreviewBridge.stop() },
                    seek = { MidiPreviewBridge.seek(it.toFloat() / MidiPreviewBridge.getState().durationMs) },
                ),
            "CD" to
                PreviewTransport(
                    snapshot = {
                        CdPreviewBridge.getState().let {
                            nativePreviewSnapshot(
                                it.state,
                                it.positionMs,
                                it.durationMs,
                            )
                        }
                    },
                    start = {
                        if (CdPreviewBridge.getState().state == -1) {
                            CdPreviewBridge.resume()
                        } else {
                            check(
                                CdPreviewBridge.start(
                                    bin.path,
                                    cue.path,
                                    1,
                                    CdPreviewBridge.getNativeSampleRate(context),
                                ),
                            )
                        }
                    },
                    pause = { CdPreviewBridge.pause() },
                    stop = { CdPreviewBridge.stop() },
                    seek = { CdPreviewBridge.seek(it.toFloat() / CdPreviewBridge.getState().durationMs) },
                ),
            "MP3" to
                PreviewTransport(
                    snapshot = {
                        filePlayer?.let {
                            PreviewSnapshot(
                                if (it.isPlaying) PreviewStatus.PLAYING else PreviewStatus.PAUSED,
                                it.currentPosition,
                                it.duration,
                            )
                        } ?: PreviewSnapshot(PreviewStatus.STOPPED)
                    },
                    start = {
                        if (filePlayer == null) {
                            filePlayer =
                                MediaPlayer().apply {
                                    setDataSource(audioFile.path)
                                    prepare()
                                }
                        }
                        filePlayer!!.start()
                    },
                    pause = { filePlayer?.pause() },
                    stop = {
                        filePlayer?.release()
                        filePlayer = null
                    },
                    seek = { filePlayer?.seekTo(it) },
                ),
        )
    val audio = context.getSystemService(AudioManager::class.java)

    suspend fun press(key: Int) {
        audio.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, key))
        audio.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_UP, key))
        delay(250)
    }
    for ((name, player) in players) {
        val media = LauncherPreviewMediaSession(context, "Test $name") { player }

        suspend fun expect(
            label: String,
            condition: (PreviewSnapshot) -> Boolean,
        ) {
            repeat(40) {
                media.refresh()
                if (condition(player.snapshot())) return
                delay(100)
            }
            error("$name $label: ${player.snapshot()}")
        }
        try {
            media.play()
            expect("start") { it.status == PreviewStatus.PLAYING && it.durationMs > 20_000 }
            press(KeyEvent.KEYCODE_MEDIA_PAUSE)
            expect("pause") { it.status == PreviewStatus.PAUSED }
            val initial = player.snapshot().positionMs
            press(KeyEvent.KEYCODE_MEDIA_NEXT)
            expect("paused +10s") { it.status == PreviewStatus.PAUSED && abs(it.positionMs - initial - 10_000) < 700 }
            press(KeyEvent.KEYCODE_MEDIA_PREVIOUS)
            expect("paused -10s") { it.status == PreviewStatus.PAUSED && abs(it.positionMs - initial) < 700 }
            press(KeyEvent.KEYCODE_MEDIA_PREVIOUS)
            expect("clamp start") { it.status == PreviewStatus.PAUSED && it.positionMs < 700 }
            // An explicit Pause must not toggle back to playing
            press(KeyEvent.KEYCODE_MEDIA_PAUSE)
            expect("idempotent pause") { it.status == PreviewStatus.PAUSED }
            press(KeyEvent.KEYCODE_MEDIA_PLAY)
            expect("resume") { it.status == PreviewStatus.PLAYING }
            press(KeyEvent.KEYCODE_MEDIA_NEXT)
            expect("playing +10s") { it.status == PreviewStatus.PLAYING && it.positionMs >= 9_500 }
            press(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE)
            expect("toggle pause") { it.status == PreviewStatus.PAUSED }
            press(KeyEvent.KEYCODE_MEDIA_STOP)
            expect("stop") { it.status == PreviewStatus.STOPPED }
            check(
                !LauncherPreviewMediaSession.dispatchKeyEvent(
                    KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PLAY),
                ),
            )
            media.foreground = false
            media.play()
            check(player.snapshot().status == PreviewStatus.STOPPED) { "$name restarted outside launcher" }
            LauncherDebugLog.log("preview-media-test PASS $name")
        } finally {
            media.release()
        }
    }
    bin.delete()
    cue.delete()
}
