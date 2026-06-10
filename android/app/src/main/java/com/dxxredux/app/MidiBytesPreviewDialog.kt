package com.dxxredux.app

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
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun MidiBytesPreviewDialog(
    title: String,
    trackName: String,
    detailLines: List<String>,
    isHmp: Boolean,
    loadBytes: suspend () -> ByteArray?,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val sampleRate = remember { MidiPreviewBridge.getNativeSampleRate(context) }

    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }
    val playFocus = remember { FocusRequester() }
    val sliderFocus = remember { FocusRequester() }
    val closeFocus = remember { FocusRequester() }
    var loadError by remember { mutableStateOf<String?>(null) }

    DisposableEffect(Unit) {
        MidiPreviewBridge.init(context)
        onDispose { MidiPreviewBridge.stop() }
    }

    LaunchedEffect(playing) {
        while (playing) {
            val state = MidiPreviewBridge.getState()
            if (!seeking) {
                positionMs = state.positionMs
                durationMs = state.durationMs
            }
            if (state.state == MidiPreviewBridge.STATE_STOPPED && durationMs > 0) {
                playing = false
                positionMs = durationMs
            }
            delay(100)
        }
    }

    fun togglePlayback() {
        if (!playing) {
            scope.launch(Dispatchers.IO) {
                val data = loadBytes()
                if (data == null) {
                    loadError = "Could not read $trackName"
                    return@launch
                }
                if (MidiPreviewBridge.start(data, isHmp, sampleRate)) {
                    playing = true
                    loadError = null
                } else {
                    loadError = "Playback failed"
                }
            }
        } else {
            val state = MidiPreviewBridge.getState()
            if (state.state == MidiPreviewBridge.STATE_PLAYING) {
                MidiPreviewBridge.pause()
            } else {
                MidiPreviewBridge.resume()
            }
        }
    }

    AlertDialog(
        modifier = Modifier.repeatVerticalDpadFocus(closeFocus),
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(trackName, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                detailLines.forEach { line ->
                    Text(
                        line,
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                loadError?.let {
                    Text(it, fontSize = 12.sp, color = MaterialTheme.colorScheme.error)
                }

                Spacer(modifier = Modifier.height(4.dp))

                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    TextButton(
                        onClick = { togglePlayback() },
                        modifier = Modifier.focusRequester(playFocus).tvFocusBorder(),
                    ) {
                        val label =
                            if (!playing) {
                                "Play"
                            } else {
                                val state = MidiPreviewBridge.getState()
                                if (state.state == MidiPreviewBridge.STATE_PAUSED) "Resume" else "Pause"
                            }
                        Text(label, fontSize = 13.sp)
                    }
                    if (playing) {
                        TextButton(
                            onClick = {
                                MidiPreviewBridge.stop()
                                playing = false
                                positionMs = 0
                            },
                            modifier = Modifier.tvFocusBorder(),
                        ) {
                            Text("Stop", fontSize = 13.sp)
                        }
                    }
                }

                Slider(
                    value = if (durationMs > 0) positionMs.toFloat() / durationMs.toFloat() else 0f,
                    onValueChange = { fraction ->
                        if (durationMs > 0) {
                            seeking = true
                            positionMs = (fraction * durationMs).toInt()
                        }
                    },
                    onValueChangeFinished = {
                        if (durationMs > 0) {
                            MidiPreviewBridge.seek(positionMs.toFloat() / durationMs.toFloat())
                        }
                        seeking = false
                    },
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .focusRequester(sliderFocus)
                            .focusProperties {
                                up = playFocus
                                down = closeFocus
                            }.tvFocusBorder()
                            .repeatVerticalDpadFocus(),
                )
                if (durationMs > 0) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text(formatMidiPreviewTime(positionMs), fontSize = 10.sp)
                        Text(formatMidiPreviewTime(durationMs), fontSize = 10.sp)
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

private fun formatMidiPreviewTime(ms: Int): String {
    val seconds = ms / 1000
    return "%d:%02d".format(seconds / 60, seconds % 60)
}
