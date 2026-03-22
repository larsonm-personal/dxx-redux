package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.util.Log
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ScrollState
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
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.UUID

// Music mode constants -- must match d2/main/digi.h
// MUSIC_TYPE_NONE=0, MUSIC_TYPE_BUILTIN=1, MUSIC_TYPE_REDBOOK=2, MUSIC_TYPE_CUSTOM=3
private const val MUSIC_MODE_MIDI = "midi"
private const val MUSIC_MODE_CD = "cd"
private const val MUSIC_MODE_FILES = "files"

private const val TAG = "DXX-MusicPicker"

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
                                        arrayOf("audio/mpeg", "audio/ogg", "audio/flac", "audio/*"),
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
                onRemove = {
                    audioSrcManager.removeSource(src.id)
                    onSourcesChanged()
                },
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
            modifier = Modifier.weight(1f),
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
                modifier = Modifier.weight(1f),
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
                onClick = {
                    customMgr.removeSet(set.id, deleteFiles = true)
                    onSetsChanged()
                },
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

    val tracks: List<Pair<String, String>> =
        if (musicMode == MUSIC_MODE_CD) {
            // Show enabled redbook sources in order with track numbers
            val sources = audioSrcManager.getEnabledSources()
            if (sources.isEmpty()) {
                listOf("(no sources enabled)" to "")
            } else {
                var trackNum = 1
                sources.flatMap { src ->
                    (1..src.audioTrackCount).map { i ->
                        "Track ${trackNum++}" to src.discLabel
                    }
                }
            }
        } else {
            val merged = customMgr.getMergedTrackList()
            if (merged.isEmpty()) {
                listOf("(no files)" to "")
            } else {
                merged
            }
        }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title, fontSize = 16.sp) },
        text = {
            Column(
                modifier =
                    Modifier
                        .heightIn(max = 400.dp)
                        .verticalScroll(rememberScrollState()),
            ) {
                tracks.forEachIndexed { i, (name, source) ->
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(vertical = 1.dp),
                    ) {
                        Text(
                            "${i + 1}.",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.width(28.dp),
                        )
                        Text(
                            name,
                            fontSize = 11.sp,
                            modifier = Modifier.weight(1f),
                        )
                        if (source.isNotEmpty()) {
                            Text(
                                source,
                                fontSize = 10.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
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
                val dest = File(destDir, fileName)
                ctx.contentResolver.openInputStream(uri)?.use { input ->
                    dest.outputStream().use { output -> input.copyTo(output) }
                }
                imported.add(fileName)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to import: $uri", e)
            }
        }
        if (imported.isNotEmpty()) {
            customMgr.addSet(
                CustomAudioSetManager.AudioSet(
                    id = setId,
                    label = setName,
                    files = imported,
                    enabled = true,
                    order = customMgr.getSets().size,
                ),
            )
            Log.i(TAG, "Imported ${imported.size} files as set '$setName'")
        } else {
            destDir.deleteRecursively()
            withContext(Dispatchers.Main) {
                Toast.makeText(ctx, "No files could be imported", Toast.LENGTH_SHORT).show()
            }
        }
    }
}

private fun resolveFileName(ctx: Context, uri: Uri): String? {
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
