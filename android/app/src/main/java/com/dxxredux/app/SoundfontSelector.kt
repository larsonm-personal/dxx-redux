package com.dxxredux.app

import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
fun SoundfontSelector() {
    val context = LocalContext.current
    val store = remember { SoundfontStore(context.filesDir) }
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf(store.read()) }
    var expanded by remember { mutableStateOf(false) }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }

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
            "Used for MIDI previews and the next game launch in both games. Preview a track below to hear your choice.",
            style = MaterialTheme.typography.bodySmall,
        )
        TextButton(onClick = { picker.launch(arrayOf("*/*")) }, enabled = !busy, modifier = Modifier.tvFocusBorder()) {
            Text("Import SF2 soundfont")
        }
        if (busy) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
            Text("Loading soundfont...", style = MaterialTheme.typography.bodySmall)
        }
        error?.let { Text(it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall) }
    }
}
