package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.util.Log
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.util.UUID
import java.util.zip.ZipInputStream

// Music mode constants -- must match d2/main/digi.h
// MUSIC_TYPE_NONE=0, MUSIC_TYPE_BUILTIN=1, MUSIC_TYPE_REDBOOK=2, MUSIC_TYPE_CUSTOM=3
private const val MUSIC_MODE_MIDI = "midi"
private const val MUSIC_MODE_CD = "cd"
private const val MUSIC_MODE_FILES = "files"

private const val TAG = "DXX-MusicPicker"

// Duplicated from FingerprintBridge.kt -- both files need it for filtering
private fun isPlaceholderName(name: String): Boolean = name == "[unknown] - [untitled]"

@Composable
fun MusicPickerPage(
    filesDir: File,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
    val scrollState = rememberScrollState()
    val scope = rememberCoroutineScope()

    var musicMode by remember {
        mutableStateOf(prefs.getString("music_mode", MUSIC_MODE_CD) ?: MUSIC_MODE_CD)
    }

    // Redbook source management
    val audioSrcManager = remember { AudioSourceManager(filesDir) }
    var audioSources by remember { mutableStateOf(audioSrcManager.getSources()) }

    // Custom audio set management
    val customMgr = remember { CustomAudioSetManager(filesDir) }
    var customSets by remember { mutableStateOf(customMgr.getSets()) }

    // Track list preview dialog
    var showTrackPreview by remember { mutableStateOf(false) }

    // Audio file import state
    var importingFiles by remember { mutableStateOf(false) }
    var showNameDialog by remember { mutableStateOf(false) }
    var pendingUris by remember { mutableStateOf<List<Uri>>(emptyList()) }
    var pendingSetName by remember { mutableStateOf("") }

    val audioFilePicker =
        rememberLauncherForActivityResult(
            ActivityResultContracts.OpenMultipleDocuments(),
        ) { uris ->
            if (uris.isNotEmpty()) {
                pendingUris = uris
                pendingSetName = "Set ${customSets.size + 1}"
                showNameDialog = true
            }
        }

    fun saveMusicMode(mode: String) {
        musicMode = mode
        prefs.edit().putString("music_mode", mode).apply()
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(16.dp),
        ) {
            // Top bar
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onBack) {
                    Text("< Back", fontSize = 14.sp)
                }
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    "Music",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                )
            }

            Spacer(modifier = Modifier.height(12.dp))

            // Mode selector
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = musicMode == MUSIC_MODE_MIDI,
                    onClick = { saveMusicMode(MUSIC_MODE_MIDI) },
                    label = { Text("MIDI") },
                    modifier = Modifier.weight(1f),
                )
                FilterChip(
                    selected = musicMode == MUSIC_MODE_CD,
                    onClick = { saveMusicMode(MUSIC_MODE_CD) },
                    label = { Text("CD Audio") },
                    modifier = Modifier.weight(1f),
                )
                FilterChip(
                    selected = musicMode == MUSIC_MODE_FILES,
                    onClick = { saveMusicMode(MUSIC_MODE_FILES) },
                    label = { Text("Audio Files") },
                    modifier = Modifier.weight(1f),
                )
            }

            Spacer(modifier = Modifier.height(12.dp))

            Box(modifier = Modifier.weight(1f)) {
                Column(
                    modifier =
                        Modifier
                            .fillMaxSize()
                            .verticalScroll(scrollState),
                ) {
                    when (musicMode) {
                        MUSIC_MODE_MIDI -> MidiSection()
                        MUSIC_MODE_CD ->
                            CdAudioSection(
                                audioSrcManager = audioSrcManager,
                                audioSources = audioSources,
                                onSourcesChanged = { audioSources = audioSrcManager.getSources() },
                                onShowTrackPreview = { showTrackPreview = true },
                            )
                        MUSIC_MODE_FILES ->
                            AudioFilesSection(
                                customMgr = customMgr,
                                customSets = customSets,
                                onSetsChanged = { customSets = customMgr.getSets() },
                                onAddSet = {
                                    audioFilePicker.launch(
                                        arrayOf(
                                            "audio/mpeg",
                                            "audio/ogg",
                                            "audio/flac",
                                            "audio/*",
                                            "application/zip",
                                            "application/x-7z-compressed",
                                            "application/octet-stream",
                                        ),
                                    )
                                },
                                onShowTrackPreview = { showTrackPreview = true },
                                importingFiles = importingFiles,
                            )
                    }
                }
                ScrollArrows(scrollState)
            }
        }
    }

    // Name dialog for new audio file set
    if (showNameDialog) {
        AlertDialog(
            onDismissRequest = {
                showNameDialog = false
                pendingUris = emptyList()
            },
            title = { Text("Name this set") },
            text = {
                OutlinedTextField(
                    value = pendingSetName,
                    onValueChange = { pendingSetName = it },
                    singleLine = true,
                    label = { Text("Set name") },
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        showNameDialog = false
                        val name = pendingSetName.trim().ifEmpty { "Set ${customSets.size + 1}" }
                        val uris = pendingUris
                        pendingUris = emptyList()
                        scope.launch {
                            importingFiles = true
                            importAudioFiles(ctx, filesDir, customMgr, name, uris)
                            customSets = customMgr.getSets()
                            importingFiles = false
                        }
                    },
                    enabled = pendingSetName.isNotBlank(),
                ) {
                    Text("OK")
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    showNameDialog = false
                    pendingUris = emptyList()
                }) {
                    Text("Cancel")
                }
            },
        )
    }

    // Track preview dialog
    if (showTrackPreview) {
        TrackPreviewDialog(
            musicMode = musicMode,
            audioSrcManager = audioSrcManager,
            customMgr = customMgr,
            onDismiss = { showTrackPreview = false },
        )
    }
}

