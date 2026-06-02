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
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private data class StorageFileEntry(
    val file: File,
    val location: String,
    val relativePath: String,
    val absolutePath: String,
    val purpose: String,
    val size: Long,
    val helperSymlinkTargetName: String? = null,
) {
    val isHelperSymlink: Boolean
        get() = helperSymlinkTargetName != null
}

private data class StorageFileScanResult(
    val entries: List<StorageFileEntry>,
    val totalSize: Long,
)

private const val STORAGE_FILE_PAGE_SIZE = 200

private fun annotateStorageHelperSymlinks(entries: List<StorageFileEntry>): List<StorageFileEntry> {
    val helperTargets = mutableMapOf<String, String>()

    entries
        .groupBy { entry -> entry.location to entry.relativePath.lowercase(Locale.US) }
        .values
        .forEach { group ->
            if (group.size < 2) return@forEach
            val primary =
                group
                    .filter { it.size > 1L }
                    .maxWithOrNull(compareBy<StorageFileEntry> { it.size }.thenBy { it.relativePath })
                    ?: return@forEach

            group
                .filter { it.absolutePath != primary.absolutePath && it.size <= 1L }
                .forEach { helperTargets[it.absolutePath] = primary.file.name }
        }

    return entries.map { entry ->
        entry.copy(helperSymlinkTargetName = helperTargets[entry.absolutePath])
    }
}

private fun storageFileNameComparator(): Comparator<StorageFileEntry> =
    compareBy<StorageFileEntry> { it.location }
        .thenBy { it.relativePath.lowercase(Locale.US) }
        .thenBy { if (it.isHelperSymlink) 1 else 0 }
        .thenBy { it.relativePath }

private fun buildCdSourceSafLabel(source: AudioSourceManager.AudioSource): String {
    val safBinCount = source.binContentUriList().count { !isLocalCdContentPath(it) }
    val hasSafCue = source.cueContentUri?.let { !isLocalCdContentPath(it) } == true
    val summary =
        when {
            hasSafCue && safBinCount > 0 -> {
                val noun = if (safBinCount == 1) "bin file" else "bin files"
                "cue + $safBinCount $noun"
            }

            hasSafCue -> {
                "cue"
            }

            safBinCount > 0 -> {
                val noun = if (safBinCount == 1) "bin file" else "bin files"
                "$safBinCount $noun"
            }

            else -> {
                null
            }
        }

    return if (summary != null) {
        "CD Source: ${source.discLabel} ($summary)"
    } else {
        "CD Source: ${source.discLabel}"
    }
}

private fun scanStorageFiles(filesDir: File): StorageFileScanResult {
    val importRoot = ImportLocationManager(filesDir).getActiveRoot()
    val helperArtifacts = getSafLinkedHelperArtifactPaths(filesDir, AudioSourceManager(filesDir).getSources())
    val entries = mutableListOf<StorageFileEntry>()

    fun addTree(
        root: File,
        location: String,
        importedRoot: Boolean,
    ) {
        if (!root.exists()) return
        root
            .walkTopDown()
            .filter { it.isFile }
            .filterNot { location == "internal" && it.absolutePath in helperArtifacts }
            .forEach { file ->
                val rel = file.relativeTo(root).path
                entries.add(
                    StorageFileEntry(
                        file = file,
                        location = location,
                        relativePath = rel,
                        absolutePath = file.absolutePath,
                        purpose = launcherStorageFilePurpose(file, rel, importedRoot),
                        size = file.length(),
                    ),
                )
            }
    }

    addTree(filesDir, "internal", false)
    if (!isUnderDirectory(importRoot, filesDir)) {
        addTree(importRoot, "external", true)
    }

    val distinctEntries = entries.distinctBy { it.absolutePath }
    val annotatedEntries = annotateStorageHelperSymlinks(distinctEntries)
    return StorageFileScanResult(
        entries = annotatedEntries,
        totalSize = annotatedEntries.sumOf { it.size },
    )
}

