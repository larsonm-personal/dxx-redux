package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.KeyEvent
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import kotlinx.coroutines.delay

@Suppress("DEPRECATION")
internal class LauncherPreviewMediaSession(
    context: Context,
    private val title: String,
    private val transport: () -> PreviewTransport,
) {
    private val audio = context.getSystemService(AudioManager::class.java)
    private val attributes =
        AudioAttributes
            .Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build()
    private val session = MediaSession(context, "Launcher music preview")
    private var owned = false
    private var focused = false
    private var released = false
    var foreground = true
    private val focusListener =
        AudioManager.OnAudioFocusChangeListener { change ->
            if (change < 0) {
                // Explicit resume only, including after calls and transient interruptions
                pause()
                abandonFocus()
                if (change == AudioManager.AUDIOFOCUS_LOSS) deactivate()
            }
        }
    private val focusRequest =
        if (Build.VERSION.SDK_INT >= 26) {
            AudioFocusRequest
                .Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(attributes)
                .setWillPauseWhenDucked(true)
                .setOnAudioFocusChangeListener(focusListener, Handler(Looper.getMainLooper()))
                .build()
        } else {
            null
        }

    init {
        // Required on API 24/25; newer Android versions enable these automatically
        session.setFlags(MediaSession.FLAG_HANDLES_MEDIA_BUTTONS or MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS)
        session.setPlaybackToLocal(attributes)
        session.setCallback(
            object : MediaSession.Callback() {
                override fun onPlay() = play()

                override fun onPause() = pause()

                override fun onStop() = stop()

                override fun onSkipToNext() = skip(1)

                override fun onSkipToPrevious() = skip(-1)

                override fun onFastForward() = skip(1)

                override fun onRewind() = skip(-1)

                override fun onSeekTo(pos: Long) {
                    if (owned) {
                        val duration = transport().snapshot().durationMs
                        if (duration > 0) transport().seek(pos.coerceIn(0, duration.toLong()).toInt())
                        refresh()
                    }
                }

                @Suppress("DEPRECATION")
                override fun onMediaButtonEvent(intent: Intent): Boolean {
                    val event = intent.getParcelableExtra<KeyEvent>(Intent.EXTRA_KEY_EVENT) ?: return false
                    return handleKey(event)
                }
            },
            Handler(Looper.getMainLooper()),
        )
    }

    @Suppress("DEPRECATION")
    fun play() {
        if (released || !foreground) return
        if (!focused) {
            val result =
                if (Build.VERSION.SDK_INT >= 26) {
                    audio.requestAudioFocus(focusRequest!!)
                } else {
                    audio.requestAudioFocus(focusListener, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)
                }
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) return
            focused = true
        }
        if (current !== this) current?.stop()
        current = this
        owned = true
        session.isActive = true
        if (transport().snapshot().status !in listOf(PreviewStatus.PLAYING, PreviewStatus.LOADING)) transport().start()
        refresh()
    }

    fun pause() {
        if (!owned || released) return
        transport().cancelPendingNavigation()
        when (transport().snapshot().status) {
            PreviewStatus.PLAYING -> transport().pause()
            PreviewStatus.LOADING -> transport().stop()
            else -> Unit
        }
        refresh()
    }

    fun toggle() {
        if (transport().snapshot().status in listOf(PreviewStatus.PLAYING, PreviewStatus.LOADING)) pause() else play()
    }

    fun skip(direction: Int) {
        if (!owned || released || !foreground) return
        transport().skip(direction)
        refresh()
    }

    fun stop() {
        if (released) return
        transport().cancelPendingNavigation()
        transport().stop()
        deactivate()
    }

    fun refresh() {
        if (released || !owned) return
        val state = transport().snapshot()
        val platformState =
            when (state.status) {
                PreviewStatus.STOPPED -> PlaybackState.STATE_STOPPED
                PreviewStatus.LOADING -> PlaybackState.STATE_BUFFERING
                PreviewStatus.PLAYING -> PlaybackState.STATE_PLAYING
                PreviewStatus.PAUSED -> PlaybackState.STATE_PAUSED
            }
        session.setMetadata(
            MediaMetadata
                .Builder()
                .putString(MediaMetadata.METADATA_KEY_TITLE, title)
                .putLong(MediaMetadata.METADATA_KEY_DURATION, state.durationMs.toLong())
                .build(),
        )
        session.setPlaybackState(
            PlaybackState
                .Builder()
                .setActions(
                    PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE or PlaybackState.ACTION_PLAY_PAUSE or
                        PlaybackState.ACTION_STOP or PlaybackState.ACTION_SKIP_TO_NEXT or
                        PlaybackState.ACTION_SKIP_TO_PREVIOUS or
                        PlaybackState.ACTION_SEEK_TO or PlaybackState.ACTION_FAST_FORWARD or
                        PlaybackState.ACTION_REWIND,
                ).setState(
                    platformState,
                    state.positionMs.toLong(),
                    if (state.status ==
                        PreviewStatus.PLAYING
                    ) {
                        1f
                    } else {
                        0f
                    },
                ).build(),
        )
        if (state.status == PreviewStatus.STOPPED) deactivate()
    }

    @Suppress("DEPRECATION")
    private fun abandonFocus() {
        if (!focused) return
        focused = false
        if (Build.VERSION.SDK_INT >=
            26
        ) {
            audio.abandonAudioFocusRequest(focusRequest!!)
        } else {
            audio.abandonAudioFocus(focusListener)
        }
    }

    private fun deactivate() {
        owned = false
        session.isActive = false
        session.setPlaybackState(PlaybackState.Builder().setState(PlaybackState.STATE_STOPPED, 0, 0f).build())
        abandonFocus()
        if (current === this) current = null
    }

    fun release() {
        if (released) return
        stop()
        released = true
        session.release()
    }

    private fun handleKey(event: KeyEvent): Boolean {
        if (!owned || !foreground) return false
        if (event.keyCode !in mediaKeys) return false
        // Consume both halves and repeats, but execute once per press
        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
            when (event.keyCode) {
                KeyEvent.KEYCODE_MEDIA_PLAY -> play()
                KeyEvent.KEYCODE_MEDIA_PAUSE -> pause()
                KeyEvent.KEYCODE_MEDIA_STOP -> stop()
                KeyEvent.KEYCODE_MEDIA_NEXT, KeyEvent.KEYCODE_MEDIA_FAST_FORWARD -> skip(1)
                KeyEvent.KEYCODE_MEDIA_PREVIOUS, KeyEvent.KEYCODE_MEDIA_REWIND -> skip(-1)
                else -> toggle()
            }
        }
        return true
    }

    companion object {
        private var current: LauncherPreviewMediaSession? = null
        private val mediaKeys =
            setOf(
                KeyEvent.KEYCODE_MEDIA_PLAY,
                KeyEvent.KEYCODE_MEDIA_PAUSE,
                KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE,
                KeyEvent.KEYCODE_HEADSETHOOK,
                KeyEvent.KEYCODE_MEDIA_STOP,
                KeyEvent.KEYCODE_MEDIA_NEXT,
                KeyEvent.KEYCODE_MEDIA_PREVIOUS,
                KeyEvent.KEYCODE_MEDIA_FAST_FORWARD,
                KeyEvent.KEYCODE_MEDIA_REWIND,
            )

        fun dispatchKeyEvent(event: KeyEvent): Boolean = current?.handleKey(event) == true
    }
}

@Composable
internal fun rememberPreviewMediaSession(
    title: String,
    autoPlay: Boolean,
    transport: PreviewTransport,
): LauncherPreviewMediaSession {
    val context = LocalContext.current
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    val latest = rememberUpdatedState(transport)
    val media = remember { LauncherPreviewMediaSession(context, title) { latest.value } }
    DisposableEffect(media, lifecycle) {
        media.foreground = lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED)
        val observer =
            LifecycleEventObserver { _, event ->
                if (event == Lifecycle.Event.ON_STOP) {
                    media.foreground = false
                    media.stop()
                } else if (event == Lifecycle.Event.ON_START) {
                    media.foreground = true
                }
            }
        lifecycle.addObserver(observer)
        onDispose {
            lifecycle.removeObserver(observer)
            media.release()
        }
    }
    LaunchedEffect(media) {
        if (autoPlay) media.play()
        while (true) {
            media.refresh()
            delay(100)
        }
    }
    return media
}