// ── Mode sections ──────────────────────────────────────────────────

@Composable
private fun MidiSection() {
    Text(
        "Uses built-in MIDI music from game files.",
        fontSize = 13.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))
    Text(
        "No additional configuration needed. MIDI music plays from " +
            "HMP/MID files included in the game data.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

@Composable
private fun CdAudioSection(
    audioSrcManager: AudioSourceManager,
    audioSources: List<AudioSourceManager.AudioSource>,
    onSourcesChanged: () -> Unit,
    onShowTrackPreview: () -> Unit,
) {
    Text(
        "Redbook CD audio from disc images (BIN/CUE or GOG/INST).",
        fontSize = 13.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))

    if (audioSources.isEmpty()) {
        Text(
            "No audio sources registered. Import disc images from the " +
                "main setup screen using the file picker.",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 4.dp),
        )
    } else {
        // Confirmation dialog state for source removal
        var removeConfirmId by remember { mutableStateOf<String?>(null) }
        val removeConfirmLabel = audioSources.firstOrNull { it.id == removeConfirmId }?.discLabel

        // Source info dialog state
        var infoSource by remember { mutableStateOf<AudioSourceManager.AudioSource?>(null) }

        if (removeConfirmId != null && removeConfirmLabel != null) {
            AlertDialog(
                onDismissRequest = { removeConfirmId = null },
                title = { Text("Remove source") },
                text = {
                    Column {
                        Text("Remove \"$removeConfirmLabel\"?")
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            "The BIN/CUE disc files will remain on disk",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                },
                confirmButton = {
                    TextButton(onClick = {
                        audioSrcManager.removeSource(removeConfirmId!!)
                        removeConfirmId = null
                        onSourcesChanged()
                    }) { Text("Yes") }
                },
                dismissButton = {
                    TextButton(onClick = { removeConfirmId = null }) { Text("Cancel") }
                },
            )
        }

        // Source info dialog
        infoSource?.let { src ->
            AlertDialog(
                onDismissRequest = { infoSource = null },
                title = { Text("Source Info", fontSize = 16.sp) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(src.discLabel, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                        Text("Audio tracks: ${src.audioTrackCount}", fontSize = 12.sp)
                        Text("Total tracks: ${src.trackCount}", fontSize = 12.sp)
                        Text("CUE: ${src.cuePath}", fontSize = 12.sp)
                        Text("BIN: ${src.binPaths.joinToString(", ")}", fontSize = 12.sp)
                        if (src.discId != "unknown") {
                            Text("Disc ID: ${src.discId}", fontSize = 12.sp)
                        }
                        val matched = src.trackNames.count { (_, v) -> v != "[unknown] - [untitled]" }
                        if (matched > 0) {
                            Text(
                                "$matched/${src.audioTrackCount} tracks identified",
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.primary,
                            )
                        }
                    }
                },
                confirmButton = {
                    TextButton(onClick = { infoSource = null }) { Text("Close") }
                },
            )
        }

        Text(
            "Audio Sources:",
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(bottom = 4.dp),
        )
        audioSources.forEachIndexed { index, src ->
            AudioSourceRow(
                src = src,
                index = index,
                total = audioSources.size,
                onToggle = { checked ->
                    audioSrcManager.setEnabled(src.id, checked)
                    onSourcesChanged()
                },
                onMoveUp = {
                    val ids = audioSources.map { it.id }.toMutableList()
                    ids[index] = ids[index - 1].also { ids[index - 1] = ids[index] }
                    audioSrcManager.reorder(ids)
                    onSourcesChanged()
                },
                onMoveDown = {
                    val ids = audioSources.map { it.id }.toMutableList()
                    ids[index] = ids[index + 1].also { ids[index + 1] = ids[index] }
                    audioSrcManager.reorder(ids)
                    onSourcesChanged()
                },
                onRemove = { removeConfirmId = src.id },
                onInfo = { infoSource = src },
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedButton(
            onClick = onShowTrackPreview,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
        ) {
            Text("Preview Track List", fontSize = 12.sp)
        }
    }
}

@Composable
private fun AudioSourceRow(
    src: AudioSourceManager.AudioSource,
    index: Int,
    total: Int,
    onToggle: (Boolean) -> Unit,
    onMoveUp: () -> Unit,
    onMoveDown: () -> Unit,
    onRemove: () -> Unit,
    onInfo: () -> Unit = {},
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(start = 4.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(
            checked = src.enabled,
            onCheckedChange = onToggle,
            modifier = Modifier.size(20.dp),
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = "${src.discLabel} (${src.audioTrackCount} tracks)",
            fontSize = 12.sp,
            color =
                if (src.enabled) {
                    MaterialTheme.colorScheme.onSurface
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
            modifier = Modifier.weight(1f).clickable(onClick = onInfo),
        )
        if (index > 0) {
            IconButton(
                onClick = onMoveUp,
                modifier = Modifier.size(24.dp),
            ) {
                Icon(Icons.Filled.KeyboardArrowUp, "Move up", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        if (index < total - 1) {
            IconButton(
                onClick = onMoveDown,
                modifier = Modifier.size(24.dp),
            ) {
                Icon(Icons.Filled.KeyboardArrowDown, "Move down", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        TextButton(
            onClick = onRemove,
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
            modifier = Modifier.height(24.dp),
        ) {
            Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
        }
    }
}

@Composable
private fun AudioFilesSection(
    customMgr: CustomAudioSetManager,
    customSets: List<CustomAudioSetManager.AudioSet>,
    onSetsChanged: () -> Unit,
    onAddSet: () -> Unit,
    onShowTrackPreview: () -> Unit,
    importingFiles: Boolean,
) {
    Text(
        "Custom audio files (MP3, OGG, FLAC) used as jukebox music.",
        fontSize = 13.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))

    // Delete confirmation dialog state
    var removeConfirmId by remember { mutableStateOf<String?>(null) }
    val removeConfirmSet = customSets.firstOrNull { it.id == removeConfirmId }

    // Set info dialog state
    var infoSet by remember { mutableStateOf<CustomAudioSetManager.AudioSet?>(null) }

    if (removeConfirmId != null && removeConfirmSet != null) {
        AlertDialog(
            onDismissRequest = { removeConfirmId = null },
            title = { Text("Delete set") },
            text = {
                Column {
                    Text("Delete \"${removeConfirmSet.label}\" and its ${removeConfirmSet.files.size} audio files?")
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "This cannot be undone",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    customMgr.removeSet(removeConfirmId!!, deleteFiles = true)
                    removeConfirmId = null
                    onSetsChanged()
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { removeConfirmId = null }) { Text("Cancel") }
            },
        )
    }

    // Set info dialog
    infoSet?.let { set ->
        AlertDialog(
            onDismissRequest = { infoSet = null },
            title = { Text("Set Info", fontSize = 16.sp) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(set.label, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                    Text("Files: ${set.files.size}", fontSize = 12.sp)
                    val matched = set.trackNames.count { (_, v) -> v != "[unknown] - [untitled]" }
                    if (matched > 0) {
                        Text(
                            "$matched/${set.files.size} tracks identified",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                    if (set.files.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text("Files:", fontSize = 11.sp, fontWeight = FontWeight.SemiBold)
                        set.files.sorted().forEach { f ->
                            val name = set.trackNames[f]
                            val label = if (name != null && !isPlaceholderName(name)) "$f - $name" else f
                            Text(label, fontSize = 10.sp)
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { infoSet = null }) { Text("Close") }
            },
        )
    }

    if (customSets.isEmpty() && !importingFiles) {
        Text(
            "No audio file sets added. Tap \"Add Set\" to import audio files.",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 4.dp),
        )
    }

    customSets.forEachIndexed { index, set ->
        Row(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(start = 4.dp, bottom = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Checkbox(
                checked = set.enabled,
                onCheckedChange = { checked ->
                    customMgr.setEnabled(set.id, checked)
                    onSetsChanged()
                },
                modifier = Modifier.size(20.dp),
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = "${set.label} (${set.files.size} files)",
                fontSize = 12.sp,
                color =
                    if (set.enabled) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
                modifier = Modifier.weight(1f).clickable { infoSet = set },
            )
            if (index > 0) {
                IconButton(
                    onClick = {
                        val ids = customSets.map { it.id }.toMutableList()
                        ids[index] = ids[index - 1].also { ids[index - 1] = ids[index] }
                        customMgr.reorder(ids)
                        onSetsChanged()
                    },
                    modifier = Modifier.size(24.dp),
                ) {
                    Icon(Icons.Filled.KeyboardArrowUp, "Move up", modifier = Modifier.size(16.dp))
                }
            } else {
                Spacer(modifier = Modifier.size(24.dp))
            }
            if (index < customSets.size - 1) {
                IconButton(
                    onClick = {
                        val ids = customSets.map { it.id }.toMutableList()
                        ids[index] = ids[index + 1].also { ids[index + 1] = ids[index] }
                        customMgr.reorder(ids)
                        onSetsChanged()
                    },
                    modifier = Modifier.size(24.dp),
                ) {
                    Icon(Icons.Filled.KeyboardArrowDown, "Move down", modifier = Modifier.size(16.dp))
                }
            } else {
                Spacer(modifier = Modifier.size(24.dp))
            }
            TextButton(
                onClick = { removeConfirmId = set.id },
                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                modifier = Modifier.height(24.dp),
            ) {
                Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
            }
        }
    }

    if (importingFiles) {
        Row(
            modifier = Modifier.padding(start = 4.dp, top = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
            Spacer(modifier = Modifier.width(8.dp))
            Text("Importing files...", fontSize = 12.sp)
        }
    }

    Spacer(modifier = Modifier.height(8.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = onAddSet,
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            enabled = !importingFiles,
        ) {
            Text("Add Set", fontSize = 12.sp)
        }
        if (customSets.isNotEmpty()) {
            OutlinedButton(
                onClick = onShowTrackPreview,
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                modifier = Modifier.height(32.dp),
            ) {
                Text("Preview Track List", fontSize = 12.sp)
            }
        }
    }
}

// ── Track preview dialog ───────────────────────────────────────────

@Composable
private fun TrackPreviewDialog(
    musicMode: String,
    audioSrcManager: AudioSourceManager,
    customMgr: CustomAudioSetManager,
    onDismiss: () -> Unit,
) {
    val title =
        if (musicMode == MUSIC_MODE_CD) "CD Audio Track Order" else "Audio File Playlist"

    // Track info tap state
    var infoTrack by remember { mutableStateOf<CustomAudioSetManager.TrackDetail?>(null) }

    // CD track tap state for mini player
    data class CdTrackInfo(
        val name: String,
        val audioTrackIdx: Int,
        val source: AudioSourceManager.AudioSource,
    )
    var cdPreviewTrack by remember { mutableStateOf<CdTrackInfo?>(null) }

    data class TrackRow(
        val display: String,
        val sourceLabel: String,
        val detail: CustomAudioSetManager.TrackDetail?,
        val cdInfo: CdTrackInfo? = null,
    )

    val tracks: List<TrackRow> =
        if (musicMode == MUSIC_MODE_CD) {
            val sources = audioSrcManager.getEnabledSources()
            if (sources.isEmpty()) {
                listOf(TrackRow("(no sources enabled)", "", null))
            } else {
                var trackNum = 1
                sources.flatMap { src ->
                    val namedTracks = src.trackNames.toSortedMap()
                    (1..src.audioTrackCount).map { i ->
                        val raw =
                            namedTracks.values.elementAtOrNull(i - 1)
                                ?: "Track $trackNum"
                        val name =
                            if (raw == "[unknown] - [untitled]") "Track $trackNum" else raw
                        val info = CdTrackInfo(name, i, src)
                        trackNum++
                        TrackRow(name, src.discLabel, null, info)
                    }
                }
            }
        } else {
            val detailed = customMgr.getDetailedTrackList()
            if (detailed.isEmpty()) {
                listOf(TrackRow("(no files)", "", null))
            } else {
                detailed.map { d ->
                    TrackRow(d.matchedName ?: d.filename, d.setLabel, d)
                }
            }
        }

    val filesDir = LocalContext.current.filesDir

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            val dialogScroll = rememberScrollState()
            Box(modifier = Modifier.heightIn(max = 400.dp)) {
                Column(modifier = Modifier.verticalScroll(dialogScroll)) {
                    tracks.forEachIndexed { i, row ->
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(vertical = 1.dp)
                                    .then(
                                        when {
                                            row.cdInfo != null ->
                                                Modifier.clickable { cdPreviewTrack = row.cdInfo }
                                            row.detail != null ->
                                                Modifier.clickable { infoTrack = row.detail }
                                            else -> Modifier
                                        },
                                    ),
                        ) {
                            Text(
                                "${i + 1}.",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.width(28.dp),
                            )
                            Text(
                                row.display,
                                fontSize = 11.sp,
                                modifier = Modifier.weight(1f),
                            )
                            if (row.sourceLabel.isNotEmpty()) {
                                Text(
                                    row.sourceLabel,
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                }
                ScrollArrows(dialogScroll)
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
    )

    // Track info sub-dialog (audio files)
    infoTrack?.let { track ->
        AlertDialog(
            onDismissRequest = { infoTrack = null },
            title = { Text("Track Info", fontSize = 16.sp) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("File: ${track.filename}", fontSize = 12.sp)
                    Text("Set: ${track.setLabel}", fontSize = 12.sp)
                    if (track.matchedName != null) {
                        Text(
                            "Matched: ${track.matchedName}",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    } else {
                        Text(
                            "No fingerprint match",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { infoTrack = null }) { Text("Close") }
            },
        )
    }

    // CD track mini player dialog
    cdPreviewTrack?.let { info ->
        CdTrackDetailDialog(
            filesDir = filesDir,
            trackName = info.name,
            audioTrackIdx = info.audioTrackIdx,
            source = info.source,
            onDismiss = { cdPreviewTrack = null },
        )
    }
}

// ── CD track detail dialog with mini player ────────────────────────

@Composable
private fun CdTrackDetailDialog(
    filesDir: File,
    trackName: String,
    audioTrackIdx: Int,
    source: AudioSourceManager.AudioSource,
    onDismiss: () -> Unit,
) {
    val ctx = LocalContext.current
    val sampleRate = remember { CdPreviewBridge.getNativeSampleRate(ctx) }

    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }

    // Stop preview when dialog is dismissed
    DisposableEffect(Unit) {
        onDispose { CdPreviewBridge.stop() }
    }

    // Poll playback state while playing or paused
    LaunchedEffect(playing) {
        while (playing) {
            val state = CdPreviewBridge.getState()
            if (!seeking) {
                positionMs = state.positionMs
                durationMs = state.durationMs
            }
            if (state.state == CdPreviewBridge.STATE_STOPPED && durationMs > 0) {
                playing = false
                positionMs = durationMs
            }
            delay(100)
        }
    }

    fun togglePlayback() {
        if (!playing) {
            val binPath = File(filesDir, source.binPaths.first()).absolutePath
            val cuePath = File(filesDir, source.cuePath).absolutePath
            if (CdPreviewBridge.start(binPath, cuePath, audioTrackIdx, sampleRate)) {
                playing = true
            }
        } else {
            val state = CdPreviewBridge.getState()
            if (state.state == CdPreviewBridge.STATE_PLAYING) {
                CdPreviewBridge.pause()
            } else {
                CdPreviewBridge.resume()
            }
        }
    }

    fun formatTime(ms: Int): String {
        val s = ms / 1000
        return "%d:%02d".format(s / 60, s % 60)
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Track Preview", fontSize = 16.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(trackName, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text(
                    "Disc: ${source.discLabel}",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (durationMs > 0) {
                    Text(
                        "Duration: ${formatTime(durationMs)}",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }

                Spacer(modifier = Modifier.height(4.dp))

                // Play/pause button
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    TextButton(onClick = { togglePlayback() }) {
                        val state =
                            if (!playing) {
                                "Play"
                            } else {
                                val s = CdPreviewBridge.getState()
                                if (s.state == CdPreviewBridge.STATE_PAUSED) "Resume" else "Pause"
                            }
                        Text(state, fontSize = 13.sp)
                    }
                    if (playing) {
                        TextButton(onClick = {
                            CdPreviewBridge.stop()
                            playing = false
                            positionMs = 0
                        }) {
                            Text("Stop", fontSize = 13.sp)
                        }
                    }
                }

                // Progress slider
                if (durationMs > 0) {
                    Slider(
                        value = positionMs.toFloat() / durationMs.toFloat(),
                        onValueChange = { frac ->
                            seeking = true
                            positionMs = (frac * durationMs).toInt()
                        },
                        onValueChangeFinished = {
                            CdPreviewBridge.seek(positionMs.toFloat() / durationMs.toFloat())
                            seeking = false
                        },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text(formatTime(positionMs), fontSize = 10.sp)
                        Text(formatTime(durationMs), fontSize = 10.sp)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
    )
}

// ── Scroll arrows (from AdvancedSettingsPage pattern) ──────────────

@Composable
private fun BoxScope.ScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.8f),
            modifier =
                Modifier
                    .align(Alignment.TopEnd)
                    .padding(end = 4.dp, top = 4.dp)
                    .size(28.dp),
        ) {
            Icon(
                Icons.Filled.KeyboardArrowUp,
                contentDescription = "Scroll up",
                modifier = Modifier.padding(4.dp),
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.8f),
            modifier =
                Modifier
                    .align(Alignment.BottomEnd)
                    .padding(end = 4.dp, bottom = 4.dp)
                    .size(28.dp),
        ) {
            Icon(
                Icons.Filled.KeyboardArrowDown,
                contentDescription = "Scroll down",
                modifier = Modifier.padding(4.dp),
            )
        }
    }
}

// ── File import helper ─────────────────────────────────────────────

private val AUDIO_EXTENSIONS = setOf("mp3", "ogg", "flac")
private val ARCHIVE_EXTENSIONS = setOf("zip", "dxa", "7z")

private fun isAudioFile(name: String): Boolean = name.substringAfterLast('.').lowercase() in AUDIO_EXTENSIONS

private fun isArchiveFile(name: String): Boolean = name.substringAfterLast('.').lowercase() in ARCHIVE_EXTENSIONS

private suspend fun importAudioFiles(
    ctx: Context,
    filesDir: File,
    customMgr: CustomAudioSetManager,
    setName: String,
    uris: List<Uri>,
) {
    val setId = UUID.randomUUID().toString().take(8)
    val destDir = customMgr.setDir(setId)

    withContext(Dispatchers.IO) {
        destDir.mkdirs()
        val imported = mutableListOf<String>()
        for (uri in uris) {
            try {
                val fileName = resolveFileName(ctx, uri) ?: "track_${imported.size + 1}.audio"
                if (isArchiveFile(fileName)) {
                    val extracted = extractAudioFromArchive(ctx, uri, destDir)
                    imported.addAll(extracted)
                } else {
                    val dest = File(destDir, fileName)
                    ctx.contentResolver.openInputStream(uri)?.use { input ->
                        dest.outputStream().use { output -> input.copyTo(output) }
                    }
                    if (isAudioFile(fileName)) imported.add(fileName)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to import: $uri", e)
            }
        }
        if (imported.isNotEmpty()) {
            // Run chromaprint matching on imported files
            val trackNames = mutableMapOf<String, String>()
            try {
                FingerprintBridge.ensureDbLoaded(ctx)
                for (f in imported) {
                    val path = File(destDir, f).absolutePath
                    val match = FingerprintBridge.fingerprintAndMatch(path)
                    if (match != null) {
                        trackNames[f] = match.name
                        Log.i(TAG, "Matched '$f' -> '${match.name}' (${match.confidence})")
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Fingerprint matching failed (non-fatal)", e)
            }

            customMgr.addSet(
                CustomAudioSetManager.AudioSet(
                    id = setId,
                    label = setName,
                    files = imported,
                    enabled = true,
                    order = customMgr.getSets().size,
                    trackNames = trackNames,
                ),
            )
            Log.i(TAG, "Imported ${imported.size} files as set '$setName' (${trackNames.size} matched)")
        } else {
            destDir.deleteRecursively()
            withContext(Dispatchers.Main) {
                Toast.makeText(ctx, "No audio files found in import", Toast.LENGTH_SHORT).show()
            }
        }
    }
}

/** Extract audio files from a ZIP/DXA archive, returns filenames written to destDir */
private fun extractAudioFromArchive(
    ctx: Context,
    uri: Uri,
    destDir: File,
): List<String> {
    val extracted = mutableListOf<String>()
    ctx.contentResolver.openInputStream(uri)?.use { raw ->
        ZipInputStream(raw).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val name = entry.name.substringAfterLast('/')
                    if (isAudioFile(name)) {
                        // Avoid path traversal
                        val safeName = name.replace("..", "_").replace('/', '_').replace('\\', '_')
                        val dest = File(destDir, safeName)
                        FileOutputStream(dest).use { out -> zip.copyTo(out) }
                        extracted.add(safeName)
                    }
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
    }
    Log.i(TAG, "Extracted ${extracted.size} audio files from archive")
    return extracted
}

private fun resolveFileName(
    ctx: Context,
    uri: Uri,
): String? {
    // Try DocumentsContract display name first
    ctx.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
        val nameCol = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME)
        if (nameCol >= 0 && cursor.moveToFirst()) {
            return cursor.getString(nameCol)
        }
    }
    // Fall back to last path segment
    return uri.lastPathSegment?.substringAfterLast('/')
}
