package com.dxxredux.app

import android.content.ContentValues
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun AdvancedSettingsPage(
    filesDir: File,
    fileSetManager: FileSetManager,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val prefs = ctx.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val scrollState = rememberScrollState()

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
                    "Advanced Settings",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
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
                    // -- Render Resolution --
                    ResolutionPickerAdvanced(prefs = prefs, filesDir = filesDir)

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Export / Import configs --
                    Text("Config Management", fontWeight = FontWeight.Bold, fontSize = 14.sp)
                    Spacer(modifier = Modifier.height(8.dp))

                    val configImportLauncher =
                        rememberLauncherForActivityResult(
                            contract =
                                androidx.activity.result.contract.ActivityResultContracts
                                    .OpenDocument(),
                        ) { uri ->
                            if (uri == null) return@rememberLauncherForActivityResult
                            val msg = ConfigImportExport.importFromUri(ctx, uri)
                            Toast.makeText(ctx, msg, Toast.LENGTH_LONG).show()
                        }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(
                            onClick = { ConfigImportExport.exportAll(ctx) },
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                            modifier = Modifier.height(32.dp),
                        ) {
                            Text("Export All Configs", fontSize = 12.sp)
                        }
                        OutlinedButton(
                            onClick = { configImportLauncher.launch(arrayOf("application/json", "*/*")) },
                            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                            modifier = Modifier.height(32.dp),
                        ) {
                            Text("Import Config", fontSize = 12.sp)
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Debug Logging --
                    DebugLoggingSection()

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Crash Reports --
                    CrashReportsSection()

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Storage Inspector --
                    StorageInspectorSection(filesDir)

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Dangerous zone --
                    Text("Danger Zone", fontWeight = FontWeight.Bold, fontSize = 14.sp, color = Color(0xFFF44336))
                    Spacer(modifier = Modifier.height(8.dp))

                    // Reset All Controls
                    var showResetDialog by remember { mutableStateOf(false) }
                    OutlinedButton(
                        onClick = { showResetDialog = true },
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                        modifier = Modifier.height(36.dp),
                        colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                    ) {
                        Text("Reset All Controls", fontSize = 12.sp)
                    }
                    if (showResetDialog) {
                        AlertDialog(
                            onDismissRequest = { showResetDialog = false },
                            title = { Text("Reset All Controls") },
                            text = {
                                Text(
                                    "This will reset ALL control bindings to defaults:\n\n" +
                                        "- Touch layout (positions, sizes, bindings)\n" +
                                        "- Physical controller mappings\n" +
                                        "- In-game keyboard, joystick, and mouse settings for every pilot\n\n" +
                                        "The app will restart after reset.",
                                    fontSize = 13.sp,
                                )
                            },
                            confirmButton = {
                                TextButton(onClick = {
                                    File(ctx.filesDir, "controller_config.json").delete()
                                    File(ctx.filesDir, "touch_layout.json").delete()
                                    NativePilotPatcher.nativeResetToDefaults(ctx.filesDir.absolutePath, "d2")
                                    NativePilotPatcher.nativeResetToDefaults(ctx.filesDir.absolutePath, "d1")
                                    showResetDialog = false
                                    android.os.Process.killProcess(android.os.Process.myPid())
                                }) {
                                    Text("Reset & Restart", color = Color(0xFFF44336))
                                }
                            },
                            dismissButton = {
                                TextButton(onClick = { showResetDialog = false }) { Text("Cancel") }
                            },
                        )
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    // Delete All Player Files
                    var showDeletePilotsDialog by remember { mutableStateOf(false) }
                    OutlinedButton(
                        onClick = { showDeletePilotsDialog = true },
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                        modifier = Modifier.height(36.dp),
                        colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                    ) {
                        Text("Delete All Player Files", fontSize = 12.sp)
                    }
                    if (showDeletePilotsDialog) {
                        AlertDialog(
                            onDismissRequest = { showDeletePilotsDialog = false },
                            title = { Text("Delete All Player Files") },
                            text = {
                                Text(
                                    "This will delete ALL pilot files (.plr), extended configs (.plx), " +
                                        "effects (.eff), new game plus (.ngp), and saved games " +
                                        "(.sg*, .mg*) for both Descent 1 and Descent 2 across all " +
                                        "file sets.\n\nThis cannot be undone.\n\n" +
                                        "The app will restart after deletion.",
                                    fontSize = 13.sp,
                                )
                            },
                            confirmButton = {
                                TextButton(onClick = {
                                    val deleted = fileSetManager.deleteAllPilotFiles()
                                    showDeletePilotsDialog = false
                                    Toast.makeText(ctx, "Deleted $deleted file(s)", Toast.LENGTH_SHORT).show()
                                    android.os.Process.killProcess(android.os.Process.myPid())
                                }) {
                                    Text("Delete & Restart", color = Color(0xFFF44336))
                                }
                            },
                            dismissButton = {
                                TextButton(onClick = { showDeletePilotsDialog = false }) { Text("Cancel") }
                            },
                        )
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // Restart App
                    Button(
                        onClick = { android.os.Process.killProcess(android.os.Process.myPid()) },
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary),
                        modifier = Modifier.fillMaxWidth().height(44.dp),
                    ) {
                        Text("Restart App", fontSize = 14.sp)
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                }
                ScrollArrows(scrollState)
            }
        }
    }
}

