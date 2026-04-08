package com.dxxredux.app

import android.content.Context
import android.media.MediaPlayer
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
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.apache.commons.compress.archivers.sevenz.SevenZFile
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
    val initialFocus = remember { FocusRequester() }
    LaunchedEffect(Unit) { initialFocus.requestFocus() }

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
    var showAddToSetDialog by remember { mutableStateOf(false) }
    var pendingUris by remember { mutableStateOf<List<Uri>>(emptyList()) }

    val audioFilePicker =
        rememberLauncherForActivityResult(
            ActivityResultContracts.OpenMultipleDocuments(),
        ) { uris ->
            if (uris.isNotEmpty()) {
                pendingUris = uris
                showAddToSetDialog = true
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
                TextButton(onClick = onBack, modifier = Modifier.focusRequester(initialFocus)) {
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
                        MUSIC_MODE_MIDI -> MidiSection(filesDir)
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

    // Add-to-set dialog for audio file import
    if (showAddToSetDialog) {
        AddToSetDialog(
            existingSets = customSets,
            defaultName = "Set ${customSets.size + 1}",
            selectedUris = pendingUris,
            onDismiss = {
                showAddToSetDialog = false
                pendingUris = emptyList()
            },
            onConfirm = { targetSetId, newName, copyToStorage ->
                showAddToSetDialog = false
                val uris = pendingUris
                pendingUris = emptyList()
                scope.launch {
                    importingFiles = true
                    importAudioFiles(ctx, filesDir, customMgr, newName, uris, targetSetId, copyToStorage)
                    customSets = customMgr.getSets()
                    importingFiles = false
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

/**
 * Reusable dialog for importing audio files into a new or existing set.
 * Used by both the Music tab picker and the game file picker in SetupActivity.
 *
 * @param existingSets current sets (empty = force "Create new set")
 * @param defaultName pre-filled name for new sets
 * @param selectedUris URIs being imported (used to detect archive-only imports)
 * @param onDismiss called when dialog is cancelled
 * @param onConfirm called with (targetSetId or null for new, setName, copyToStorage)
 */
@Composable
fun AddToSetDialog(
    existingSets: List<CustomAudioSetManager.AudioSet>,
    defaultName: String,
    selectedUris: List<Uri> = emptyList(),
    onDismiss: () -> Unit,
    onConfirm: (targetSetId: String?, newName: String, copyToStorage: Boolean) -> Unit,
) {
    // "Create new set" is index 0, existing sets follow
    val createNewIdx = 0
    var selectedIdx by remember { mutableIntStateOf(createNewIdx) }
    var newName by remember { mutableStateOf(defaultName) }
    var dropdownExpanded by remember { mutableStateOf(false) }

    // Archives (zip/7z/dxa) must always be extracted -- only show "copy" for raw audio
    val ctx = LocalContext.current
    val hasRawAudio =
        remember(selectedUris) {
            selectedUris.any { uri ->
                val name = resolveFileName(ctx, uri) ?: ""
                isAudioFile(name)
            }
        }
    var copyToStorage by remember { mutableStateOf(true) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Import Audio Files") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (existingSets.isNotEmpty()) {
                    // Destination selector
                    Text("Add to:", fontSize = 13.sp)
                    Box {
                        OutlinedButton(
                            onClick = { dropdownExpanded = true },
                            modifier = Modifier.fillMaxWidth(),
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp),
                        ) {
                            Text(
                                if (selectedIdx == createNewIdx) {
                                    "Create new set"
                                } else {
                                    existingSets[selectedIdx - 1].label
                                },
                                modifier = Modifier.weight(1f),
                                fontSize = 13.sp,
                            )
                            Icon(Icons.Filled.KeyboardArrowDown, "Expand", Modifier.size(16.dp))
                        }
                        DropdownMenu(
                            expanded = dropdownExpanded,
                            onDismissRequest = { dropdownExpanded = false },
                        ) {
                            DropdownMenuItem(
                                text = { Text("Create new set") },
                                onClick = {
                                    selectedIdx = createNewIdx
                                    dropdownExpanded = false
                                },
                            )
                            existingSets.forEachIndexed { i, set ->
                                DropdownMenuItem(
                                    text = { Text("${set.label} (${set.files.size} files)") },
                                    onClick = {
                                        selectedIdx = i + 1
                                        dropdownExpanded = false
                                    },
                                )
                            }
                        }
                    }
                }
                // Name field (only when creating new)
                if (selectedIdx == createNewIdx) {
                    OutlinedTextField(
                        value = newName,
                        onValueChange = { newName = it },
                        singleLine = true,
                        label = { Text("Set name") },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                // Copy checkbox -- only for raw audio; archives always extract
                if (hasRawAudio) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.clickable { copyToStorage = !copyToStorage },
                    ) {
                        Checkbox(
                            checked = copyToStorage,
                            onCheckedChange = { copyToStorage = it },
                            modifier = Modifier.size(20.dp),
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text("Copy files to app storage", fontSize = 13.sp)
                    }
                    if (!copyToStorage) {
                        Text(
                            "Files will be referenced in place. If the original files " +
                                "are moved or deleted, playback will fail",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        },
        confirmButton = {
            val canConfirm = selectedIdx != createNewIdx || newName.isNotBlank()
            TextButton(
                onClick = {
                    val targetId = if (selectedIdx == createNewIdx) null else existingSets[selectedIdx - 1].id
                    val name = newName.trim().ifEmpty { defaultName }
                    onConfirm(targetId, name, copyToStorage)
                },
                enabled = canConfirm,
            ) { Text("OK") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MidiSection(filesDir: File) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val sampleRate = remember { MidiPreviewBridge.getNativeSampleRate(ctx) }

    var enumResult by remember { mutableStateOf<MidiEnumerationBridge.EnumerationResult?>(null) }
    var enumerating by remember { mutableStateOf(false) }
    var selectedSource by remember { mutableStateOf<MidiEnumerationBridge.SourceInfo?>(null) }
    var previewTrack by remember { mutableStateOf<MidiEnumerationBridge.TrackInfo?>(null) }
    var previewSource by remember { mutableStateOf<MidiEnumerationBridge.SourceInfo?>(null) }

    // Initialize TSF and enumerate on first composition
    LaunchedEffect(Unit) {
        enumerating = true
        withContext(Dispatchers.IO) {
            MidiPreviewBridge.init(ctx)
            val setDir = FileSetManager(filesDir).let { it.getSetDir(it.getActive()) }
            enumResult = MidiEnumerationBridge.enumerateTracks(setDir.absolutePath)
        }
        enumerating = false
        // Auto-select first source
        enumResult?.sources?.firstOrNull()?.let { selectedSource = it }
    }

    Text(
        "MIDI music from game data files (HMP format, played via SoundFont synth)",
        fontSize = 13.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )

    Spacer(modifier = Modifier.height(8.dp))

    if (enumerating) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
            Spacer(modifier = Modifier.width(8.dp))
            Text("Scanning game files...", fontSize = 12.sp)
        }
        return
    }

    val sources = enumResult?.sources ?: emptyList()
    if (sources.isEmpty()) {
        Text(
            "No MIDI tracks found. Import game data files first.",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        return
    }

    // Source selector dropdown
    var sourceExpanded by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(
        expanded = sourceExpanded,
        onExpandedChange = { sourceExpanded = it },
    ) {
        OutlinedTextField(
            value = selectedSource?.label ?: "Select source",
            onValueChange = {},
            readOnly = true,
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = sourceExpanded) },
            modifier = Modifier.menuAnchor().fillMaxWidth(),
            textStyle = LocalTextStyle.current.copy(fontSize = 13.sp),
        )
        ExposedDropdownMenu(expanded = sourceExpanded, onDismissRequest = { sourceExpanded = false }) {
            sources.forEach { src ->
                DropdownMenuItem(
                    text = {
                        Text(
                            "${src.label} (${src.tracks.size} tracks)",
                            fontSize = 13.sp,
                        )
                    },
                    onClick = {
                        selectedSource = src
                        sourceExpanded = false
                    },
                )
            }
        }
    }

    Spacer(modifier = Modifier.height(8.dp))

    // Track list
    val currentSource = selectedSource
    if (currentSource != null && currentSource.tracks.isNotEmpty()) {
        Text(
            "${currentSource.tracks.size} tracks",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.height(4.dp))

        currentSource.tracks.forEachIndexed { idx, track ->
            Surface(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .clickable {
                            previewTrack = track
                            previewSource = currentSource
                        },
                color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f),
                shape = MaterialTheme.shapes.small,
            ) {
                Row(
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "${idx + 1}.",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.width(28.dp),
                    )
                    Text(track.filename, fontSize = 13.sp, modifier = Modifier.weight(1f))
                    if (track.duration_ms > 0) {
                        val s = track.duration_ms / 1000
                        Text(
                            "%d:%02d".format(s / 60, s % 60),
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
            Spacer(modifier = Modifier.height(2.dp))
        }
    }

    // Preview dialog
    if (previewTrack != null && previewSource != null) {
        MidiTrackPreviewDialog(
            filesDir = filesDir,
            track = previewTrack!!,
            source = previewSource!!,
            sampleRate = sampleRate,
            onDismiss = {
                MidiPreviewBridge.stop()
                previewTrack = null
                previewSource = null
            },
        )
    }
}

@Composable
private fun MidiTrackPreviewDialog(
    filesDir: File,
    track: MidiEnumerationBridge.TrackInfo,
    source: MidiEnumerationBridge.SourceInfo,
    sampleRate: Int,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()

    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }
    var loadError by remember { mutableStateOf<String?>(null) }

    DisposableEffect(Unit) {
        onDispose { MidiPreviewBridge.stop() }
    }

    // Poll playback state
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
                val data = MidiPreviewBridge.readHogEntry(source.hog, track.filename)
                if (data == null) {
                    loadError = "Could not read ${track.filename} from HOG"
                    return@launch
                }
                val isHmp = track.filename.lowercase().endsWith(".hmp")
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

    fun formatTime(ms: Int): String {
        val s = ms / 1000
        return "%d:%02d".format(s / 60, s % 60)
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("MIDI Preview", fontSize = 16.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(track.filename, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text(
                    "Source: ${source.label}",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (track.duration_ms > 0) {
                    Text(
                        "Duration: ${formatTime(track.duration_ms)}",
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
                    TextButton(onClick = { togglePlayback() }) {
                        val label =
                            if (!playing) {
                                "Play"
                            } else {
                                val s = MidiPreviewBridge.getState()
                                if (s.state == MidiPreviewBridge.STATE_PAUSED) "Resume" else "Pause"
                            }
                        Text(label, fontSize = 13.sp)
                    }
                    if (playing) {
                        TextButton(onClick = {
                            MidiPreviewBridge.stop()
                            playing = false
                            positionMs = 0
                        }) {
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
                            MidiPreviewBridge.seek(positionMs.toFloat() / durationMs.toFloat())
                        }
                        seeking = false
                    },
                    enabled = durationMs > 0,
                    modifier = Modifier.fillMaxWidth(),
                )
                if (durationMs > 0) {
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
            val filesDir = LocalContext.current.filesDir
            val ctx = LocalContext.current
            val removeSource = audioSources.firstOrNull { it.id == removeConfirmId }
            val isSafSource = removeSource?.binContentUri != null
            // Check if source files are inside the app data dir
            val filesInAppDir =
                !isSafSource &&
                    removeSource?.let { src ->
                        src.binPaths.any { File(filesDir, it).exists() } ||
                            File(filesDir, src.cuePath).exists()
                    } ?: false
            AlertDialog(
                onDismissRequest = { removeConfirmId = null },
                title = { Text("Remove source") },
                text = {
                    Column {
                        Text("Remove \"$removeConfirmLabel\"?")
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            when {
                                isSafSource ->
                                    "Source reference will be removed. Original files on external storage are not affected"
                                filesInAppDir ->
                                    "Extracted audio files in app storage will be deleted. Re-import from original source to restore"
                                else ->
                                    "The BIN/CUE disc files will remain on disk"
                            },
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                },
                confirmButton = {
                    TextButton(onClick = {
                        val src = removeSource
                        if (isSafSource && src != null) {
                            // Release persistable URI permissions
                            for (uriStr in listOfNotNull(src.binContentUri, src.cueContentUri)) {
                                try {
                                    ctx.contentResolver.releasePersistableUriPermission(
                                        android.net.Uri.parse(uriStr),
                                        android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
                                    )
                                } catch (_: SecurityException) {
                                    // already released
                                }
                            }
                            // Delete local CUE copy only
                            val cueFile = File(filesDir, src.cuePath)
                            if (cueFile.exists()) cueFile.delete()
                        } else if (filesInAppDir && src != null) {
                            // Delete files if they're in app data dir
                            val deletedDirs = mutableSetOf<File>()
                            for (bin in src.binPaths) {
                                val f = File(filesDir, bin)
                                if (f.exists()) {
                                    deletedDirs.add(f.parentFile!!)
                                    f.delete()
                                }
                            }
                            val cueFile = File(filesDir, src.cuePath)
                            if (cueFile.exists()) {
                                deletedDirs.add(cueFile.parentFile!!)
                                cueFile.delete()
                            }
                            // Clean up empty parent dirs (e.g. sets/<name>/)
                            for (dir in deletedDirs) {
                                var d = dir
                                while (d != filesDir && d.isDirectory && (d.list()?.isEmpty() == true)) {
                                    d.delete()
                                    d = d.parentFile ?: break
                                }
                            }
                        }
                        audioSrcManager.removeSource(removeConfirmId!!)
                        removeConfirmId = null
                        onSourcesChanged()
                    }) { Text(if (filesInAppDir) "Delete" else "Remove") }
                },
                dismissButton = {
                    TextButton(onClick = { removeConfirmId = null }) { Text("Cancel") }
                },
            )
        }

        // Source info dialog
        infoSource?.let { src ->
            val filesDir = LocalContext.current.filesDir
            AlertDialog(
                onDismissRequest = { infoSource = null },
                title = { Text("Source Info", fontSize = 16.sp) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(src.discLabel, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                        Text("Audio tracks: ${src.audioTrackCount}", fontSize = 12.sp)
                        Text("Total tracks: ${src.trackCount}", fontSize = 12.sp)
                        if (src.binContentUri != null) {
                            Text("Source: SAF reference (not copied)", fontSize = 12.sp)
                            Text("CUE: ${src.cuePath} (local copy)", fontSize = 12.sp)
                            Text("BIN: ${src.binPaths.joinToString(", ")}", fontSize = 12.sp)
                        } else {
                            Text("CUE: ${File(filesDir, src.cuePath).absolutePath}", fontSize = 12.sp)
                            Text(
                                "BIN: ${src.binPaths.joinToString(", ") { File(filesDir, it).absolutePath }}",
                                fontSize = 12.sp,
                            )
                        }
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
        val copiedCount = removeConfirmSet.files.size - removeConfirmSet.referencedUris.size
        val refCount = removeConfirmSet.referencedUris.size
        val hasOnlyRefs = copiedCount <= 0
        AlertDialog(
            onDismissRequest = { removeConfirmId = null },
            title = { Text(if (hasOnlyRefs) "Remove set" else "Delete set") },
            text = {
                Column {
                    if (hasOnlyRefs) {
                        Text("Remove \"${removeConfirmSet.label}\"? ($refCount referenced files will be unlinked)")
                    } else if (refCount > 0) {
                        Text("Remove \"${removeConfirmSet.label}\"?")
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            "$copiedCount copied file(s) will be deleted. $refCount referenced file(s) will be unlinked",
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    } else {
                        Text("Delete \"${removeConfirmSet.label}\" and its ${removeConfirmSet.files.size} audio files?")
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        if (hasOnlyRefs) {
                            "Source reference will be removed. Original files on external storage are not affected"
                        } else {
                            "Extracted audio files in app storage will be deleted. Re-import from original source to restore"
                        },
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
                }) { Text(if (hasOnlyRefs) "Remove" else "Delete") }
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

    // Track info sub-dialog (audio files) with mini player
    infoTrack?.let { track ->
        AudioFileDetailDialog(
            filesDir = filesDir,
            track = track,
            onDismiss = { infoTrack = null },
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
            val cuePath = File(filesDir, source.cuePath).absolutePath
            val started =
                if (source.binContentUri != null) {
                    // SAF source: open fd via content resolver
                    val uri = android.net.Uri.parse(source.binContentUri)
                    val pfd = ctx.contentResolver.openFileDescriptor(uri, "r")
                    if (pfd != null) {
                        val ok = CdPreviewBridge.startFd(pfd.detachFd(), cuePath, audioTrackIdx, sampleRate)
                        ok
                    } else {
                        false
                    }
                } else {
                    // Local source: use file path
                    val binPath = File(filesDir, source.binPaths.first()).absolutePath
                    CdPreviewBridge.start(binPath, cuePath, audioTrackIdx, sampleRate)
                }
            if (started) playing = true
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
                Text(
                    "Track $audioTrackIdx",
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

                // Progress slider (shown immediately, disabled until duration known)
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
                            CdPreviewBridge.seek(positionMs.toFloat() / durationMs.toFloat())
                        }
                        seeking = false
                    },
                    enabled = durationMs > 0,
                    modifier = Modifier.fillMaxWidth(),
                )
                if (durationMs > 0) {
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

// ── Audio file detail dialog with mini player ──────────────────────

@Composable
private fun AudioFileDetailDialog(
    filesDir: File,
    track: CustomAudioSetManager.TrackDetail,
    onDismiss: () -> Unit,
) {
    val audioFile = File(File(filesDir, CustomAudioSetManager.MUSIC_DIR), "${track.setId}/${track.filename}")
    var player by remember { mutableStateOf<MediaPlayer?>(null) }
    var playing by remember { mutableStateOf(false) }
    var positionMs by remember { mutableIntStateOf(0) }
    var durationMs by remember { mutableIntStateOf(0) }
    var seeking by remember { mutableStateOf(false) }

    DisposableEffect(Unit) {
        onDispose {
            player?.release()
        }
    }

    // Poll playback position
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
                Log.e(TAG, "Failed to play ${audioFile.name}: ${e.message}")
            }
        } else if (p.isPlaying) {
            p.pause()
        } else {
            p.start()
            playing = true
        }
    }

    fun formatTime(ms: Int): String {
        val s = ms / 1000
        return "%d:%02d".format(s / 60, s % 60)
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Track Info", fontSize = 16.sp) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("File: ${track.filename}", fontSize = 12.sp)
                Text("Set: ${track.setLabel}", fontSize = 12.sp)
                Text(
                    "Path: ${CustomAudioSetManager.MUSIC_DIR}/${track.setId}/${track.filename}",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (track.matchedName != null) {
                    val detail =
                        buildString {
                            append("Matched: ${track.matchedName}")
                            if (track.trackNum != null) append(" (Track ${track.trackNum})")
                            if (track.confidence != null) append(" [${"%d".format((track.confidence * 100).toInt())}%]")
                        }
                    Text(
                        detail,
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
                            TextButton(onClick = {
                                player?.stop()
                                player?.release()
                                player = null
                                playing = false
                                positionMs = 0
                                durationMs = 0
                            }) {
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
                        modifier = Modifier.fillMaxWidth(),
                    )
                    if (durationMs > 0) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            Text(formatTime(positionMs), fontSize = 10.sp)
                            Text(formatTime(durationMs), fontSize = 10.sp)
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
                    .align(Alignment.TopCenter)
                    .padding(top = 4.dp)
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
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 4.dp)
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

internal suspend fun importAudioFiles(
    ctx: Context,
    filesDir: File,
    customMgr: CustomAudioSetManager,
    setName: String,
    uris: List<Uri>,
    targetSetId: String? = null,
    copyToStorage: Boolean = true,
) {
    val setId = targetSetId ?: UUID.randomUUID().toString().take(8)
    val destDir = customMgr.setDir(setId)

    withContext(Dispatchers.IO) {
        if (copyToStorage) destDir.mkdirs()
        val imported = mutableListOf<String>()
        val referencedUris = mutableMapOf<String, String>()
        for (uri in uris) {
            try {
                val fileName = resolveFileName(ctx, uri) ?: "track_${imported.size + 1}.audio"
                if (isArchiveFile(fileName)) {
                    if (copyToStorage) {
                        val extracted = extractAudioFromArchive(ctx, uri, destDir, fileName)
                        imported.addAll(extracted)
                    }
                    // Archives are always extracted (copied). Can't reference archive contents
                } else if (isAudioFile(fileName)) {
                    if (copyToStorage) {
                        val dest = File(destDir, fileName)
                        ctx.contentResolver.openInputStream(uri)?.use { input ->
                            dest.outputStream().use { output -> input.copyTo(output) }
                        }
                    } else {
                        // Take persistable URI permission so we can read later
                        try {
                            ctx.contentResolver.takePersistableUriPermission(
                                uri,
                                android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
                            )
                        } catch (e: SecurityException) {
                            Log.w(TAG, "Could not persist URI permission for $fileName", e)
                        }
                        referencedUris[fileName] = uri.toString()
                    }
                    imported.add(fileName)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to import: $uri", e)
            }
        }
        if (imported.isNotEmpty()) {
            // Run chromaprint matching on imported files
            val trackNames = mutableMapOf<String, String>()
            val trackConfidences = mutableMapOf<String, Float>()
            val trackNumbers = mutableMapOf<String, Int>()

            fun recordMatch(
                f: String,
                match: FingerprintBridge.MatchResult,
            ) {
                trackNames[f] = match.name
                trackConfidences[f] = match.confidence
                trackNumbers[f] = match.trackNum
            }
            try {
                FingerprintBridge.ensureDbLoaded(ctx)
                if (copyToStorage) {
                    for (f in imported) {
                        val path = File(destDir, f).absolutePath
                        val match = FingerprintBridge.fingerprintAndMatch(path)
                        if (match != null) {
                            recordMatch(f, match)
                            Log.i(TAG, "Matched '$f' -> '${match.name}' (${match.confidence})")
                        }
                    }
                } else {
                    // Fingerprint SAF-referenced files via content URI
                    for (f in imported) {
                        val uriStr = referencedUris[f] ?: continue
                        val uri = android.net.Uri.parse(uriStr)
                        val match = FingerprintBridge.fingerprintAndMatch(ctx.contentResolver, uri)
                        if (match != null) {
                            recordMatch(f, match)
                            Log.i(TAG, "Matched ref '$f' -> '${match.name}' (${match.confidence})")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Fingerprint matching failed (non-fatal)", e)
            }

            if (targetSetId != null) {
                // Append to existing set
                customMgr.addFilesToSet(
                    targetSetId,
                    imported,
                    referencedUris,
                    trackNames,
                    trackConfidences,
                    trackNumbers,
                )
                Log.i(TAG, "Added ${imported.size} files to existing set '$targetSetId'")
            } else {
                customMgr.addSet(
                    CustomAudioSetManager.AudioSet(
                        id = setId,
                        label = setName,
                        files = imported,
                        enabled = true,
                        order = customMgr.getSets().size,
                        trackNames = trackNames,
                        trackConfidences = trackConfidences,
                        trackNumbers = trackNumbers,
                        referencedUris = referencedUris,
                    ),
                )
                Log.i(TAG, "Imported ${imported.size} files as set '$setName' (${trackNames.size} matched)")
            }
        } else {
            if (copyToStorage) destDir.deleteRecursively()
            withContext(Dispatchers.Main) {
                Toast.makeText(ctx, "No audio files found in import", Toast.LENGTH_SHORT).show()
            }
        }
    }
}

/** Extract audio files from a ZIP/DXA/7z archive, returns filenames written to destDir */
private fun extractAudioFromArchive(
    ctx: Context,
    uri: Uri,
    destDir: File,
    fileName: String = "",
): List<String> {
    val ext = fileName.substringAfterLast('.', "").lowercase()
    return if (ext == "7z") {
        extract7zAudio(ctx, uri, destDir)
    } else {
        extractZipAudio(ctx, uri, destDir)
    }
}

private fun extractZipAudio(
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
    Log.i(TAG, "Extracted ${extracted.size} audio files from ZIP archive")
    return extracted
}

private fun extract7zAudio(
    ctx: Context,
    uri: Uri,
    destDir: File,
): List<String> {
    val extracted = mutableListOf<String>()
    // SevenZFile requires a seekable file; copy content URI to temp
    val tmpFile = File(destDir, ".tmp_7z_import")
    try {
        ctx.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(tmpFile).use { output -> input.copyTo(output) }
        }
        SevenZFile.builder().setFile(tmpFile).get().use { szf ->
            var entry = szf.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val name = entry.name.substringAfterLast('/')
                    if (isAudioFile(name)) {
                        val safeName = name.replace("..", "_").replace('/', '_').replace('\\', '_')
                        val dest = File(destDir, safeName)
                        FileOutputStream(dest).use { out ->
                            val buf = ByteArray(8192)
                            while (true) {
                                val n = szf.read(buf)
                                if (n <= 0) break
                                out.write(buf, 0, n)
                            }
                        }
                        extracted.add(safeName)
                    }
                }
                entry = szf.nextEntry
            }
        }
    } finally {
        tmpFile.delete()
    }
    Log.i(TAG, "Extracted ${extracted.size} audio files from 7z archive")
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
