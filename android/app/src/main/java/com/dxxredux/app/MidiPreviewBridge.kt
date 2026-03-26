package com.dxxredux.app

import android.content.Context
import android.media.AudioManager

/**
 * JNI bridge for MIDI/HMP preview playback in the launcher.
 *
 * Uses a standalone C player (midi_preview.c) that renders MIDI via
 * TinySoundFont and outputs through OpenSL ES -- no SDL required.
 */
object MidiPreviewBridge {
    const val STATE_STOPPED = 0
    const val STATE_PLAYING = 1
    const val STATE_PAUSED = -1

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    data class PlaybackState(
        val state: Int,
        val positionMs: Int,
        val durationMs: Int,
    )

    fun getNativeSampleRate(context: Context): Int {
        val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val rateStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        return rateStr?.toIntOrNull() ?: 48000
    }

    /** Initialize the MIDI synth (loads gm.sf2 from APK assets). Call once. */
    fun init(context: Context): Boolean = nativeInit(context.assets)

    /**
     * Start MIDI/HMP preview from raw file bytes.
     * @param data raw file bytes (HMP or standard MIDI)
     * @param isHmp true if the data is HMP format
     * @param sampleRate device native sample rate
     */
    fun start(
        data: ByteArray,
        isHmp: Boolean,
        sampleRate: Int,
    ): Boolean = nativeStart(data, isHmp, sampleRate)

    fun stop() = nativeStop()

    fun pause() = nativePause()

    fun resume() = nativeResume()

    fun seek(fraction: Float): Boolean = nativeSeek(fraction)

    fun getState(): PlaybackState {
        val raw = nativeGetState()
        val parts = raw.split("|")
        if (parts.size != 3) return PlaybackState(STATE_STOPPED, 0, 0)
        return PlaybackState(
            state = parts[0].toIntOrNull() ?: STATE_STOPPED,
            positionMs = parts[1].toIntOrNull() ?: 0,
            durationMs = parts[2].toIntOrNull() ?: 0,
        )
    }

    /** Read a file entry from a HOG archive. Returns null if not found. */
    fun readHogEntry(
        hogPath: String,
        entryName: String,
    ): ByteArray? = nativeReadHogEntry(hogPath, entryName)

    // -- JNI declarations --

    @JvmStatic private external fun nativeInit(assetManager: android.content.res.AssetManager): Boolean

    @JvmStatic private external fun nativeStart(
        data: ByteArray,
        isHmp: Boolean,
        sampleRate: Int,
    ): Boolean

    @JvmStatic private external fun nativeStop()

    @JvmStatic private external fun nativePause()

    @JvmStatic private external fun nativeResume()

    @JvmStatic private external fun nativeSeek(fraction: Float): Boolean

    @JvmStatic private external fun nativeGetState(): String

    @JvmStatic private external fun nativeReadHogEntry(
        hogPath: String,
        entryName: String,
    ): ByteArray?
}
