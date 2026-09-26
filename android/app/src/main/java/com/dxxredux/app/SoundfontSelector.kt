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
    val bundled = remember(context) { SoundfontCatalog.bundled(context) }
    val downloads = remember(context) { SoundfontCatalog.entries(context) }
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
    var infoFromDownloads by remember { mutableStateOf(false) }
    var pendingDelete by remember { mutableStateOf<SoundfontStore.Font?>(null) }
    var rendererInfo by remember { mutableStateOf<String?>(null) }
    var showEq by remember { mutableStateOf(false) }
    val fontName = state.fonts.firstOrNull { it.id == state.selected }?.name ?: bundled.name

    fun selectEq(preset: String) {
        busy = true
        error = null
        scope.launch {
            try {
                withContext(Dispatchers.IO) { MidiPreviewBridge.selectEq(context, preset) }
                state = store.read()
            } catch (e: Exception) {
                error = e.message ?: "Could not change music EQ"
            } finally {
                busy = false
            }
        }
    }

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
                if (key in
                    setOf(
                        SoundfontStore.PREF_RENDERER,
                        SoundfontStore.PREF_SOUNDFONT,
                        SoundfontStore.PREF_REVERB,
                        SoundfontStore.PREF_CHORUS,
                        SoundfontStore.PREF_EQ,
                    )
                ) {
                    state =
                        store.read()
                }
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
        listOf("ymfm" to "AdLib (OPL3) FM", "sf2" to "MIDI soundfont").forEach { (id, label) ->
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
            OutlinedButton(
                onClick = { showEq = true },
                enabled = !busy,
                modifier = Modifier.fillMaxWidth().tvFocusBorder(),
            ) {
                Text("eq opl3")
            }
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
                Text(state.fonts.firstOrNull { it.id == state.selected }?.name ?: bundled.name)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                DropdownMenuItem(text = { Text(bundled.name) }, onClick = {
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
        if (state.renderer == "sf2") {
            OutlinedButton(
                onClick = { showEq = true },
                enabled = !busy,
                modifier = Modifier.fillMaxWidth().tvFocusBorder(),
            ) {
                Text("eq $fontName")
            }
        }
        Text("MIDI soundfont effects", style = MaterialTheme.typography.titleSmall)
        listOf("Reverb" to state.reverb, "Chorus" to state.chorus).forEach { (label, checked) ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(label, modifier = Modifier.weight(1f))
                Switch(checked = checked, enabled = !busy, onCheckedChange = { enabled ->
                    busy = true
                    error = null
                    scope.launch {
                        try {
                            withContext(Dispatchers.IO) {
                                MidiPreviewBridge.selectEffects(
                                    context,
                                    if (label == "Reverb") enabled else state.reverb,
                                    if (label == "Chorus") enabled else state.chorus,
                                )
                            }
                            state = store.read()
                        } catch (e: Exception) {
                            error = e.message ?: "Could not change MIDI effects"
                        } finally {
                            busy = false
                        }
                    }
                })
            }
        }
        Text(
            "Applies to soundfont playback, including AdLib fallback songs.",
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
            enabled = !busy,
            modifier = Modifier.tvFocusBorder(),
        ) {
            Text("Download soundfonts")
        }
        if (downloads.isEmpty()) {
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

    if (showEq) {
        MusicEqDialog(
            name = if (state.renderer == "ymfm") "opl3" else fontName,
            profile = state.eqProfile,
            preset = state.eq,
            busy = busy,
            error = error,
            onSelect = ::selectEq,
            onDismiss = { if (!busy) showEq = false },
        )
    }

    rendererInfo?.let { renderer ->
        AlertDialog(
            onDismissRequest = { rendererInfo = null },
            title = { Text(if (renderer == "ymfm") "AdLib (OPL3) FM" else "MIDI soundfont") },
            text = {
                Text(
                    if (renderer == "ymfm") {
                        "Recreates the original AdLib/Sound Blaster music using an emulated Yamaha OPL chip " +
                            "and the game's FM instruments. It synthesizes sounds electronically, " +
                            "giving them their characteristic retro tone.\n\n" +
                            "HMP/HMQ music is converted to MIDI, parsed by TinyMidiLoader, " +
                            "and rendered through ymfmidi and ymfm's OPL3 emulation."
                    } else {
                        "Plays MIDI using sampled instruments from your selected SoundFont. " +
                            "This corresponds to the original game's General MIDI sound option; " +
                            "changing the SoundFont changes the instrument sounds, much like choosing " +
                            "a different MIDI sound card or module.\n\n" +
                            "HMP music is converted to MIDI, parsed by TinyMidiLoader, " +
                            "and rendered by FluidSynth using your selected SF2 soundfont."
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
                    BundledSoundfontEntry(bundled.name, state.selected.isEmpty()) {
                        showLibrary = false
                        infoFromDownloads = false
                        infoFont = bundled
                    }
                    HorizontalDivider()
                    if (state.fonts.isEmpty()) Text("No downloaded or imported soundfonts.")
                    state.fonts.forEach { font ->
                        Text(font.name, style = MaterialTheme.typography.titleSmall)
                        if (font.id == state.selected) Text("Selected", style = MaterialTheme.typography.bodySmall)
                        Row {
                            TextButton(onClick = {
                                showLibrary = false
                                infoFromDownloads = false
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
            if (infoFromDownloads) showDownloads = true else showLibrary = true
        }
        AlertDialog(
            onDismissRequest = { closeInfo() },
            title = { Text(font.name) },
            text = {
                Column(
                    Modifier.verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    font.download?.let { SoundfontDetails(it, bundled = font.id.isEmpty()) } ?: Text(
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
                            " ${bundled.name} will be selected and MIDI preview will stop."
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
                    BundledSoundfontEntry(bundled.name, state.selected.isEmpty()) {
                        showDownloads = false
                        infoFromDownloads = true
                        infoFont = bundled
                    }
                    HorizontalDivider()
                    downloads.forEach { entry ->
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
                        "Downloads and selects this soundfont. Your AdLib / MIDI soundfont choice stays the same.",
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
private fun BundledSoundfontEntry(
    name: String,
    selected: Boolean,
    onInfo: () -> Unit,
) {
    TextButton(onClick = onInfo, modifier = Modifier.fillMaxWidth().tvFocusBorder()) {
        Column(Modifier.fillMaxWidth()) {
            Text(name, style = MaterialTheme.typography.titleSmall)
            Text("Included with app - Info", style = MaterialTheme.typography.bodySmall)
            if (selected) Text("Selected", style = MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable
private fun SoundfontDetails(
    entry: SoundfontDownload,
    bundled: Boolean = false,
) {
    val uriHandler = LocalUriHandler.current
    var linkError by remember(entry) { mutableStateOf<String?>(null) }
    Text(entry.description)
    SelectionContainer {
        Text(
            "${if (bundled) "Source file URL" else "Download URL"}:\n${entry.url}",
            style = MaterialTheme.typography.bodySmall,
        )
    }
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
