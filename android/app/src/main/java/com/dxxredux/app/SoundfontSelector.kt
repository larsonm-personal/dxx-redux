package com.dxxredux.app

import android.content.Context
import android.content.SharedPreferences
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun SoundfontSelector() {
    val context = LocalContext.current
    val store = remember { SoundfontStore(context) }
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf(store.read()) }
    var expanded by remember { mutableStateOf(false) }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    var showDownloads by remember { mutableStateOf(false) }
    var pendingDownload by remember { mutableStateOf<SoundfontDownload?>(null) }
    var downloadJob by remember { mutableStateOf<Job?>(null) }
    var downloadedBytes by remember { mutableStateOf(0L) }
    var downloadSize by remember { mutableStateOf<Long?>(null) }
    var showLibrary by remember { mutableStateOf(false) }
    var infoFont by remember { mutableStateOf<SoundfontStore.Font?>(null) }
    var pendingDelete by remember { mutableStateOf<SoundfontStore.Font?>(null) }
    var rendererInfo by remember { mutableStateOf<String?>(null) }

    fun delete(font: SoundfontStore.Font) {
        busy = true
        error = null
        scope.launch {
            try {
                withContext(Dispatchers.IO) { MidiPreviewBridge.deleteSoundfont(context, font.id) }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                error = e.message ?: "Could not delete soundfont"
            } finally {
                state = store.read()
                busy = false
                showLibrary = true
            }
        }
    }

    fun download(entry: SoundfontDownload) {
        busy = true
        error = null
        downloadedBytes = 0
        downloadSize = null
        downloadJob =
            scope.launch {
                try {
                    val font =
                        SoundfontDownloader().download(
                            entry,
                            store,
                            requireSpace = { ImportStorageGuard.requireFreeSpace(context.filesDir, it, "soundfont") },
                            validate = { MidiPreviewBridge.validateSoundfont(it.absolutePath) },
                            onProgress = { bytes, size ->
                                downloadedBytes = bytes
                                downloadSize = size
                            },
                        )
                    withContext(Dispatchers.IO) { MidiPreviewBridge.selectSoundfont(context, font.id) }
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Exception) {
                    error = e.message ?: "Could not download soundfont"
                } finally {
                    state = store.read()
                    busy = false
                    downloadJob = null
                }
            }
    }

    DisposableEffect(context, store) {
        val preferences = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        val listener =
            SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
                if (key == SoundfontStore.PREF_RENDERER || key == SoundfontStore.PREF_SOUNDFONT) state = store.read()
            }
        preferences.registerOnSharedPreferenceChangeListener(listener)
        onDispose { preferences.unregisterOnSharedPreferenceChangeListener(listener) }
    }

    fun select(id: String) {
        scope.launch {
            busy = true
            error = null
            try {
                withContext(Dispatchers.IO) { MidiPreviewBridge.selectSoundfont(context, id) }
                state = store.read()
            } catch (e: Exception) {
                error = e.message ?: "Could not change soundfont"
            } finally {
                busy = false
            }
        }
    }

    val picker =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            if (uri != null) {
                scope.launch {
                    busy = true
                    error = null
                    try {
                        val font =
                            withContext(Dispatchers.IO) {
                                val resolver = context.contentResolver
                                val name =
                                    resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use {
                                        if (it.moveToFirst()) it.getString(0) else null
                                    } ?: "Imported soundfont"
                                val size = ImportStorageGuard.queryUriSizeBytes(resolver, uri)
                                require(
                                    size == null || size <= SoundfontStore.MAX_BYTES,
                                ) { "Soundfonts must be 64 MiB or smaller" }
                                ImportStorageGuard.requireFreeSpace(
                                    context.filesDir,
                                    size ?: SoundfontStore.MAX_BYTES,
                                    "soundfont",
                                )
                                resolver.openInputStream(uri)?.use { input ->
                                    store.import(input, name) { MidiPreviewBridge.validateSoundfont(it.absolutePath) }
                                } ?: error("Could not open this file")
                            }
                        withContext(Dispatchers.IO) { MidiPreviewBridge.selectSoundfont(context, font.id) }
                        state = store.read()
                    } catch (e: Exception) {
                        error = e.message ?: "Could not import soundfont"
                        state = store.read()
                    } finally {
                        busy = false
                    }
                }
            }
        }

    Column(modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)) {
        Text("Sound profile", style = MaterialTheme.typography.titleSmall)
        listOf("ymfm" to "AdLib / Sound Blaster FM", "sf2" to "Soundfont").forEach { (id, label) ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(selected = state.renderer == id, enabled = !busy, onClick = {
                    scope.launch {
                        busy = true
                        error = null
                        try {
                            withContext(Dispatchers.IO) { MidiPreviewBridge.selectRenderer(context, id) }
                            state = store.read()
                        } catch (e: Exception) {
                            error = e.message ?: "Could not change renderer"
                        } finally {
                            busy = false
                        }
                    }
                })
                Text(label, modifier = Modifier.weight(1f, fill = false))
                IconButton(onClick = { rendererInfo = id }, modifier = Modifier.tvFocusBorder()) {
                    Icon(Icons.Filled.Info, contentDescription = "About $label")
                }
            }
        }
        if (state.renderer == "ymfm") {
            Text(
                "Uses original instruments for supported D1 and D2 songs. " +
                    "Other MIDI songs use the soundfont below.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
        Box {
            OutlinedButton(
                onClick = { expanded = true },
                enabled = !busy,
                modifier = Modifier.fillMaxWidth().tvFocusBorder(),
            ) {
                Text(state.fonts.firstOrNull { it.id == state.selected }?.name ?: "Bundled soundfont")
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                DropdownMenuItem(text = { Text("Bundled soundfont") }, onClick = {
                    expanded = false
                    select("")
                })
                state.fonts.forEach { font ->
                    DropdownMenuItem(text = { Text(font.name) }, onClick = {
                        expanded = false
                        select(font.id)
                    })
                }
            }
        }
        Text(
            "Saved immediately in Game Preferences for MIDI previews and the next launch of either game.",
            style = MaterialTheme.typography.bodySmall,
        )
        TextButton(onClick = { picker.launch(arrayOf("*/*")) }, enabled = !busy, modifier = Modifier.tvFocusBorder()) {
            Text("Import SF2 soundfont")
        }
        TextButton(onClick = { showLibrary = true }, enabled = !busy, modifier = Modifier.tvFocusBorder()) {
            Text("Manage soundfonts")
        }
        TextButton(
            onClick = { showDownloads = true },
            enabled = !busy && SoundfontCatalog.entries.isNotEmpty(),
            modifier = Modifier.tvFocusBorder(),
        ) {
            Text("Download soundfonts")
        }
        if (SoundfontCatalog.entries.isEmpty()) {
            Text("No soundfont downloads are available yet.", style = MaterialTheme.typography.bodySmall)
        }
        if (busy) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            if (downloadJob != null) {
                val received = ImportStorageGuard.formatMegabytes(downloadedBytes)
                val total = downloadSize?.let { " of ${ImportStorageGuard.formatMegabytes(it)}" } ?: ""
                Text("Downloading soundfont: $received$total", style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { downloadJob?.cancel() }, modifier = Modifier.tvFocusBorder()) { Text("Cancel") }
            } else {
                Text("Loading soundfont...", style = MaterialTheme.typography.bodySmall)
            }
        }
        error?.let { Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall) }
    }

    rendererInfo?.let { renderer ->
        AlertDialog(
            onDismissRequest = { rendererInfo = null },
            title = { Text(if (renderer == "ymfm") "AdLib / Sound Blaster FM" else "SoundFont") },
            text = {
                Text(
                    if (renderer == "ymfm") {
                        "Recreates the original AdLib/Sound Blaster music using an emulated Yamaha OPL chip " +
                            "and the game's FM instruments. It synthesizes sounds electronically, " +
                            "giving them their characteristic retro tone."
                    } else {
                        "Plays MIDI using sampled instruments from your selected SoundFont. " +
                            "This corresponds to the original game's General MIDI sound option; " +
                            "changing the SoundFont changes the instrument sounds, much like choosing " +
                            "a different MIDI sound card or module."
                    },
                    modifier = Modifier.verticalScroll(rememberScrollState()),
                )
            },
            confirmButton = {
                TextButton(onClick = { rendererInfo = null }, modifier = Modifier.tvFocusBorder()) { Text("Close") }
            },
        )
    }

    if (showLibrary) {
        AlertDialog(
            onDismissRequest = { showLibrary = false },
            title = { Text("Saved soundfonts") },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    if (state.fonts.isEmpty()) Text("No downloaded or imported soundfonts.")
                    state.fonts.forEach { font ->
                        Text(font.name, style = MaterialTheme.typography.titleSmall)
                        if (font.id == state.selected) Text("Selected", style = MaterialTheme.typography.bodySmall)
                        Row {
                            TextButton(onClick = {
                                showLibrary = false
                                infoFont = font
                            }, modifier = Modifier.tvFocusBorder()) { Text("Info") }
                            TextButton(onClick = {
                                showLibrary = false
                                pendingDelete = font
                            }, modifier = Modifier.tvFocusBorder()) { Text("Delete") }
                        }
                        HorizontalDivider()
                    }
                    error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                }
            },
            confirmButton = { TextButton(onClick = { showLibrary = false }) { Text("Close") } },
        )
    }
    infoFont?.let { font ->
        fun closeInfo() {
            infoFont = null
            showLibrary = true
        }
        AlertDialog(
            onDismissRequest = { closeInfo() },
            title = { Text(font.name) },
            text = {
                Column(
                    Modifier.verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    font.download?.let { SoundfontDetails(it) } ?: Text(
                        "Imported from a local SF2 file. No source or license information was supplied.",
                    )
                }
            },
            confirmButton = { TextButton(onClick = { closeInfo() }) { Text("Close") } },
        )
    }
    pendingDelete?.let { font ->
        fun cancelDelete() {
            pendingDelete = null
            showLibrary = true
        }
        AlertDialog(
            onDismissRequest = { cancelDelete() },
            title = { Text("Delete ${font.name}?") },
            text = {
                Text(
                    "Removes this soundfont from this device." +
                        if (font.id ==
                            state.selected
                        ) {
                            " The bundled soundfont will be selected and MIDI preview will stop."
                        } else {
                            ""
                        },
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    pendingDelete = null
                    delete(font)
                }, modifier = Modifier.tvFocusBorder()) { Text("Delete") }
            },
            dismissButton = { TextButton(onClick = { cancelDelete() }) { Text("Cancel") } },
        )
    }

    if (showDownloads) {
        AlertDialog(
            onDismissRequest = { showDownloads = false },
            title = { Text("Download soundfonts") },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    SoundfontCatalog.entries.forEach { entry ->
                        TextButton(onClick = {
                            showDownloads = false
                            pendingDownload = entry
                        }, modifier = Modifier.fillMaxWidth().tvFocusBorder()) {
                            Column(Modifier.fillMaxWidth()) {
                                Text(entry.name, style = MaterialTheme.typography.titleSmall)
                                Text(entry.description, style = MaterialTheme.typography.bodySmall)
                            }
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { showDownloads = false }) { Text("Close") } },
        )
    }
    pendingDownload?.let { entry ->
        AlertDialog(
            onDismissRequest = { pendingDownload = null },
            title = { Text("Download ${entry.name}?") },
            text = {
                Column(
                    Modifier.verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    SoundfontDetails(entry)
                    Text(
                        "Downloads and selects this soundfont. Your AdLib / Soundfont choice stays the same.",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    pendingDownload = null
                    download(entry)
                }, modifier = Modifier.tvFocusBorder()) { Text("Download") }
            },
            dismissButton = {
                TextButton(onClick = { pendingDownload = null }, modifier = Modifier.tvFocusBorder()) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun SoundfontDetails(entry: SoundfontDownload) {
    val uriHandler = LocalUriHandler.current
    var linkError by remember(entry) { mutableStateOf<String?>(null) }
    Text(entry.description)
    SelectionContainer { Text("Download URL:\n${entry.url}", style = MaterialTheme.typography.bodySmall) }
    TextButton(onClick = {
        try {
            uriHandler.openUri(entry.websiteUrl)
        } catch (_: Exception) {
            linkError =
                "Could not open the source website"
        }
    }, modifier = Modifier.tvFocusBorder()) { Text("Original website / README:\n${entry.websiteUrl}") }
    linkError?.let { Text(it, color = MaterialTheme.colorScheme.error) }
    SelectionContainer { Text("License:\n${entry.license}") }
}
