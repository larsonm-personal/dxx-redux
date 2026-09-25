package com.dxxredux.app

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
import kotlinx.coroutines.withContext

@Composable
fun MidiBytesPreviewDialog(
    title: String,
    trackName: String,
    detailLines: List<String>,
    isHmp: Boolean,
    loadBytes: suspend () -> ByteArray?,
    loadMetadata: (suspend () -> MidiMetadata?)? = null,
    onDismiss: () -> Unit,
    hogPath: String = "",
    autoPlay: Boolean = false,
    onSkip: ((Int) -> Unit)? = null,
    onCancelPending: () -> Unit = {},
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val sampleRate = remember { MidiPreviewBridge.getNativeSampleRate(context) }
    val contentScroll = rememberScrollState()

    var playbackStatus by remember { mutableStateOf<String?>(null) }
    var midiFallback by remember { mutableStateOf(false) }
    var activeStart by remember { mutableStateOf(0L) }
    var loading by remember { mutableStateOf(false) }
    var startJob by remember { mutableStateOf<kotlinx.coroutines.Job?>(null) }
    var paused by remember { mutableStateOf(false) }
    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }
    val playFocus = remember { FocusRequester() }
    val sliderFocus = remember { FocusRequester() }
    val closeFocus = remember { FocusRequester() }
    var loadError by remember { mutableStateOf<String?>(null) }
    var metadata by remember { mutableStateOf<MidiMetadata?>(null) }
    var showMetadata by remember { mutableStateOf(false) }
    var metadataLoading by remember { mutableStateOf(false) }

    LaunchedEffect(playing) {
        while (playing) {
            val state = MidiPreviewBridge.getState()
            paused = state.state == MidiPreviewBridge.STATE_PAUSED
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

    fun startPlayback() {
        if (MidiPreviewBridge.getState().state != MidiPreviewBridge.STATE_PAUSED) {
            playbackStatus = null
            midiFallback = false
            val generation = MidiPreviewBridge.reserveStart()
            activeStart = generation
            loadError = null
            loading = true
            startJob =
                scope.launch {
                    try {
                        val started =
                            withContext(Dispatchers.IO) {
                                if (!MidiPreviewBridge.init(context)) {
                                    loadError = "Could not load the selected soundfont"
                                    return@withContext false
                                }
                                val data = loadBytes()
                                if (data == null) {
                                    loadError = "Could not read $trackName"
                                    return@withContext false
                                }
                                MidiPreviewBridge.startReserved(generation, data, isHmp, sampleRate, hogPath, trackName)
                            }
                        if (started) {
                            val soundfonts = SoundfontStore(context).read()
                            val actualRenderer = MidiPreviewBridge.getState().renderer
                            midiFallback = soundfonts.renderer == "ymfm" && actualRenderer == "sf2"
                            playbackStatus =
                                if (actualRenderer == "ymfm") {
                                    "Playing with ymfm FM"
                                } else {
                                    val name =
                                        soundfonts.fonts.firstOrNull { it.id == soundfonts.selected }?.name
                                            ?: SoundfontCatalog.bundled(context).name
                                    "Playing with MIDI $name"
                                }
                            playing = true
                            loadError = null
                        } else {
                            if (loadError == null) loadError = "Playback failed"
                        }
                    } catch (e: kotlinx.coroutines.CancellationException) {
                        throw e
                    } catch (e: Exception) {
                        loadError = "Playback failed: ${e.message}"
                    } finally {
                        if (activeStart == generation) loading = false
                    }
                }
        } else {
            MidiPreviewBridge.resume()
            playing = true
        }
    }

    val media =
        rememberPreviewMediaSession(
            trackName,
            autoPlay,
            PreviewTransport(
                snapshot = {
                    if (loading) {
                        PreviewSnapshot(PreviewStatus.LOADING)
                    } else {
                        val state = MidiPreviewBridge.getState()
                        nativePreviewSnapshot(state.state, state.positionMs, state.durationMs)
                    }
                },
                start = { startPlayback() },
                pause = { MidiPreviewBridge.pause() },
                stop = {
                    activeStart = 0L
                    startJob?.cancel()
                    MidiPreviewBridge.stop()
                    loading = false
                    playing = false
                    positionMs = 0
                },
                seek = { target ->
                    val duration = MidiPreviewBridge.getState().durationMs
                    if (duration > 0 && MidiPreviewBridge.seek(target.toFloat() / duration)) positionMs = target
                },
                navigate = onSkip,
                cancelPendingNavigation = onCancelPending,
            ),
        )

    AlertDialog(
        modifier = Modifier.repeatVerticalDpadFocus(closeFocus),
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            Column(
                modifier = Modifier.heightIn(max = 460.dp).verticalScroll(contentScroll),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                Text(trackName, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                playbackStatus?.let {
                    Text(it, fontSize = 12.sp)
                }
                if (midiFallback) {
                    Text(
                        "FM is unavailable for this song. Using MIDI soundfont playback instead.",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                    )
                }
                detailLines.forEach { line ->
                    Text(
                        line,
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                if (showMetadata) {
                    val lines = metadata?.let(::midiMetadataPrintout)?.map(::AudioFilePreviewLine)
                    MetadataPrintout(lines, metadataLoading, "No readable MIDI metadata.")
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
                        onClick = { media.toggle() },
                        modifier = Modifier.focusRequester(playFocus).tvFocusBorder(),
                    ) {
                        val label =
                            if (!playing) {
                                "Play"
                            } else {
                                if (paused) "Resume" else "Pause"
                            }
                        Text(label, fontSize = 13.sp)
                    }
                    if (playing) {
                        TextButton(
                            onClick = {
                                media.stop()
                            },
                            modifier = Modifier.tvFocusBorder(),
                        ) {
                            Text("Stop", fontSize = 13.sp)
                        }
                    }
                    if (loadMetadata != null) {
                        TextButton(
                            onClick = {
                                showMetadata = !showMetadata
                                if (showMetadata && metadata == null && !metadataLoading) {
                                    metadataLoading = true
                                    scope.launch(Dispatchers.IO) {
                                        val loaded = loadMetadata()
                                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                                            metadata = loaded
                                            metadataLoading = false
                                        }
                                    }
                                }
                            },
                            modifier = Modifier.tvFocusBorder(),
                        ) { Text(if (showMetadata) "Hide metadata" else "Metadata", fontSize = 13.sp) }
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

internal fun midiMetadataPrintout(metadata: MidiMetadata): List<String> =
    buildList {
        if (metadata.title.isNotBlank()) add("Title: ${metadata.title}")
        if (metadata.composer.isNotBlank()) add("Composer: ${metadata.composer}")
        add("SMF format ${metadata.smf_format}, ${metadata.track_count} tracks, division ${metadata.time_division}")
        if (metadata.inherited_from_midi) {
            add("Inherited from MIDI version: ${metadata.metadata_source_filename}")
        }
        metadata.text_events.forEach { event ->
            add("Track ${event.track_index + 1} ${event.type}: ${event.text}")
        }
        if (metadata.metadata_truncated) add("Metadata output was truncated.")
    }

private fun formatMidiPreviewTime(ms: Int): String {
    val seconds = ms / 1000
    return "%d:%02d".format(seconds / 60, seconds % 60)
}
