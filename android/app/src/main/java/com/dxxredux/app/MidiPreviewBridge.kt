package com.dxxredux.app

import android.content.Context
import android.media.AudioManager
import java.util.concurrent.atomic.AtomicLong

/**
 * JNI bridge for MIDI/HMP preview playback in the launcher.
 *
 * Uses a standalone C player (midi_preview.c) that renders MIDI via
 * FluidSynth or ymfm and outputs through OpenSL ES -- no SDL required.
 */
object MidiPreviewBridge {
    const val STATE_STOPPED = 0
    const val STATE_PLAYING = 1
    const val STATE_PAUSED = -1
    private val lifecycleLock = Any()
    private val requestedGeneration = AtomicLong()

    fun reserveStart(): Long = requestedGeneration.incrementAndGet()

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    data class PlaybackState(
        val state: Int,
        val positionMs: Int,
        val durationMs: Int,
        val renderer: String = "sf2",
    )

    fun getNativeSampleRate(context: Context): Int {
        val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val rateStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        return rateStr?.toIntOrNull() ?: 48000
    }

    private fun initProfile(
        context: Context,
        path: String,
        fm: Boolean,
    ): Boolean {
        val state = SoundfontStore(context).read()
        return nativeInit(context.assets, path, fm, state.reverb, state.chorus, MusicEq.nativeId(state.eq))
    }

    fun selectEffects(
        context: Context,
        reverb: Boolean,
        chorus: Boolean,
    ) = synchronized(lifecycleLock) {
        requestedGeneration.incrementAndGet()
        val store = SoundfontStore(context)
        store.selectEffects(reverb, chorus) { rev, cho ->
            nativeInit(
                context.assets,
                store.selectedPath(),
                store.read().renderer == "ymfm",
                rev,
                cho,
                MusicEq.nativeId(store.read().eq),
            )
        }
    }

    /** Resolve the same persisted instrument asset used at game startup. Call on IO. */
    fun selectEq(
        context: Context,
        preset: String,
    ) = synchronized(lifecycleLock) {
        requestedGeneration.incrementAndGet()
        val store = SoundfontStore(context)
        store.selectEq(preset) { selected ->
            val state = store.read()
            nativeInit(
                context.assets,
                store.selectedPath(),
                state.renderer == "ymfm",
                state.reverb,
                state.chorus,
                MusicEq.nativeId(selected),
            )
        }
    }

    fun getEqualizer(): Int = synchronized(lifecycleLock) { nativeGetEq() }

    fun init(context: Context): Boolean =
        synchronized(lifecycleLock) {
            val store = SoundfontStore(context)
            initProfile(context, store.selectedPath(), store.read().renderer == "ymfm")
        }

    fun selectSoundfont(
        context: Context,
        id: String,
    ) = synchronized(lifecycleLock) {
        requestedGeneration.incrementAndGet()
        val store = SoundfontStore(context)
        store.select(id) { initProfile(context, it, store.read().renderer == "ymfm") }
    }

    fun selectRenderer(
        context: Context,
        renderer: String,
    ) = synchronized(lifecycleLock) {
        requestedGeneration.incrementAndGet()
        SoundfontStore(context).selectRenderer(renderer) { path, fm -> initProfile(context, path, fm) }
    }

    fun deleteSoundfont(
        context: Context,
        id: String,
    ) = synchronized(lifecycleLock) {
        val store = SoundfontStore(context)
        store.delete(id) { path ->
            requestedGeneration.incrementAndGet()
            initProfile(context, path, store.read().renderer == "ymfm")
        }
    }

    fun validateSoundfont(path: String): Boolean = nativeValidateSoundfont(path)

    internal fun resetPreferences(
        context: Context,
        preset: GameSettingsPreset,
    ) = synchronized(lifecycleLock) {
        requestedGeneration.incrementAndGet()
        val store = SoundfontStore(context)
        val old = store.read()
        var first = true
        preset.resetMidiPreferences(store) { path, fm ->
            val result =
                nativeInit(
                    context.assets,
                    path,
                    fm,
                    if (first) true else old.reverb,
                    if (first) true else old.chorus,
                    if (first) 0 else MusicEq.nativeId(old.eq),
                )
            first = false
            result
        }
    }

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
    ): Boolean = startReserved(reserveStart(), data, isHmp, sampleRate)

    fun startReserved(
        generation: Long,
        data: ByteArray,
        isHmp: Boolean,
        sampleRate: Int,
        hogPath: String = "",
        song: String = "",
    ): Boolean =
        synchronized(lifecycleLock) {
            if (generation != requestedGeneration.get()) return@synchronized false
            synchronized(MidiEnumerationBridge.nativeDataLock) {
                nativeStart(data, isHmp, sampleRate, hogPath, song)
            }
        }

    fun stop() =
        synchronized(lifecycleLock) {
            requestedGeneration.incrementAndGet()
            nativeStop()
        }

    fun pause() = synchronized(lifecycleLock) { nativePause() }

    fun resume() = synchronized(lifecycleLock) { nativeResume() }

    fun seek(fraction: Float): Boolean = synchronized(lifecycleLock) { nativeSeek(fraction) }

    fun getState(): PlaybackState {
        val raw = synchronized(lifecycleLock) { nativeGetState() }
        val parts = raw.split("|")
        if (parts.size != 4) return PlaybackState(STATE_STOPPED, 0, 0)
        return PlaybackState(
            state = parts[0].toIntOrNull() ?: STATE_STOPPED,
            positionMs = parts[1].toIntOrNull() ?: 0,
            durationMs = parts[2].toIntOrNull() ?: 0,
            renderer = parts[3],
        )
    }

    /** Read a file entry from a HOG archive. Returns null if not found. */
    fun readHogEntry(
        hogPath: String,
        entryName: String,
    ): ByteArray? = nativeReadHogEntry(hogPath, entryName)

    // -- JNI declarations --

    @JvmStatic private external fun nativeInit(
        assetManager: android.content.res.AssetManager,
        path: String,
        preferFm: Boolean,
        reverb: Boolean,
        chorus: Boolean,
        equalizer: Int,
    ): Boolean

    @JvmStatic private external fun nativeGetEq(): Int

    @JvmStatic private external fun nativeValidateSoundfont(path: String): Boolean

    @JvmStatic private external fun nativeStart(
        data: ByteArray,
        isHmp: Boolean,
        sampleRate: Int,
        hogPath: String,
        song: String,
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
