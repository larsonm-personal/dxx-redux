package com.dxxredux.app

import android.media.MediaPlayer
import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
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
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import java.io.File

private const val AUDIO_PREVIEW_TAG = "DXX-AudioPreview"

data class AudioFilePreviewLine(
    val text: String,
    val primary: Boolean = false,
    val small: Boolean = false,
)

@Composable
fun AudioFilePreviewDialog(
    title: String,
    audioFile: File,
    lines: List<AudioFilePreviewLine>,
    onDismiss: () -> Unit,
) {
    var player by remember { mutableStateOf<MediaPlayer?>(null) }
    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }
    val sliderFocus = remember { FocusRequester() }
    val closeFocus = remember { FocusRequester() }

    DisposableEffect(Unit) {
        onDispose {
            player?.release()
        }
    }

    LaunchedEffect(playing) {
        while (playing) {
            val p = player
            if (p != null && p.isPlaying && !seeking) {
                positionMs = p.currentPosition
            }
            delay(100)
        }
    }

    fun togglePlayback() {
        val p = player
        if (p == null) {
            if (!audioFile.exists()) return
            try {
                val mp =
                    MediaPlayer().apply {
                        setDataSource(audioFile.absolutePath)
                        prepare()
                        start()
                    }
                player = mp
                durationMs = mp.duration
                playing = true
            } catch (e: Exception) {
                Log.e(AUDIO_PREVIEW_TAG, "Failed to play ${audioFile.name}: ${e.message}")
            }
        } else if (p.isPlaying) {
            p.pause()
        } else {
            p.start()
            playing = true
        }
    }

    AlertDialog(
        modifier = Modifier.repeatVerticalDpadFocus(closeFocus),
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
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
                                    player?.isPlaying == true -> "Pause"
                                    else -> "Resume"
                                }
                            Text(label, fontSize = 13.sp)
                        }
                        if (player != null) {
                            TextButton(
                                onClick = {
                                    player?.stop()
                                    player?.release()
                                    player = null
                                    playing = false
                                    positionMs = 0
                                    durationMs = 0
                                },
                            ) {
                                Text("Stop", fontSize = 13.sp)
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
