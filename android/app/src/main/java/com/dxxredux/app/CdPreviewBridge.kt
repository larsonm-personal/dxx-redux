package com.dxxredux.app

import android.content.Context
import android.media.AudioManager
import java.util.concurrent.atomic.AtomicLong

/**
 * JNI bridge for CD audio preview playback in the launcher.
 *
 * Uses a standalone C player (cd_preview.c) that reads BIN/CUE files
 * directly via fopen and outputs through OpenSL ES -- no SDL required.
 * Shares PCM decode and resample logic with the in-game player
 * (rbaudio_bin.c) so bugs surface in both code paths.
 */
object CdPreviewBridge {
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
    )

    /** Get the device native sample rate (used for OpenSL ES output) */
    fun getNativeSampleRate(context: Context): Int {
        val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val rateStr = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
        return rateStr?.toIntOrNull() ?: 48000
    }

    /**
     * Start preview of an audio track from a BIN/CUE pair.
     * @param binPath absolute path to the BIN file
     * @param cuePath absolute path to the CUE file
     * @param audioTrack 1-based index among audio tracks only
     * @param sampleRate device native sample rate
     */
    fun start(
        binPath: String,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = startReserved(reserveStart(), binPath, cuePath, audioTrack, sampleRate)

    fun startReserved(
        generation: Long,
        binPath: String,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = runReserved(generation) { nativeStart(binPath, cuePath, audioTrack, sampleRate) }

    fun startMulti(
        binPaths: List<String>,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = startMultiReserved(reserveStart(), binPaths, cuePath, audioTrack, sampleRate)

    fun startMultiReserved(
        generation: Long,
        binPaths: List<String>,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = runReserved(generation) { nativeStartMulti(binPaths.toTypedArray(), cuePath, audioTrack, sampleRate) }

    /**
     * Start preview using a file descriptor for the BIN file.
     * The fd is dup'd internally so the caller may close it after this returns.
     */
    fun startFd(
        fd: Int,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = startFdReserved(reserveStart(), fd, cuePath, audioTrack, sampleRate)

    fun startFdReserved(
        generation: Long,
        fd: Int,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = runReserved(generation) { nativeStartFd(fd, cuePath, audioTrack, sampleRate) }

    fun startMultiFd(
        fds: IntArray,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = startMultiFdReserved(reserveStart(), fds, cuePath, audioTrack, sampleRate)

    fun startMultiFdReserved(
        generation: Long,
        fds: IntArray,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean = runReserved(generation) { nativeStartMultiFd(fds, cuePath, audioTrack, sampleRate) }

    private fun runReserved(
        generation: Long,
        start: () -> Boolean,
    ): Boolean =
        synchronized(lifecycleLock) {
            generation == requestedGeneration.get() && start()
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
        if (parts.size != 3) return PlaybackState(STATE_STOPPED, 0, 0)
        return PlaybackState(
            state = parts[0].toIntOrNull() ?: STATE_STOPPED,
            positionMs = parts[1].toIntOrNull() ?: 0,
            durationMs = parts[2].toIntOrNull() ?: 0,
        )
    }

    // ── JNI declarations ──────────────────────────────────────────

    @JvmStatic private external fun nativeStart(
        binPath: String,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean

    @JvmStatic private external fun nativeStartMulti(
        binPaths: Array<String>,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean

    @JvmStatic private external fun nativeStartFd(
        fd: Int,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean

    @JvmStatic private external fun nativeStartMultiFd(
        fds: IntArray,
        cuePath: String,
        audioTrack: Int,
        sampleRate: Int,
    ): Boolean

    @JvmStatic private external fun nativeStop()

    @JvmStatic private external fun nativePause()

    @JvmStatic private external fun nativeResume()

    @JvmStatic private external fun nativeSeek(fraction: Float): Boolean

    @JvmStatic private external fun nativeGetState(): String
}