@Composable
fun AdvancedSettingsPage(
    filesDir: File,
    fileSetManager: FileSetManager,
    isGameReady: (String) -> Boolean,
    controllerFocusActive: Boolean = true,
    onPlayInputDemo: (StagedInputDemo) -> Unit,
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)

    val ctx = LocalContext.current
    val scrollState = rememberScrollState()
    val initialFocus = remember { FocusRequester() }
    RequestLauncherControllerFocus(initialFocus, controllerFocusActive)

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxSize()
                    .safeDrawingPadding()
                    .padding(16.dp)
                    .repeatVerticalDpadFocus(),
        ) {
            // Top bar
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onBack, modifier = Modifier.focusRequester(initialFocus).tvFocusBorder()) {
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

                    // -- Newly-Recorded Demos --
                    RecordedInputDemosSection(filesDir, fileSetManager, isGameReady, onPlayInputDemo)

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Storage Inspector --
                    StorageInspectorSection(filesDir)

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(16.dp))

                    // -- Imported Files Location --
                    ImportLocationSection(filesDir)

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
                                        "The app will restart after rese.",
                                    fontSize = 13.sp,
                                )
                            },
                            confirmButton = {
                                TextButton(onClick = {
                                    File(ctx.filesDir, "controller_config.json").delete()
                                    File(ctx.filesDir, "touch_layout.json").delete()
                                    ControllerConfigSlotRepository.clear(ctx)
                                    TouchLayoutSlotRepository.clear(ctx)
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

                    // Clear All Game Data
                    var showClearGameDataDialog by remember { mutableStateOf(false) }
                    OutlinedButton(
                        onClick = { showClearGameDataDialog = true },
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                        modifier = Modifier.height(36.dp),
                        colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                    ) {
                        Text("Clear All Game Data", fontSize = 12.sp)
                    }
                    if (showClearGameDataDialog) {
                        AlertDialog(
                            onDismissRequest = { showClearGameDataDialog = false },
                            title = { Text("Clear All Game Data") },
                            text = {
                                Text(
                                    "This will remove imported game data from every file set and delete all mods.\n\n" +
                                        "Files added via file picker (leave-in-place) will be unlinked " +
                                        "but not deleted from their original location.\n\n" +
                                        "Pilot files, saved games, and control settings will be kept.\n\n" +
                                        "The app will restart after clearing data",
                                    fontSize = 13.sp,
                                )
                            },
                            confirmButton = {
                                TextButton(onClick = {
                                    val retainedSafUris =
                                        CustomAudioSetManager(ctx.filesDir)
                                            .getSets()
                                            .flatMap { it.referencedUris.values }
                                    val audioSourceManager = AudioSourceManager(ctx.filesDir)
                                    val audioCleared = audioSourceManager.getSources().size
                                    val setsCleared =
                                        fileSetManager.clearAllGameDataPreservingPlayers(
                                            context = ctx,
                                            retainedTrackedUris = retainedSafUris,
                                        )
                                    audioSourceManager.clearAll(
                                        context = ctx,
                                        retainedTrackedUris = retainedSafUris,
                                    )
                                    val modsCleared = ModManager(ctx.filesDir).clearAllMods()
                                    showClearGameDataDialog = false
                                    Toast
                                        .makeText(
                                            ctx,
                                            "Cleared $setsCleared set(s), removed $modsCleared mod(s), cleared $audioCleared CD source(s)",
                                            Toast.LENGTH_SHORT,
                                        ).show()
                                    android.os.Process.killProcess(android.os.Process.myPid())
                                }) {
                                    Text("Clear & Restart", color = Color(0xFFF44336))
                                }
                            },
                            dismissButton = {
                                TextButton(onClick = { showClearGameDataDialog = false }) { Text("Cancel") }
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
                                        "The app will restart after deletion",
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
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    val categoryStates =
        remember {
            mutableStateListOf(*Array(DebugLogCategory.COUNT) { DebugLog.isCategoryEnabled(ctx, it) })
        }
    var logFiles by remember { mutableStateOf(DebugLog.listLogFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    var transferProgress by remember { mutableStateOf<LauncherCopyProgress?>(null) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Debug Logging Categories", fontWeight = FontWeight.Bold, fontSize = 14.sp)
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
                modifier = Modifier.tvFocusBorder(),
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
                    modifier =
                        Modifier
                            .weight(1f)
                            .fillMaxWidth()
                            .tvFocusBorder()
                            .clickable {
                                scope.launch {
                                    try {
                                        val uri =
                                            withContext(Dispatchers.IO) {
                                                copyFileToCache(ctx, file, "file_view") { progress ->
                                                    mainHandler.post { transferProgress = progress }
                                                }
                                            }
                                        transferProgress = null
                                        openTextFile(ctx, uri)
                                    } catch (e: Exception) {
                                        transferProgress = null
                                        Toast.makeText(ctx, "Open failed: ${e.message}", Toast.LENGTH_SHORT).show()
                                    }
                                }
                            }.padding(horizontal = 4.dp, vertical = 2.dp),
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
                    onClick = {
                        scope.launch {
                            val ok =
                                withContext(Dispatchers.IO) {
                                    saveToDownloads(ctx, file) { progress ->
                                        mainHandler.post { transferProgress = progress }
                                    }
                                }
                            transferProgress = null
                            Toast
                                .makeText(
                                    ctx,
                                    if (ok) "Saved to Downloads/${file.name}" else "Save failed",
                                    Toast.LENGTH_SHORT,
                                ).show()
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Save", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = {
                        scope.launch {
                            try {
                                val uri =
                                    withContext(Dispatchers.IO) {
                                        copyFileToCache(ctx, file, "debuglog_exports") { progress ->
                                            mainHandler.post { transferProgress = progress }
                                        }
                                    }
                                transferProgress = null
                                shareTextFile(ctx, uri, "Share Debug Log")
                            } catch (e: Exception) {
                                transferProgress = null
                                Toast.makeText(ctx, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
                            }
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Share", fontSize = 11.sp)
                }
            }
        }
        FileTransferProgress(transferProgress)
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
                text = { Text("Delete all debug log files? This cannot be undone") },
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
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var crashFiles by remember { mutableStateOf(CrashLog.listCrashFiles(ctx)) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    var transferProgress by remember { mutableStateOf<LauncherCopyProgress?>(null) }
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }

    Text("Crash Reports", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Crash reports are captured automatically when the launcher or game crashes",
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
                    modifier =
                        Modifier.weight(1f).clickable {
                            scope.launch {
                                try {
                                    val uri =
                                        withContext(Dispatchers.IO) {
                                            copyFileToCache(ctx, file, "file_view") { progress ->
                                                mainHandler.post { transferProgress = progress }
                                            }
                                        }
                                    transferProgress = null
                                    openTextFile(ctx, uri)
                                } catch (e: Exception) {
                                    transferProgress = null
                                    Toast.makeText(ctx, "Open failed: ${e.message}", Toast.LENGTH_SHORT).show()
                                }
                            }
                        },
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
                    onClick = {
                        scope.launch {
                            val ok =
                                withContext(Dispatchers.IO) {
                                    saveToDownloads(ctx, file) { progress ->
                                        mainHandler.post { transferProgress = progress }
                                    }
                                }
                            transferProgress = null
                            Toast
                                .makeText(
                                    ctx,
                                    if (ok) "Saved to Downloads/${file.name}" else "Save failed",
                                    Toast.LENGTH_SHORT,
                                ).show()
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Save", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = {
                        scope.launch {
                            try {
                                val uri =
                                    withContext(Dispatchers.IO) {
                                        copyFileToCache(ctx, file, "crashlog_exports") { progress ->
                                            mainHandler.post { transferProgress = progress }
                                        }
                                    }
                                transferProgress = null
                                shareTextFile(ctx, uri, "Share Crash Report")
                            } catch (e: Exception) {
                                transferProgress = null
                                Toast.makeText(ctx, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
                            }
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Share", fontSize = 11.sp)
                }
            }
        }
        FileTransferProgress(transferProgress)
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
                text = { Text("Delete all crash report files? This cannot be undone") },
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
            "No crash reports",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun RecordedInputDemosSection(
    filesDir: File,
    fileSetManager: FileSetManager,
    isGameReady: (String) -> Boolean,
    onPlayInputDemo: (StagedInputDemo) -> Unit,
) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    val activeSetName = fileSetManager.getActive()
    val dateFmt = remember { SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US) }
    var demos by remember { mutableStateOf(InputDemoManager.listStagedDemos(filesDir)) }
    var transferProgress by remember { mutableStateOf<LauncherCopyProgress?>(null) }
    var showDeleteAllDialog by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<StagedInputDemo?>(null) }
    var installTarget by remember { mutableStateOf<StagedInputDemo?>(null) }
    var installName by remember { mutableStateOf("") }

    val sharedPrefs = remember { ctx.getSharedPreferences("launcher_prefs", android.content.Context.MODE_PRIVATE) }
    var recordPerFrameState by remember {
        mutableStateOf(
            sharedPrefs.getBoolean(PREF_DEMO_RECORD_PER_FRAME_STATE, false),
        )
    }

    fun refresh() {
        demos = InputDemoManager.listStagedDemos(filesDir)
    }

    Text("Newly-Recorded Demos", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Quick-recorded .dximdemo files from d1x-redux and d2x-redux. Play launches the staged input demo directly, and paired .rngtrace.jsonl and .dem sidecars still export with the demo and follow it into the active set ($activeSetName)",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )

    Spacer(modifier = Modifier.height(8.dp))
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text("Record per-frame state", fontSize = 12.sp, fontWeight = FontWeight.Medium)
            Text(
                "Include detailed state snapshots and events in demos (larger files)",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Checkbox(
            checked = recordPerFrameState,
            onCheckedChange = { newValue ->
                recordPerFrameState = newValue
                sharedPrefs.edit().putBoolean(PREF_DEMO_RECORD_PER_FRAME_STATE, newValue).apply()
            },
            modifier = Modifier.size(24.dp),
        )
    }

    if (demos.isEmpty()) {
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            "No newly-recorded demos",
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        return
    }

    Spacer(modifier = Modifier.height(8.dp))
    Text("Recordings (${demos.size})", fontSize = 12.sp, fontWeight = FontWeight.Medium)
    Spacer(modifier = Modifier.height(4.dp))

    for (demo in demos) {
        val gameReady = isGameReady(demo.game)
        Column(
            modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(demo.file.name, fontSize = 11.sp, color = MaterialTheme.colorScheme.primary)
                    Text(
                        "${demo.game.uppercase(Locale.US)}  ${demo.mission}  level ${demo.level}",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurface,
                    )
                    Text(
                        "${dateFmt.format(
                            Date(demo.file.lastModified()),
                        )}  ${formatSize(
                            demo.file.length(),
                        )}  ${formatDurationMillis(demo.durationMillis)}  ${demo.frameCount} frames",
                        fontSize = 10.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (demo.traceFile != null) {
                        Text(
                            "RNG trace available: ${demo.traceFile.name}",
                            fontSize = 10.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    if (demo.classicDemoFile != null) {
                        Text(
                            "Classic demo available: ${demo.classicDemoFile.name}",
                            fontSize = 10.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    if (!demo.headerReadable) {
                        Text(
                            "Header unreadable. Save, add, and delete still work",
                            fontSize = 10.sp,
                            color = Color(0xFFFF9800),
                        )
                    }
                    if (!gameReady) {
                        Text(
                            "${demo.game.uppercase(Locale.US)} data missing. Play is disabled",
                            fontSize = 10.sp,
                            color = Color(0xFFFF9800),
                        )
                    }
                }
                OutlinedButton(
                    onClick = {
                        scope.launch {
                            val exportedFiles = InputDemoManager.exportFiles(demo)
                            val ok =
                                withContext(Dispatchers.IO) {
                                    exportedFiles.all { exportFile ->
                                        saveToDownloads(
                                            context = ctx,
                                            file = exportFile,
                                            mimeType = exportMimeType(exportFile),
                                        ) { progress ->
                                            mainHandler.post { transferProgress = progress }
                                        }
                                    }
                                }
                            transferProgress = null
                            Toast
                                .makeText(
                                    ctx,
                                    if (ok) {
                                        when {
                                            demo.traceFile != null && demo.classicDemoFile != null -> {
                                                "Saved demo, RNG trace, and classic demo to Downloads"
                                            }

                                            demo.traceFile != null -> {
                                                "Saved demo and RNG trace to Downloads"
                                            }

                                            demo.classicDemoFile != null -> {
                                                "Saved demo and classic demo to Downloads"
                                            }

                                            else -> {
                                                "Saved to Downloads/${demo.file.name}"
                                            }
                                        }
                                    } else {
                                        "Save failed"
                                    },
                                    Toast.LENGTH_SHORT,
                                ).show()
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Save", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = {
                        scope.launch {
                            try {
                                val exportedFiles = InputDemoManager.exportFiles(demo)
                                val uris =
                                    withContext(Dispatchers.IO) {
                                        exportedFiles.map { exportFile ->
                                            copyFileToCache(ctx, exportFile, "inputdemo_exports") { progress ->
                                                mainHandler.post { transferProgress = progress }
                                            }
                                        }
                                    }
                                transferProgress = null
                                if (uris.size == 1) {
                                    shareFile(
                                        ctx,
                                        uris.first(),
                                        "Share Recorded Demo",
                                        exportMimeType(exportedFiles.first()),
                                    )
                                } else {
                                    shareFiles(ctx, uris, "Share Recorded Demo", "*/*")
                                }
                            } catch (e: Exception) {
                                transferProgress = null
                                Toast.makeText(ctx, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
                            }
                        }
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Share", fontSize = 11.sp)
                }
            }
            Spacer(modifier = Modifier.height(4.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(
                    onClick = { onPlayInputDemo(demo) },
                    enabled = gameReady,
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Play", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = {
                        installTarget = demo
                        installName = InputDemoManager.sanitizeInstallName(demo.file.nameWithoutExtension)
                    },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Add to Game", fontSize = 11.sp)
                }
                Spacer(modifier = Modifier.width(4.dp))
                OutlinedButton(
                    onClick = { deleteTarget = demo },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                    modifier = Modifier.height(28.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
                ) {
                    Text("Delete", fontSize = 11.sp)
                }
            }
        }
    }

    FileTransferProgress(transferProgress)
    Spacer(modifier = Modifier.height(8.dp))
    OutlinedButton(
        onClick = { showDeleteAllDialog = true },
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
        modifier = Modifier.height(32.dp),
        colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFFF44336)),
    ) {
        Text("Delete All Recorded Demos", fontSize = 12.sp)
    }

    if (deleteTarget != null) {
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text("Delete Recorded Demo") },
            text = { Text("Delete ${deleteTarget?.file?.name}? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    val target = deleteTarget
                    if (target != null) {
                        InputDemoManager.deleteStagedDemo(target)
                        refresh()
                    }
                    deleteTarget = null
                }) {
                    Text("Delete", color = Color(0xFFF44336))
                }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) { Text("Cancel") }
            },
        )
    }

    if (showDeleteAllDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteAllDialog = false },
            title = { Text("Delete All Recorded Demos") },
            text = { Text("Delete all staged recorded demos? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    val deleted = InputDemoManager.deleteAllStagedDemos(filesDir)
                    refresh()
                    showDeleteAllDialog = false
                    Toast.makeText(ctx, "Deleted $deleted recorded demo(s)", Toast.LENGTH_SHORT).show()
                }) {
                    Text("Delete", color = Color(0xFFF44336))
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteAllDialog = false }) { Text("Cancel") }
            },
        )
    }

    if (installTarget != null) {
        AlertDialog(
            onDismissRequest = { installTarget = null },
            title = { Text("Add Demo To Game") },
            text = {
                Column {
                    Text(
                        "Copy ${installTarget?.file?.name} and any recorded sidecars into the active set ($activeSetName) demos folder",
                        fontSize = 13.sp,
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = installName,
                        onValueChange = { installName = it },
                        singleLine = true,
                        label = { Text("Installed filename") },
                        modifier = Modifier.dpadTextFieldNavigation(),
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    val target = installTarget ?: return@TextButton
                    scope.launch {
                        try {
                            val dest =
                                withContext(Dispatchers.IO) {
                                    val activeSetDir = fileSetManager.getSetDir(activeSetName)
                                    val demosDir = File(activeSetDir, "demos").also { it.mkdirs() }

                                    ImportStorageGuard.requireFreeSpace(
                                        demosDir,
                                        InputDemoManager.stagedFileBytes(target),
                                        "install ${target.file.name}",
                                    )
                                    InputDemoManager.installToSet(target, activeSetDir, installName) { progress ->
                                        mainHandler.post { transferProgress = progress }
                                    }
                                }
                            transferProgress = null
                            installTarget = null
                            refresh()
                            Toast.makeText(ctx, "Added to ${dest.name}", Toast.LENGTH_SHORT).show()
                        } catch (e: Exception) {
                            transferProgress = null
                            Toast.makeText(ctx, "Add failed: ${e.message}", Toast.LENGTH_SHORT).show()
                        }
                    }
                }) {
                    Text("Add")
                }
            },
            dismissButton = {
                TextButton(onClick = { installTarget = null }) { Text("Cancel") }
            },
        )
    }
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

private fun isUnderDirectory(
    child: File,
    parent: File,
): Boolean =
    try {
        child.canonicalPath == parent.canonicalPath ||
            child.canonicalPath.startsWith(parent.canonicalPath + File.separator)
    } catch (_: Exception) {
        child.absolutePath == parent.absolutePath ||
            child.absolutePath.startsWith(parent.absolutePath + File.separator)
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
        var refreshFiles by remember { mutableIntStateOf(0) }
        var selectedEntry by remember { mutableStateOf<StorageFileEntry?>(null) }
        var deleteEntry by remember { mutableStateOf<StorageFileEntry?>(null) }
        var sortBySize by remember { mutableStateOf(false) }
        var displayLimit by remember(refreshFiles) { mutableIntStateOf(STORAGE_FILE_PAGE_SIZE) }
        val scanResult by produceState<StorageFileScanResult?>(initialValue = null, refreshFiles) {
            value = withContext(Dispatchers.IO) { scanStorageFiles(filesDir) }
        }
        val allFiles = scanResult?.entries ?: emptyList()
        val fileEntries =
            remember(allFiles, sortBySize) {
                if (sortBySize) {
                    allFiles.sortedByDescending { it.size }
                } else {
                    allFiles.sortedWith(storageFileNameComparator())
                }
            }
        val visibleEntries =
            remember(fileEntries, displayLimit) { fileEntries.take(displayLimit.coerceAtMost(fileEntries.size)) }
        val hiddenCount = fileEntries.size - visibleEntries.size
        val totalSize = scanResult?.totalSize ?: 0L

        AlertDialog(
            onDismissRequest = { showFilesDialog = false },
            title = {
                Text(
                    if (scanResult == null) {
                        "App Storage Files"
                    } else {
                        "App Storage Files (${fileEntries.size})"
                    },
                )
            },
            text = {
                if (scanResult == null) {
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        CircularProgressIndicator()
                        Spacer(modifier = Modifier.height(12.dp))
                        Text("Scanning app storage…", fontSize = 12.sp)
                    }
                } else {
                    Column {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                "Total: ${formatSize(totalSize)}",
                                fontSize = 12.sp,
                                fontWeight = FontWeight.Medium,
                            )
                            Spacer(modifier = Modifier.weight(1f))
                            OutlinedButton(
                                onClick = { sortBySize = false },
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                modifier = Modifier.height(28.dp),
                                border =
                                    if (!sortBySize) {
                                        androidx.compose.foundation.BorderStroke(
                                            2.dp,
                                            MaterialTheme.colorScheme.primary,
                                        )
                                    } else {
                                        null
                                    },
                            ) { Text("Name", fontSize = 10.sp) }
                            OutlinedButton(
                                onClick = { sortBySize = true },
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                modifier = Modifier.height(28.dp),
                                border =
                                    if (sortBySize) {
                                        androidx.compose.foundation.BorderStroke(
                                            2.dp,
                                            MaterialTheme.colorScheme.primary,
                                        )
                                    } else {
                                        null
                                    },
                            ) { Text("Size", fontSize = 10.sp) }
                        }
                        if (hiddenCount > 0) {
                            Spacer(modifier = Modifier.height(8.dp))
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Text(
                                    "Showing ${visibleEntries.size} of ${fileEntries.size} files",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                                TextButton(
                                    onClick = {
                                        displayLimit =
                                            (displayLimit + STORAGE_FILE_PAGE_SIZE).coerceAtMost(fileEntries.size)
                                    },
                                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                ) {
                                    Text("Show More", fontSize = 10.sp)
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                        LazyColumn(modifier = Modifier.heightIn(max = 400.dp)) {
                            items(visibleEntries, key = { it.absolutePath }) { entry ->
                                Column(
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .clickable { selectedEntry = entry }
                                            .tvFocusable()
                                            .padding(vertical = 5.dp, horizontal = 4.dp),
                                ) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                    ) {
                                        Text(
                                            entry.file.name,
                                            fontSize = 12.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = Color(0xFF2E7D32),
                                            modifier = Modifier.weight(1f),
                                        )
                                        Text(formatSize(entry.size), fontSize = 10.sp)
                                    }
                                    entry.helperSymlinkTargetName?.let { targetName ->
                                        Text(
                                            "helper symlink for $targetName",
                                            fontSize = 10.sp,
                                            fontWeight = FontWeight.Medium,
                                            color = MaterialTheme.colorScheme.primary,
                                        )
                                    }
                                    Text(
                                        entry.purpose,
                                        fontSize = 10.sp,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                        Text(entry.location, fontSize = 10.sp, fontWeight = FontWeight.Bold)
                                        Text(entry.relativePath, fontSize = 10.sp)
                                    }
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showFilesDialog = false }) { Text("Close") }
            },
        )

        selectedEntry?.let { entry ->
            AlertDialog(
                onDismissRequest = { selectedEntry = null },
                title = { Text(entry.file.name) },
                text = {
                    Column {
                        Text("Location: ${entry.location}", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                        Spacer(modifier = Modifier.height(4.dp))
                        entry.helperSymlinkTargetName?.let { targetName ->
                            Text("Type: helper symlink", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text("Companion file: $targetName", fontSize = 12.sp)
                            Spacer(modifier = Modifier.height(4.dp))
                        }
                        Text("Purpose: ${entry.purpose}", fontSize = 12.sp)
                        Spacer(modifier = Modifier.height(4.dp))
                        Text("Size: ${formatSize(entry.size)}", fontSize = 12.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Full path:", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                        Text(entry.absolutePath, fontSize = 11.sp)
                    }
                },
                confirmButton = {
                    TextButton(onClick = { selectedEntry = null }) { Text("Close") }
                },
                dismissButton = {
                    TextButton(onClick = { deleteEntry = entry }) { Text("Delete") }
                },
            )
        }

        deleteEntry?.let { entry ->
            AlertDialog(
                onDismissRequest = { deleteEntry = null },
                title = { Text("Delete file?") },
                text = {
                    Column {
                        Text(entry.absolutePath, fontSize = 11.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("This cannot be undone.", fontSize = 12.sp, color = MaterialTheme.colorScheme.error)
                    }
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            if (entry.file.delete()) {
                                selectedEntry = null
                                deleteEntry = null
                                refreshFiles++
                                Toast.makeText(ctx, "Deleted ${entry.file.name}", Toast.LENGTH_SHORT).show()
                            } else {
                                deleteEntry = null
                                Toast.makeText(ctx, "Delete failed", Toast.LENGTH_LONG).show()
                            }
                        },
                    ) { Text("Delete") }
                },
                dismissButton = {
                    TextButton(onClick = { deleteEntry = null }) { Text("Cancel") }
                },
            )
        }
    }

    if (showSafDialog) {
        var refreshSafEntries by remember { mutableIntStateOf(0) }

        data class SafEntry(
            val label: String,
            val uri: String,
            val accessible: Boolean,
            val customSetId: String? = null,
            val customFilename: String? = null,
            val cdSourceId: String? = null,
            val isPermissionEntry: Boolean = false,
        ) {
            val key: String
                get() =
                    when {
                        customSetId != null -> "custom:$customSetId:$customFilename:$uri"
                        cdSourceId != null -> "cd:$cdSourceId:$uri"
                        isPermissionEntry -> "perm:$uri"
                        else -> uri
                    }
        }

        var selectedSafEntry by remember { mutableStateOf<SafEntry?>(null) }
        var removeSafEntry by remember { mutableStateOf<SafEntry?>(null) }

        val safEntries =
            remember(refreshSafEntries) {
                val entries = mutableListOf<SafEntry>()
                val trackedSafUris = mutableSetOf<String>()
                // Custom audio referenced URIs
                try {
                    val customMgr = CustomAudioSetManager(filesDir)
                    for (set in customMgr.getSets()) {
                        for ((filename, uriStr) in set.referencedUris) {
                            trackedSafUris.add(uriStr)
                            entries.add(
                                SafEntry(
                                    label = "Audio: $filename (${set.label})",
                                    uri = uriStr,
                                    accessible = canAccessSafUri(ctx, Uri.parse(uriStr)),
                                    customSetId = set.id,
                                    customFilename = filename,
                                ),
                            )
                        }
                    }
                } catch (_: Exception) {
                }
                // CD audio SAF sources
                try {
                    val srcMgr = AudioSourceManager(filesDir)
                    for (src in srcMgr.getSources()) {
                        if (!hasSafLinkedCdContent(src)) continue
                        val safBinUris = src.binContentUriList().filterNot(::isLocalCdContentPath)
                        val safCueUri = src.cueContentUri?.takeUnless(::isLocalCdContentPath)
                        (safBinUris + listOfNotNull(safCueUri)).forEach(trackedSafUris::add)
                        val displayUri = safCueUri ?: safBinUris.firstOrNull() ?: continue
                        val accessible =
                            (
                                safBinUris.map { uriStr ->
                                    canAccessSafUri(ctx, Uri.parse(uriStr), useFileDescriptor = true)
                                } +
                                    listOfNotNull(
                                        safCueUri?.let { uriStr ->
                                            canAccessSafUri(ctx, Uri.parse(uriStr))
                                        },
                                    )
                            ).all { it }
                        entries.add(
                            SafEntry(
                                label = buildCdSourceSafLabel(src),
                                uri = displayUri,
                                accessible = accessible,
                                cdSourceId = src.id,
                            ),
                        )
                    }
                } catch (_: Exception) {
                }
                // Persistable URI permissions
                val persisted = ctx.contentResolver.persistedUriPermissions
                for (perm in persisted) {
                    val uriStr = perm.uri.toString()
                    if (!isPersistedPermissionCoveredByTrackedUris(uriStr, trackedSafUris)) {
                        entries.add(
                            SafEntry(
                                label = "Permission",
                                uri = uriStr,
                                accessible = perm.isReadPermission,
                                isPermissionEntry = true,
                            ),
                        )
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
                        items(items = safEntries, key = { it.key }) { entry ->
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .clickable { selectedSafEntry = entry }
                                        .tvFocusable()
                                        .padding(vertical = 4.dp),
                            ) {
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

        selectedSafEntry?.let { entry ->
            val removeSummary =
                when {
                    entry.customSetId != null -> {
                        "This removes the referenced audio file from the launcher set only. External storage files are not deleted"
                    }

                    entry.cdSourceId != null -> {
                        "This removes the CD audio source from the launcher. External storage files are not deleted"
                    }

                    entry.isPermissionEntry -> {
                        "This revokes the persisted SAF permission. Other entries that still rely on this grant may stop working"
                    }

                    else -> {
                        "This entry can be removed from the launcher"
                    }
                }

            AlertDialog(
                onDismissRequest = { selectedSafEntry = null },
                title = { Text(entry.label) },
                text = {
                    Column {
                        Text(
                            if (entry.accessible) "Status: OK" else "Status: BROKEN",
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Medium,
                            color = if (entry.accessible) MaterialTheme.colorScheme.primary else Color(0xFFF44336),
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("URI:", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                        Text(entry.uri, fontSize = 11.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(removeSummary, fontSize = 12.sp)
                    }
                },
                confirmButton = {
                    TextButton(onClick = { selectedSafEntry = null }) { Text("Close") }
                },
                dismissButton = {
                    TextButton(onClick = { removeSafEntry = entry }) { Text("Remove") }
                },
            )
        }

        removeSafEntry?.let { entry ->
            val removeDetail =
                when {
                    entry.customSetId != null -> {
                        "Remove this referenced audio file from its launcher set? This will not delete the external file"
                    }

                    entry.cdSourceId != null -> {
                        "Remove this CD audio source from the launcher? This will not delete the external file"
                    }

                    entry.isPermissionEntry -> {
                        "Revoke this persisted SAF permission? Any remaining launcher entries that still need it may stop working"
                    }

                    else -> {
                        "Remove this SAF entry from the launcher?"
                    }
                }

            AlertDialog(
                onDismissRequest = { removeSafEntry = null },
                title = { Text("Remove SAF link?") },
                text = {
                    Column {
                        Text(entry.uri, fontSize = 11.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(removeDetail, fontSize = 12.sp)
                    }
                },
                confirmButton = {
                    TextButton(
                        onClick = {
                            var removed = false
                            when {
                                entry.customSetId != null && entry.customFilename != null -> {
                                    CustomAudioSetManager(
                                        filesDir,
                                    ).removeReferencedFile(entry.customSetId, entry.customFilename)
                                    Toast.makeText(ctx, "Removed SAF audio link", Toast.LENGTH_SHORT).show()
                                    removed = true
                                }

                                entry.cdSourceId != null -> {
                                    val srcMgr = AudioSourceManager(filesDir)
                                    val source = srcMgr.getSources().firstOrNull { it.id == entry.cdSourceId }
                                    if (source != null) {
                                        srcMgr.removeSource(source.id, ctx)
                                        Toast.makeText(ctx, "Removed CD audio source", Toast.LENGTH_SHORT).show()
                                        removed = true
                                    }
                                }

                                entry.isPermissionEntry -> {
                                    releaseReadPermissionForUri(ctx, Uri.parse(entry.uri))
                                    Toast.makeText(ctx, "Removed SAF permission", Toast.LENGTH_SHORT).show()
                                    removed = true
                                }
                            }
                            removeSafEntry = null
                            selectedSafEntry = null
                            if (removed) {
                                refreshSafEntries++
                            }
                        },
                    ) { Text("Remove") }
                },
                dismissButton = {
                    TextButton(onClick = { removeSafEntry = null }) { Text("Cancel") }
                },
            )
        }
    }
}

@Composable
private fun ImportLocationSection(filesDir: File) {
    val ctx = LocalContext.current
    val mgr = remember { ImportLocationManager(filesDir) }
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var activePath by remember { mutableStateOf(mgr.getActiveRoot().absolutePath) }
    var overrideActive by remember { mutableStateOf(mgr.isOverrideActive()) }
    var showPicker by remember { mutableStateOf(false) }
    var pendingTarget by remember { mutableStateOf<File?>(null) }
    var migrating by remember { mutableStateOf(false) }
    var migrateCopied by remember { mutableStateOf(0L) }
    var migrateTotal by remember { mutableStateOf(0L) }

    Text("Imported Files Location", fontWeight = FontWeight.Bold, fontSize = 14.sp)
    Spacer(modifier = Modifier.height(4.dp))
    Text(
        "Where extracted CD images, GOG installer files, and mods are stored. " +
            "Move this to an SD card or USB drive on devices with limited internal storage. " +
            "Saves, pilots, and configs always stay in app storage.",
        fontSize = 12.sp,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Spacer(modifier = Modifier.height(8.dp))
    Text(
        "Current: $activePath" + if (overrideActive) "  (override)" else "  (default)",
        fontSize = 11.sp,
    )
    Spacer(modifier = Modifier.height(8.dp))
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedButton(
            onClick = { showPicker = true },
            contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
            modifier = Modifier.height(32.dp),
            enabled = !migrating,
        ) {
            Text("Set new location...", fontSize = 12.sp)
        }
        if (overrideActive) {
            OutlinedButton(
                onClick = { pendingTarget = mgr.getDefaultRoot() },
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                modifier = Modifier.height(32.dp),
                enabled = !migrating,
            ) {
                Text("Revert to default", fontSize = 12.sp)
            }
        }
    }

    if (showPicker) {
        val volumes = remember { mgr.listCandidateVolumes(ctx) }
        AlertDialog(
            onDismissRequest = { showPicker = false },
            title = { Text("Choose imported files location") },
            text = {
                Column {
                    if (volumes.size <= 1) {
                        Text(
                            "No additional storage volumes detected on this device. " +
                                "Plug in an SD card or USB drive and try again.",
                            fontSize = 12.sp,
                        )
                    }
                    LazyColumn(modifier = Modifier.heightIn(max = 360.dp)) {
                        items(volumes) { vol ->
                            val srcUsage =
                                try {
                                    mgr
                                        .getActiveRoot()
                                        .walkTopDown()
                                        .filter { it.isFile }
                                        .sumOf { it.length() }
                                } catch (_: Exception) {
                                    0L
                                }
                            val tooSmall = !vol.isCurrent && vol.freeBytes < srcUsage + 64L * 1024L * 1024L
                            val clickable = !vol.isCurrent && !tooSmall
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .clickable(enabled = clickable) {
                                            pendingTarget = vol.path
                                            showPicker = false
                                        }.padding(vertical = 6.dp, horizontal = 4.dp),
                            ) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(
                                        vol.label,
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.Medium,
                                    )
                                    if (vol.isCurrent) {
                                        Spacer(modifier = Modifier.width(8.dp))
                                        Text(
                                            "(current)",
                                            fontSize = 10.sp,
                                            color = MaterialTheme.colorScheme.primary,
                                        )
                                    }
                                    if (tooSmall) {
                                        Spacer(modifier = Modifier.width(8.dp))
                                        Text(
                                            "(not enough free space)",
                                            fontSize = 10.sp,
                                            color = Color(0xFFF44336),
                                        )
                                    }
                                }
                                Text(
                                    vol.path.absolutePath,
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                                Text(
                                    "${formatSize(vol.freeBytes)} free of ${formatSize(vol.totalBytes)}",
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showPicker = false }) { Text("Cancel") }
            },
        )
    }

    pendingTarget?.let { target ->
        val src = remember(target) { mgr.getActiveRoot() }
        val srcSize =
            remember(target) {
                try {
                    src.walkTopDown().filter { it.isFile }.sumOf { it.length() }
                } catch (_: Exception) {
                    0L
                }
            }
        AlertDialog(
            onDismissRequest = { pendingTarget = null },
            title = { Text("Move imported files?") },
            text = {
                Column {
                    Text("From:", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                    Text(src.absolutePath, fontSize = 11.sp)
                    Spacer(modifier = Modifier.height(6.dp))
                    Text("To:", fontSize = 12.sp, fontWeight = FontWeight.Medium)
                    Text(target.absolutePath, fontSize = 11.sp)
                    Spacer(modifier = Modifier.height(6.dp))
                    Text(
                        "About ${formatSize(srcSize)} will be copied. The app will restart " +
                            "after the move completes.",
                        fontSize = 11.sp,
                    )
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        val dst = target
                        pendingTarget = null
                        migrating = true
                        migrateCopied = 0
                        migrateTotal = srcSize
                        Thread {
                            val result =
                                mgr.migrate(src, dst) { copied, total ->
                                    mainHandler.post {
                                        migrateCopied = copied
                                        migrateTotal = total
                                    }
                                }
                            android.os.Handler(android.os.Looper.getMainLooper()).post {
                                migrating = false
                                when (result) {
                                    is ImportLocationManager.MigrateResult.Success -> {
                                        if (dst.absolutePath == mgr.getDefaultRoot().absolutePath) {
                                            mgr.clearOverride()
                                        } else {
                                            mgr.setOverride(dst)
                                        }
                                        Toast
                                            .makeText(
                                                ctx,
                                                "Move complete; restarting",
                                                Toast.LENGTH_SHORT,
                                            ).show()
                                        android.os.Process.killProcess(android.os.Process.myPid())
                                    }

                                    is ImportLocationManager.MigrateResult.Failure -> {
                                        Toast
                                            .makeText(
                                                ctx,
                                                "Move failed: ${result.reason}",
                                                Toast.LENGTH_LONG,
                                            ).show()
                                        activePath = mgr.getActiveRoot().absolutePath
                                        overrideActive = mgr.isOverrideActive()
                                    }
                                }
                            }
                        }.start()
                    },
                ) { Text("Move") }
            },
            dismissButton = {
                TextButton(onClick = { pendingTarget = null }) { Text("Cancel") }
            },
        )
    }

    if (migrating) {
        AlertDialog(
            onDismissRequest = {},
            title = { Text("Moving imported files...") },
            text = {
                Column {
                    val pct =
                        if (migrateTotal > 0) {
                            (migrateCopied * 100 / migrateTotal).toInt().coerceIn(0, 100)
                        } else {
                            0
                        }
                    Text("$pct%  (${formatSize(migrateCopied)} / ${formatSize(migrateTotal)})", fontSize = 12.sp)
                    Spacer(modifier = Modifier.height(8.dp))
                    LinearProgressIndicator(
                        progress = { pct / 100f },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            },
            confirmButton = {},
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

@Composable
private fun FileTransferProgress(progress: LauncherCopyProgress?) {
    if (progress == null) return
    Spacer(modifier = Modifier.height(6.dp))
    Text(progress.label, fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
    Spacer(modifier = Modifier.height(3.dp))
    if (progress.bytesTotal > 0L) {
        val pct = (progress.bytesDone.toFloat() / progress.bytesTotal.toFloat()).coerceIn(0f, 1f)
        LinearProgressIndicator(progress = { pct }, modifier = Modifier.fillMaxWidth().height(4.dp))
    } else {
        LinearProgressIndicator(modifier = Modifier.fillMaxWidth().height(4.dp))
    }
}

private fun copyFileToCache(
    context: android.content.Context,
    file: File,
    dirName: String,
    onProgress: (LauncherCopyProgress) -> Unit = {},
): Uri {
    val viewDir = File(context.cacheDir, dirName)
    viewDir.mkdirs()
    val copy = File(viewDir, file.name)
    LauncherFileCopy.copyFileToFile(file, copy, file.name, onProgress)
    return androidx.core.content.FileProvider
        .getUriForFile(context, FILE_PROVIDER_AUTHORITY, copy)
}

private fun openTextFile(
    context: android.content.Context,
    uri: Uri,
) {
    try {
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

private fun shareTextFile(
    context: android.content.Context,
    uri: Uri,
    chooserTitle: String,
) = shareFile(context, uri, chooserTitle, "text/plain")

private fun shareFile(
    context: android.content.Context,
    uri: Uri,
    chooserTitle: String,
    mimeType: String,
) {
    try {
        val intent =
            Intent(Intent.ACTION_SEND).apply {
                type = mimeType
                putExtra(Intent.EXTRA_STREAM, uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
        val chooser = Intent.createChooser(intent, chooserTitle)
        chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(chooser)
    } catch (e: Exception) {
        Toast.makeText(context, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}

private fun shareFiles(
    context: android.content.Context,
    uris: List<Uri>,
    chooserTitle: String,
    mimeType: String,
) {
    try {
        val intent =
            Intent(Intent.ACTION_SEND_MULTIPLE).apply {
                type = mimeType
                putParcelableArrayListExtra(Intent.EXTRA_STREAM, ArrayList(uris))
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
        val chooser = Intent.createChooser(intent, chooserTitle)
        chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(chooser)
    } catch (e: Exception) {
        Toast.makeText(context, "Share failed: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}

private fun exportMimeType(file: File): String =
    if (file.name.endsWith(".jsonl", ignoreCase = true)) "application/json" else "application/octet-stream"

private fun formatDurationMillis(durationMillis: Long?): String {
    if (durationMillis == null) return "--:--"
    val totalSeconds = (durationMillis / 1000L).coerceAtLeast(0L)
    val minutes = totalSeconds / 60L
    val seconds = totalSeconds % 60L
    return String.format(Locale.US, "%02d:%02d", minutes, seconds)
}

private fun saveToDownloads(
    context: android.content.Context,
    file: File,
    mimeType: String = "text/plain",
    onProgress: (LauncherCopyProgress) -> Unit = {},
): Boolean =
    try {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val values =
                ContentValues().apply {
                    put(MediaStore.Downloads.DISPLAY_NAME, file.name)
                    put(MediaStore.Downloads.MIME_TYPE, mimeType)
                }
            val uri =
                context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                    ?: throw Exception("MediaStore insert failed")
            LauncherFileCopy.copyFileToUri(context, file, uri, file.name, onProgress)
        } else {
            @Suppress("DEPRECATION")
            val dlDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            dlDir.mkdirs()
            LauncherFileCopy.copyFileToFile(file, File(dlDir, file.name), file.name, onProgress)
        }
        true
    } catch (e: Exception) {
        false
    }
