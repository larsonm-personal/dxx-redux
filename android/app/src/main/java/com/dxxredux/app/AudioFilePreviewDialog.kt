package com.dxxredux.app

import android.media.MediaPlayer
import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private const val AUDIO_PREVIEW_TAG = "DXX-AudioPreview"

data class AudioFilePreviewLine(
    val text: String,
    val primary: Boolean = false,
    val small: Boolean = false,
)

@Composable
internal fun MetadataPrintout(
    lines: List<AudioFilePreviewLine>?,
    loading: Boolean,
    emptyMessage: String,
) {
    when {
        loading -> {
            Text("Reading metadata...", fontSize = 12.sp)
        }

        lines.isNullOrEmpty() -> {
            Text(emptyMessage, fontSize = 12.sp)
        }

        else -> {
            lines.forEach { line ->
                Text(
                    line.text,
                    fontSize = if (line.small) 10.sp else 11.sp,
                    color =
                        if (line.primary) {
                            MaterialTheme.colorScheme.primary
                        } else {
                            MaterialTheme.colorScheme.onSurfaceVariant
                        },
                )
            }
        }
    }
}

@Composable
fun AudioFilePreviewDialog(
    title: String,
    audioFile: File,
    lines: List<AudioFilePreviewLine>,
    loadMetadata: (suspend () -> List<AudioFilePreviewLine>)? = null,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var player by remember { mutableStateOf<MediaPlayer?>(null) }
    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }
    var showMetadata by remember { mutableStateOf(false) }
    var metadataLoading by remember { mutableStateOf(false) }
    var metadataLines by remember { mutableStateOf<List<AudioFilePreviewLine>?>(null) }
    val sliderFocus = remember { FocusRequester() }
    val closeFocus = remember { FocusRequester() }
    val playerOwner =
        remember {
            AudioPreviewResourceOwner<MediaPlayer> { released ->
                runCatching { released.setOnCompletionListener(null) }
                runCatching { released.setOnErrorListener(null) }
                runCatching { released.release() }
                    .onFailure { Log.e(AUDIO_PREVIEW_TAG, "Failed to release audio preview", it) }
            }
        }

    fun releasePlayer(
        expected: MediaPlayer? = playerOwner.current,
        finalPositionMs: Int = 0,
        retainDuration: Boolean = false,
    ) {
        if (!playerOwner.release(expected)) return
        if (player === expected) player = null
        playing = false
        seeking = false
        positionMs = finalPositionMs
        if (!retainDuration) durationMs = 0
    }

    DisposableEffect(playerOwner) {
        onDispose {
            playerOwner.release()
        }
    }

    LaunchedEffect(playing) {
        while (playing) {
            val p = player
            if (p != null && !seeking) {
                try {
                    if (p.isPlaying) positionMs = p.currentPosition
                } catch (e: IllegalStateException) {
                    Log.e(AUDIO_PREVIEW_TAG, "Audio preview polling failed", e)
                    releasePlayer(p)
                }
            }
            delay(100)
        }
    }

    fun togglePlayback() {
        val p = player
        if (p == null) {
            if (!audioFile.exists()) return
            try {
                val mp = MediaPlayer()
                initializeAudioPreviewResource(playerOwner, mp) { starting ->
                    starting.setOnCompletionListener { completed ->
                        val finalPosition = runCatching { completed.duration }.getOrDefault(durationMs)
                        releasePlayer(completed, finalPosition, retainDuration = true)
                    }
                    starting.setOnErrorListener { failed, what, extra ->
                        Log.e(AUDIO_PREVIEW_TAG, "Audio preview error what=$what extra=$extra")
                        releasePlayer(failed)
                        true
                    }
                    starting.setDataSource(audioFile.absolutePath)
                    starting.prepare()
                    durationMs = starting.duration
                    starting.start()
                }
                if (playerOwner.current === mp) {
                    player = mp
                    positionMs = 0
                    playing = true
                }
            } catch (e: Exception) {
                positionMs = 0
                durationMs = 0
                playing = false
                Log.e(AUDIO_PREVIEW_TAG, "Failed to play ${audioFile.name}", e)
            }
        } else {
            try {
                if (p.isPlaying) {
                    p.pause()
                    playing = false
                } else {
                    p.start()
                    playing = true
                }
            } catch (e: Exception) {
                Log.e(AUDIO_PREVIEW_TAG, "Failed to toggle ${audioFile.name}", e)
                releasePlayer(p)
            }
        }
    }

    AlertDialog(
        modifier = Modifier.repeatVerticalDpadFocus(closeFocus),
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            Column(
                modifier = Modifier.heightIn(max = 460.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                lines.forEach { line ->
                    Text(
                        line.text,
                        fontSize = if (line.small) 10.sp else 12.sp,
                        color =
                            if (line.primary) {
                                MaterialTheme.colorScheme.primary
                            } else if (line.small) {
                                MaterialTheme.colorScheme.onSurfaceVariant
                            } else {
                                MaterialTheme.colorScheme.onSurface
                            },
                    )
                }
                if (showMetadata) {
                    MetadataPrintout(metadataLines, metadataLoading, "No readable embedded metadata.")
                }

                if (audioFile.exists()) {
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        TextButton(onClick = { togglePlayback() }) {
                            val label =
                                when {
                                    player == null -> "Play"
                                    playing -> "Pause"
                                    else -> "Resume"
                                }
                            Text(label, fontSize = 13.sp)
                        }
                        if (player != null) {
                            TextButton(
                                onClick = {
                                    releasePlayer()
                                },
                            ) {
                                Text("Stop", fontSize = 13.sp)
                            }
                        }
                        if (loadMetadata != null) {
                            TextButton(
                                onClick = {
                                    showMetadata = !showMetadata
                                    if (showMetadata && metadataLines == null && !metadataLoading) {
                                        metadataLoading = true
                                        scope.launch(Dispatchers.IO) {
                                            val loaded = loadMetadata()
                                            withContext(Dispatchers.Main) {
                                                metadataLines = loaded
                                                metadataLoading = false
                                            }
                                        }
                                    }
                                },
                            ) {
                                Text(if (showMetadata) "Hide metadata" else "Metadata", fontSize = 13.sp)
                            }
                        }
                    }
                    Slider(
                        value = if (durationMs > 0) positionMs.toFloat() / durationMs.toFloat() else 0f,
                        onValueChange = { frac ->
                            if (durationMs > 0) {
                                seeking = true
                                positionMs = (frac * durationMs).toInt()
                            }
                        },
                        onValueChangeFinished = {
                            if (durationMs > 0) {
                                player?.seekTo(positionMs)
                            }
                            seeking = false
                        },
                        enabled = durationMs > 0,
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .focusRequester(sliderFocus)
                                .focusProperties { down = closeFocus }
                                .tvFocusBorder()
                                .repeatVerticalDpadFocus(),
                    )
                    if (durationMs > 0) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            Text(formatAudioPreviewTime(positionMs), fontSize = 10.sp)
                            Text(formatAudioPreviewTime(durationMs), fontSize = 10.sp)
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = onDismiss,
                modifier = Modifier.focusRequester(closeFocus).focusProperties { up = sliderFocus }.tvFocusBorder(),
            ) { Text("Close") }
        },
    )
}

private fun formatAudioPreviewTime(ms: Int): String {
    val seconds = ms / 1000
    return "%d:%02d".format(seconds / 60, seconds % 60)
}
