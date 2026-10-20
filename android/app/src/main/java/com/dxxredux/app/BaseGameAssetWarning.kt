package com.dxxredux.app

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import java.io.File
import com.dxxredux.app.NavigationAlertDialog as AlertDialog

@Composable
internal fun BaseGameAssetWarning(
    setDir: File,
    refreshTrigger: Int,
    importing: Boolean,
    onCleared: () -> Unit,
) {
    var audits by remember(setDir) { mutableStateOf(emptyList<BaseGameAssetAudit>()) }
    var reviewed by remember(setDir) { mutableStateOf<BaseGameAssetAudit?>(null) }
    var error by remember(setDir) { mutableStateOf<String?>(null) }
    var clearing by remember(setDir) { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    LaunchedEffect(setDir, refreshTrigger, importing) {
        if (importing) {
            audits = emptyList()
            return@LaunchedEffect
        }
        try {
            audits = listOf("d1", "d2").map { auditBaseGameAssets(setDir, it) }.filter { it.reasons.isNotEmpty() }
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (failure: Exception) {
            error = "Could not check game editions: ${failure.message}"
        }
    }
    for (audit in audits) {
        Card(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
            Column(Modifier.padding(12.dp)) {
                Text("Mixed ${audit.title} game files", style = MaterialTheme.typography.titleSmall)
                audit.reasons.forEach { Text(it) }
                TextButton(onClick = {
                    error = null
                    reviewed = audit
                }, enabled = !importing && !clearing) {
                    Text("Review and clear game files")
                }
            }
        }
    }
    error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
    reviewed?.let { audit ->
        AlertDialog(
            onDismissRequest = { if (!clearing) reviewed = null },
            title = { Text("Clear ${audit.title} game files?") },
            text = {
                Column(Modifier.heightIn(max = 320.dp).verticalScroll(rememberScrollState())) {
                    Text(
                        "Clear these game assets from \"${setDir.name}\", then import one edition again. " +
                            "Pilots, saves, settings, added missions and music are preserved. " +
                            "Linked source files stay in their original locations.",
                    )
                    audit.assets.forEach { Text(it.name + if (it.sourceUri == null) "" else " (link)") }
                }
            },
            confirmButton = {
                TextButton(
                    enabled = !clearing && !importing,
                    onClick = {
                        clearing = true
                        scope.launch {
                            try {
                                clearBaseGameAssets(setDir, audit)
                                reviewed = null
                                audits = emptyList()
                                error = null
                                onCleared()
                            } catch (cancelled: CancellationException) {
                                throw cancelled
                            } catch (failure: Exception) {
                                error = failure.message ?: "Could not clear game files"
                                reviewed = null
                            } finally {
                                clearing = false
                            }
                        }
                    },
                ) { Text(if (clearing) "Clearing..." else "Clear game files") }
            },
            dismissButton = {
                TextButton(onClick = { reviewed = null }, enabled = !clearing) { Text("Cancel") }
            },
        )
    }
}