@Composable
private fun DebugLoggingSection() {
    val ctx = LocalContext.current
    val categoryStates =
        remember {
            mutableStateListOf(*Array(DebugLogCategory.COUNT) { DebugLog.isCategoryEnabled(ctx, it) })
        }
    var logFiles by remember { mutableStateOf(DebugLog.listLogFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Debug Logging", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Log categories to files for debugging. Enable only what you need.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))

    for (cat in 0 until DebugLogCategory.COUNT) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(DebugLogCategory.labels[cat], fontSize = 13.sp)
            Spacer(modifier = Modifier.weight(1f))
            Switch(
                checked = categoryStates[cat],
                onCheckedChange = { on ->
                    DebugLog.setCategoryEnabled(ctx, cat, on)
                    categoryStates[cat] = on
                    logFiles = DebugLog.listLogFiles(ctx)
                },
            )
        }
    }

    if (logFiles.isNotEmpty()) {
        Spacer(modifier = Modifier.height(8.dp))
        Text("Log Files (${logFiles.size})", fontSize = 12.sp, fontWeight = FontWeight.Medium)
        Spacer(modifier = Modifier.height(4.dp))
        for (file in logFiles) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(
                    modifier = Modifier.weight(1f).clickable { openTextFile(ctx, file) },
                ) {
                    Text(file.name, fontSize = 11.sp, color = MaterialTheme.colorScheme.primary)
                    val sizeKb = file.length() / 1024
                    val date = dateFmt.format(Date(file.lastModified()))
                    Text(
                        "$date -- ${sizeKb}KB",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                OutlinedButton(
                    onClick = { saveToDownloads(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Save", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = { DebugLog.shareLogFile(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Share", fontSize = 11.sp)
                }
            }
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedButton(
            onClick = { showDeleteDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
        ) {
            Text("Delete All Logs", fontSize = 12.sp)
        }
        if (showDeleteDialog) {
            AlertDialog(
                onDismissRequest = { showDeleteDialog = false },
                title = { Text("Delete All Logs") },
                text = { Text("Delete all debug log files? This cannot be undone.") },
                confirmButton = {
                    TextButton(onClick = {
                        DebugLog.deleteAllLogs(ctx)
                        logFiles = emptyList()
                        showDeleteDialog = false
                    }) {
                        Text("Delete", color = Color(0xFFF44336))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
                },
            )
        }
    }
}

@Composable
private fun CrashReportsSection() {
    val ctx = LocalContext.current
    var crashFiles by remember { mutableStateOf(CrashLog.listCrashFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Crash Reports", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Crash reports are captured automatically when the app or game crashes.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )

    if (crashFiles.isNotEmpty()) {
        Spacer(modifier = Modifier.height(8.dp))
        Text("Reports (${crashFiles.size})", fontSize = 12.sp, fontWeight = FontWeight.Medium)
        Spacer(modifier = Modifier.height(4.dp))
        for (file in crashFiles) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(
                    modifier = Modifier.weight(1f).clickable { openTextFile(ctx, file) },
                ) {
                    Text(file.name, fontSize = 11.sp, color = MaterialTheme.colorScheme.primary)
                    val sizeKb = file.length() / 1024
                    val date = dateFmt.format(Date(file.lastModified()))
                    Text(
                        "$date -- ${sizeKb}KB",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                OutlinedButton(
                    onClick = { saveToDownloads(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Save", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = { CrashLog.shareCrashFile(ctx, file) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Share", fontSize = 11.sp)
                }
            }
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedButton(
            onClick = { showDeleteDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
        ) {
            Text("Delete All Reports", fontSize = 12.sp)
        }
        if (showDeleteDialog) {
            AlertDialog(
                onDismissRequest = { showDeleteDialog = false },
                title = { Text("Delete All Crash Reports") },
                text = { Text("Delete all crash report files? This cannot be undone.") },
                confirmButton = {
                    TextButton(onClick = {
                        CrashLog.deleteAllCrashFiles(ctx)
                        crashFiles = emptyList()
                        showDeleteDialog = false
                    }) {
                        Text("Delete", color = Color(0xFFF44336))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDeleteDialog = false }) { Text("Cancel") }
                },
            )
        }
    } else {
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "No crash reports.",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun ResolutionPickerAdvanced(
    prefs: android.content.SharedPreferences,
    filesDir: File,
) {
    val ctx = LocalContext.current
    val options =
        remember {
            computeResolutionOptions(ctx)
        }
    val validValues = remember(options) { options.map { it.first }.toSet() }
    val defaultValue = remember(options) { options.firstOrNull()?.first ?: "640x480" }
    var selected by remember {
        val stored = prefs.getString("render_resolution", null) ?: ""
        mutableStateOf(if (stored in validValues) stored else defaultValue)
    }

    Text("Render Resolution", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    options.forEach { (value, label) ->
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(vertical = 2.dp),
        ) {
            RadioButton(
                selected = selected == value,
                onClick = {
                    selected = value
                    prefs.edit().putString("render_resolution", value).apply()
                    updateDescentCfgResolution(filesDir, value)
                },
            )
            Text(text = label, fontSize = 13.sp, modifier = Modifier.padding(start = 4.dp))
        }
    }
    Text(
        "Takes effect on next launch",
        fontSize = 11.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

/** Compute resolution options as fractions of the device's real screen size. */
internal fun computeResolutionOptions(ctx: android.content.Context): List<Pair<String, String>> {
    val wm = ctx.getSystemService(android.content.Context.WINDOW_SERVICE) as android.view.WindowManager
    val (rawW, rawH) =
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            val bounds = wm.currentWindowMetrics.bounds
            Pair(bounds.width(), bounds.height())
        } else {
            @Suppress("DEPRECATION")
            val size = android.graphics.Point()
            @Suppress("DEPRECATION")
            wm.defaultDisplay.getRealSize(size)
            Pair(size.x, size.y)
        }
    // Normalize to landscape (width >= height)
    val screenW = maxOf(rawW, rawH)
    val screenH = minOf(rawW, rawH)

    val labels = listOf("Full" to 1, "1/2" to 2, "1/3" to 3, "1/4" to 4)
    val result = mutableListOf<Pair<String, String>>()
    for ((label, divisor) in labels) {
        // Round to nearest even to avoid odd-pixel issues
        val w = (screenW / divisor + 1) and 0x7FFFFFFE
        val h = (screenH / divisor + 1) and 0x7FFFFFFE
        if (w < 640 || h < 480) break
        val value = "${w}x$h"
        result.add(value to "$label ($value)")
    }
    // Always offer at least 640x480 as a fallback
    if (result.isEmpty()) {
        result.add("640x480" to "Low (640x480)")
    }
    return result
}

@Composable
private fun StorageInspectorSection(filesDir: File) {
    val ctx = LocalContext.current
    var showFilesDialog by remember { mutableStateOf(false) }
    var showSafDialog by remember { mutableStateOf(false) }

    Text("Storage Inspector", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "View files stored by the app and SAF (Storage Access Framework) links",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))

    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = { showFilesDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
        ) {
            Text("View App Storage Files", fontSize = 12.sp)
        }
        OutlinedButton(
            onClick = { showSafDialog = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
        ) {
            Text("View SAF Links", fontSize = 12.sp)
        }
    }

    if (showFilesDialog) {
        val fileEntries =
            remember {
                filesDir
                    .walkTopDown()
                    .filter { it.isFile }
                    .map { f ->
                        val rel = f.relativeTo(filesDir).path
                        val size = f.length()
                        rel to size
                    }.sortedBy { it.first }
                    .toList()
            }
        val totalSize = remember(fileEntries) { fileEntries.sumOf { it.second } }

        AlertDialog(
            onDismissRequest = { showFilesDialog = false },
            title = { Text("App Storage Files (${fileEntries.size})") },
            text = {
                Column {
                    Text(
                        "Total: ${formatSize(totalSize)}",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    LazyColumn(modifier = Modifier.heightIn(max = 400.dp)) {
                        items(fileEntries) { (path, size) ->
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                            ) {
                                Text(
                                    path,
                                    fontSize = 11.sp,
                                    modifier = Modifier.weight(1f),
                                )
                                Text(
                                    formatSize(size),
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showFilesDialog = false }) { Text("Close") }
            },
        )
    }

    if (showSafDialog) {
        data class SafEntry(
            val label: String,
            val uri: String,
            val accessible: Boolean,
        )

        val safEntries =
            remember {
                val entries = mutableListOf<SafEntry>()
                // Custom audio referenced URIs
                try {
                    val customMgr = CustomAudioSetManager(filesDir)
                    for (set in customMgr.getSets()) {
                        for ((filename, uriStr) in set.referencedUris) {
                            val ok =
                                try {
                                    ctx.contentResolver.openInputStream(Uri.parse(uriStr))?.close()
                                    true
                                } catch (_: Exception) {
                                    false
                                }
                            entries.add(SafEntry("Audio: $filename (${set.label})", uriStr, ok))
                        }
                    }
                } catch (_: Exception) {
                }
                // CD audio SAF sources
                try {
                    val srcMgr = AudioSourceManager(filesDir)
                    for (src in srcMgr.getSources()) {
                        src.binContentUri?.let { uriStr ->
                            val ok =
                                try {
                                    ctx.contentResolver.openFileDescriptor(Uri.parse(uriStr), "r")?.close()
                                    true
                                } catch (_: Exception) {
                                    false
                                }
                            entries.add(SafEntry("CD BIN: ${src.discLabel}", uriStr, ok))
                        }
                        src.cueContentUri?.let { uriStr ->
                            val ok =
                                try {
                                    ctx.contentResolver.openInputStream(Uri.parse(uriStr))?.close()
                                    true
                                } catch (_: Exception) {
                                    false
                                }
                            entries.add(SafEntry("CD CUE: ${src.discLabel}", uriStr, ok))
                        }
                    }
                } catch (_: Exception) {
                }
                // Persistable URI permissions
                val persisted = ctx.contentResolver.persistedUriPermissions
                for (perm in persisted) {
                    val uriStr = perm.uri.toString()
                    if (entries.none { it.uri == uriStr }) {
                        entries.add(SafEntry("Permission", uriStr, perm.isReadPermission))
                    }
                }
                entries.toList()
            }

        AlertDialog(
            onDismissRequest = { showSafDialog = false },
            title = { Text("SAF Links (${safEntries.size})") },
            text = {
                if (safEntries.isEmpty()) {
                    Text("No SAF links", fontSize = 12.sp)
                } else {
                    LazyColumn(modifier = Modifier.heightIn(max = 400.dp)) {
                        items(safEntries) { entry ->
                            Column(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(entry.label, fontSize = 12.sp, fontWeight = FontWeight.Medium)
                                    Spacer(modifier = Modifier.width(8.dp))
                                    Text(
                                        if (entry.accessible) "OK" else "BROKEN",
                                        fontSize = 10.sp,
                                        color =
                                            if (entry.accessible) {
                                                MaterialTheme.colorScheme.primary
                                            } else {
                                                Color(0xFFF44336)
                                            },
                                    )
                                }
                                Text(
                                    entry.uri,
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    maxLines = 2,
                                )
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showSafDialog = false }) { Text("Close") }
            },
        )
    }
}

private fun formatSize(bytes: Long): String =
    when {
        bytes < 1024 -> "$bytes B"
        bytes < 1024 * 1024 -> "${bytes / 1024} KB"
        else -> String.format(Locale.US, "%.1f MB", bytes / (1024.0 * 1024.0))
    }

@Composable
private fun BoxScope.ScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            modifier = Modifier.align(Alignment.TopCenter).padding(top = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowUp,
                contentDescription = "Scroll up",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = "Scroll down",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

private const val FILE_PROVIDER_AUTHORITY = "com.dxxredux.app.fileprovider"

private fun openTextFile(
    context: android.content.Context,
    file: File,
) {
    try {
        val viewDir = File(context.cacheDir, "file_view")
        viewDir.mkdirs()
        val copy = File(viewDir, file.name)
        file.copyTo(copy, overwrite = true)
        val uri =
            androidx.core.content.FileProvider
                .getUriForFile(context, FILE_PROVIDER_AUTHORITY, copy)
        val intent =
            Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(uri, "text/plain")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            }
        context.startActivity(intent)
    } catch (e: Exception) {
        Toast.makeText(context, "No text viewer available", Toast.LENGTH_SHORT).show()
    }
}

private fun saveToDownloads(
    context: android.content.Context,
    file: File,
) {
    try {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val values =
                ContentValues().apply {
                    put(MediaStore.Downloads.DISPLAY_NAME, file.name)
                    put(MediaStore.Downloads.MIME_TYPE, "text/plain")
                }
            val uri =
                context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                    ?: throw Exception("MediaStore insert failed")
            context.contentResolver.openOutputStream(uri)?.use { out ->
                file.inputStream().use { it.copyTo(out) }
            }
        } else {
            @Suppress("DEPRECATION")
            val dlDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            dlDir.mkdirs()
            file.copyTo(File(dlDir, file.name), overwrite = true)
        }
        Toast.makeText(context, "Saved to Downloads/${file.name}", Toast.LENGTH_SHORT).show()
    } catch (e: Exception) {
        Toast.makeText(context, "Save failed: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}
