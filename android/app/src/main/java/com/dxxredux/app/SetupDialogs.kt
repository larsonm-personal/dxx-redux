package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.InputMode
import androidx.compose.ui.platform.LocalInputModeManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale

/** Format byte size as human-readable (KB, MB, GB). */
private fun formatSize(bytes: Long): String =
    when {
        bytes >= 1_073_741_824 -> "%.2f GB".format(bytes / 1_073_741_824.0)
        bytes >= 1_048_576 -> "%.1f MB".format(bytes / 1_048_576.0)
        bytes >= 1_024 -> "%.0f KB".format(bytes / 1_024.0)
        else -> "$bytes B"
    }

@Composable
internal fun SetManagementDialog(
    fileSetManager: FileSetManager,
    activeSetName: String,
    onSwitchSet: (String) -> Unit,
    onContentChanged: suspend () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var newSetName by remember { mutableStateOf("") }
    var showNewSetInput by remember { mutableStateOf(false) }
    var confirmDelete by remember { mutableStateOf(false) }
    var errorMessage by remember { mutableStateOf<String?>(null) }
    var sets by remember { mutableStateOf(emptyList<FileSetManager.FileSetInfo>()) }
    var setUsages by remember { mutableStateOf<Map<String, Long>?>(null) }
    var mutating by remember { mutableStateOf(false) }
    var missions by remember { mutableStateOf<List<FileSetMissionEntry>?>(null) }
    var confirmMissionRemoval by remember { mutableStateOf<FileSetMissionEntry?>(null) }

    suspend fun refreshSets() {
        val result =
            withContext(Dispatchers.IO) {
                val loadedSets = fileSetManager.listSets()
                Triple(
                    loadedSets,
                    loadedSets.associate { it.name to fileSetManager.diskUsage(it.name) },
                    FileSetMissionInventory.scan(fileSetManager.getSetDir(activeSetName)),
                )
            }
        sets = result.first
        setUsages = result.second
        missions = result.third
    }

    LaunchedEffect(fileSetManager, activeSetName) {
        setUsages = null
        refreshSets()
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = { Text("File Sets", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.heightIn(max = 560.dp).verticalScroll(rememberScrollState())) {
                // Current set info
                Text(
                    "Current: $activeSetName",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                Text(
                    "Size: ${setUsages?.get(activeSetName)?.let(::formatSize) ?: "Checking..."}",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                Spacer(modifier = Modifier.height(10.dp))
                Text("Included missions and expansions", fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
                when (val listedMissions = missions) {
                    null -> {
                        Text("Checking...", fontSize = 12.sp)
                    }

                    else -> {
                        if (listedMissions.isEmpty()) {
                            Text(
                                "No optional loose missions detected",
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        } else {
                            listedMissions.forEach { mission ->
                                Column(modifier = Modifier.fillMaxWidth().padding(top = 8.dp)) {
                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                        Column(modifier = Modifier.weight(1f)) {
                                            Text(mission.displayName, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                                            Text(
                                                buildString {
                                                    append(if (mission.game == "d1") "Descent" else "Descent 2")
                                                    mission.versionName?.let { append(" | $it") }
                                                    append(" | set: $activeSetName")
                                                },
                                                fontSize = 11.sp,
                                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                            )
                                            Text(
                                                "${mission.files.joinToString { it.name }} (${formatSize(
                                                    mission.totalBytes,
                                                )})",
                                                fontSize = 10.sp,
                                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                            )
                                            mission.sourceUri?.let { source ->
                                                Text(
                                                    "Imported source: ${Uri.parse(source).lastPathSegment ?: source}",
                                                    fontSize = 10.sp,
                                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                                )
                                            }
                                        }
                                        TextButton(
                                            enabled = !mutating,
                                            onClick = { confirmMissionRemoval = mission },
                                        ) { Text("Remove", fontSize = 11.sp) }
                                    }
                                }
                            }
                        }
                    }
                }
                errorMessage?.let {
                    Text(it, fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
                }

                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                // Other sets to switch to
                val otherSets = sets.filter { it.name != activeSetName }
                if (otherSets.isNotEmpty()) {
                    otherSets.forEach { set ->
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSwitchSet(set.name) }
                                    .padding(vertical = 8.dp, horizontal = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                text = "Switch to \"${set.name}\"",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.primary,
                                modifier = Modifier.weight(1f),
                            )
                            Text(
                                text = setUsages?.get(set.name)?.let(::formatSize) ?: "Checking...",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                }

                // Add new set
                if (showNewSetInput) {
                    OutlinedTextField(
                        value = newSetName,
                        onValueChange = {
                            newSetName = it
                            errorMessage = null
                        },
                        label = { Text("Set name") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth().dpadTextFieldNavigation(),
                        isError = errorMessage != null,
                        supportingText = errorMessage?.let { { Text(it) } },
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(
                            enabled = !mutating,
                            onClick = {
                                val name = newSetName.trim()
                                mutating = true
                                scope.launch {
                                    val error =
                                        withContext(Dispatchers.IO) {
                                            runCatching { fileSetManager.createSet(name) }.exceptionOrNull()
                                        }
                                    mutating = false
                                    if (error == null) {
                                        onSwitchSet(name)
                                    } else {
                                        errorMessage = error.message
                                    }
                                }
                            },
                        ) {
                            Text("Create", fontSize = 13.sp)
                        }
                        OutlinedButton(onClick = {
                            showNewSetInput = false
                            newSetName = ""
                        }) {
                            Text("Cancel", fontSize = 13.sp)
                        }
                    }
                } else {
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { showNewSetInput = true }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                    ) {
                        Text(
                            text = "+ Add new set\u2026",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                }

                // Delete / clear current set
                HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                if (!confirmDelete) {
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { confirmDelete = true }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                    ) {
                        Text(
                            text =
                                if (activeSetName == FileSetManager.DEFAULT_SET) {
                                    "Clear all files in \"$activeSetName\""
                                } else {
                                    "Delete \"$activeSetName\""
                                },
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.error,
                        )
                    }
                } else {
                    Column(modifier = Modifier.padding(horizontal = 4.dp)) {
                        Text(
                            text =
                                "Imported files (copied to app data) will be permanently deleted.\n\n" +
                                    "Files added via file picker (leave-in-place) will be unlinked " +
                                    "but not deleted from their original location.",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable(enabled = !mutating) {
                                        mutating = true
                                        scope.launch {
                                            withContext(Dispatchers.IO) {
                                                if (activeSetName == FileSetManager.DEFAULT_SET) {
                                                    fileSetManager.clearSet(activeSetName)
                                                } else {
                                                    fileSetManager.deleteSet(activeSetName)
                                                }
                                            }
                                            mutating = false
                                            onSwitchSet(FileSetManager.DEFAULT_SET)
                                        }
                                    }.padding(vertical = 8.dp),
                        ) {
                            Text(
                                text =
                                    if (activeSetName == FileSetManager.DEFAULT_SET) {
                                        "Confirm clear \"$activeSetName\"?"
                                    } else {
                                        "Confirm delete \"$activeSetName\"?"
                                    },
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.error,
                                fontWeight = FontWeight.Bold,
                            )
                        }
                    }
                }
            }
        },
    )

    confirmMissionRemoval?.let { mission ->
        AlertDialog(
            onDismissRequest = { confirmMissionRemoval = null },
            title = { Text("Remove ${mission.displayName}?") },
            text = {
                Text(
                    "Only ${mission.files.joinToString { it.name }} will be removed from set $activeSetName. " +
                        "Base game files will be kept.",
                )
            },
            confirmButton = {
                TextButton(
                    enabled = !mutating,
                    onClick = {
                        mutating = true
                        scope.launch {
                            val failure =
                                runCatching {
                                    withContext(Dispatchers.IO) {
                                        FileSetMissionInventory.remove(fileSetManager.getSetDir(activeSetName), mission)
                                    }
                                    onContentChanged()
                                }.exceptionOrNull()
                            if (failure == null) {
                                confirmMissionRemoval = null
                                refreshSets()
                            } else {
                                errorMessage = "Could not remove mission: ${failure.message ?: "unknown error"}"
                            }
                            mutating = false
                        }
                    },
                ) { Text("Remove") }
            },
            dismissButton = {
                TextButton(onClick = { confirmMissionRemoval = null }) { Text("Cancel") }
            },
        )
    }
}

// -- Download helper ---------------------------------------------------------

internal suspend fun setupDownloadFile(
    url: String,
    destDir: File,
    filename: String,
    onProgress: (Int) -> Unit,
    onDone: (Boolean) -> Unit,
) {
    withContext(Dispatchers.IO) {
        var conn: HttpURLConnection? = null
        try {
            val connection = URL(url).openConnection() as HttpURLConnection
            conn = connection
            connection.connectTimeout = 15_000
            connection.readTimeout = 30_000
            connection.connect()

            if (connection.responseCode != 200) {
                Log.e("DXX-Setup", "Download failed: HTTP ${connection.responseCode} for $url")
                withContext(Dispatchers.Main) { onDone(false) }
                return@withContext
            }

            val totalBytes = connection.contentLengthLong.coerceAtLeast(0L)
            val destFile = File(destDir, filename)
            val mainHandler = android.os.Handler(android.os.Looper.getMainLooper())
            val downloaded =
                LauncherFileCopy.copyInputToFile(
                    dest = destFile,
                    expectedBytes = totalBytes,
                    label = "download $filename",
                    onProgress = { progress ->
                        if (progress.bytesTotal > 0L) {
                            val pct =
                                (progress.bytesDone * 100 / progress.bytesTotal)
                                    .toInt()
                                    .coerceIn(0, 100)
                            mainHandler.post { onProgress(pct) }
                        }
                    },
                ) { connection.inputStream }
            Log.i("DXX-Setup", "Downloaded $filename ($downloaded bytes)")
            withContext(Dispatchers.Main) { onDone(true) }
        } catch (e: InsufficientStorageException) {
            Log.e("DXX-Setup", "Not enough space to download $filename", e)
            ImportStorageGuard.recordFailure(destDir.parentFile ?: destDir, "Download failed for $filename", e)
            withContext(Dispatchers.Main) { onDone(false) }
        } catch (e: Exception) {
            Log.e("DXX-Setup", "Download error for $filename", e)
            ImportStorageGuard.recordFailure(destDir.parentFile ?: destDir, "Download failed for $filename", e)
            withContext(Dispatchers.Main) { onDone(false) }
        } finally {
            conn?.disconnect()
        }
    }
}

// -- GOG installer import dialog -------------------------------------------

internal fun publishStagedArchiveFiles(
    stagingDir: File,
    setDir: File,
) {
    check(setDir.isDirectory || setDir.mkdirs()) { "Could not create ${setDir.absolutePath}" }
    val backupDir = File(setDir.parentFile, ".archive-backup-${System.nanoTime()}")
    val published = mutableListOf<File>()
    val backups = mutableListOf<Pair<File, File>>()
    var preserveBackup = false
    try {
        stagingDir
            .walkTopDown()
            .filter { it.isFile }
            .forEach { source ->
                val destination = File(setDir, source.relativeTo(stagingDir).path)
                destination.parentFile?.let { parent ->
                    check(parent.isDirectory || parent.mkdirs()) { "Could not create ${parent.absolutePath}" }
                }
                if (destination.exists()) {
                    val backup = File(backupDir, destination.relativeTo(setDir).path)
                    val backupParent = checkNotNull(backup.parentFile)
                    check((backupParent.isDirectory || backupParent.mkdirs()) && destination.renameTo(backup)) {
                        "Could not stage existing ${destination.absolutePath}"
                    }
                    backups += backup to destination
                }
                LauncherFileCopy.copyFileToFile(source, destination)
                published += destination
            }
    } catch (error: Exception) {
        published.forEach { it.delete() }
        val restored =
            backups.asReversed().all { (backup, destination) ->
                destination.parentFile?.mkdirs()
                backup.renameTo(destination)
            }
        if (!restored) {
            preserveBackup = true
            throw IllegalStateException("Archive publication rollback remains in ${backupDir.absolutePath}", error)
        }
        throw error
    } finally {
        if (!preserveBackup) backupDir.deleteRecursively()
    }
}

/**
 * Dialog for importing a GOG installer (.exe InnoSetup or .pkg Mac).
 *
 * Flow:
 *  1. Copies installer to temp via content resolver
 *  2. Detects format (InnoSetup / .pkg)
 *  3. Lists game files inside the installer
 *  4. Extracts game files to setDir with progress
 *  5. Detects .gog/.inst audio pair after extraction
 */
@Composable
internal fun GogImportDialog(
    installerName: String,
    installerUri: Uri,
    filesDir: File,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var status by remember { mutableStateOf("Analyzing installer\u2026") }
    var format by remember { mutableStateOf<String?>(null) }
    var fileList by remember { mutableStateOf<List<GogImportBridge.GogFile>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var extractedCount by remember { mutableIntStateOf(0) }
    var extractedFileNames by remember { mutableStateOf<List<String>>(emptyList()) }
    var progressFile by remember { mutableStateOf("") }
    var progressPct by remember { mutableStateOf(0f) }
    var copyingInstaller by remember { mutableStateOf(false) }
    var copyProgressPct by remember { mutableStateOf(0f) }
    var tempPath by remember { mutableStateOf<String?>(null) }
    var errorMsg by remember { mutableStateOf<String?>(null) }
    var includeAudio by remember { mutableStateOf(true) }
    var storageFailureMessage by remember { mutableStateOf<String?>(null) }
    val extractFocus = remember { FocusRequester() }
    val doneFocus = remember { FocusRequester() }

    LaunchedEffect(fileList, processing, extractedCount) {
        when {
            extractedCount > 0 -> doneFocus.requestFocus()
            fileList != null && fileList!!.isNotEmpty() && !processing -> extractFocus.requestFocus()
        }
    }

    // Analyze installer; .exe imports read directly from the SAF fd when the
    // provider gives us a seekable fd, avoiding a large tmp copy
    LaunchedEffect(installerUri) {
        withContext(Dispatchers.IO) {
            try {
                val lowerName = installerName.lowercase(Locale.US)
                val directExe = lowerName.endsWith(".exe")

                suspend fun stageInstaller(copyStatus: String): Triple<String, String, List<GogImportBridge.GogFile>?> {
                    val tmpDir = File(filesDir, "tmp")
                    tmpDir.mkdirs()
                    val tmpFile = File(tmpDir, requireSafeProviderDisplayName(installerName))
                    withContext(Dispatchers.Main) {
                        status = copyStatus
                        copyingInstaller = true
                        copyProgressPct = 0f
                    }
                    copyUriToFileWithProgress(context, installerUri, tmpFile) { copied, total ->
                        withContext(Dispatchers.Main) {
                            copyProgressPct = if (total > 0) copied.toFloat() / total.toFloat() else 0f
                        }
                    }
                    withContext(Dispatchers.Main) { copyingInstaller = false }
                    return Triple(
                        tmpFile.absolutePath,
                        GogImportBridge.detectFormat(tmpFile.absolutePath),
                        GogImportBridge.listFiles(tmpFile.absolutePath),
                    )
                }

                var tmpPath: String? = null
                var fmt: String
                var files: List<GogImportBridge.GogFile>?
                if (directExe) {
                    fmt = "innosetup"
                    files =
                        context.contentResolver.openFileDescriptor(installerUri, "r")?.use { pfd ->
                            GogImportBridge.listFilesFromFd(pfd.fd)
                        }
                    if (files.isNullOrEmpty()) {
                        Log.i("DXX-GogImport", "Direct fd listing failed; staging installer from selected provider")
                        val staged = stageInstaller("Copying installer from selected provider...")
                        tmpPath = staged.first
                        fmt = staged.second
                        files = staged.third
                    }
                } else {
                    val staged = stageInstaller("Copying installer...")
                    tmpPath = staged.first
                    fmt = staged.second
                    files = staged.third
                }
                tempPath = tmpPath
                val analyzedFiles = files ?: emptyList()
                val audioCandidates =
                    analyzedFiles.filter {
                        GogImportBridge.isAudioFile(it.name)
                    }
                val analyzedAudioNames = audioCandidates.map { it.name }
                val analyzedAudioSummary = summarizeGogAudioFiles(analyzedAudioNames)
                val analyzedAudioSizes = summarizeGogAudioEntrySizes(audioCandidates)
                val analyzedPairState = describeGogPairState(analyzedAudioNames)
                LauncherDebugLog.log(
                    "launcher-gog-analysis installer=$installerName direct_exe=$directExe " +
                        "format=$fmt " +
                        "staged_copy=${tmpPath != null} total_entries=${analyzedFiles.size} " +
                        "audio_entries=${audioCandidates.size}",
                )
                LauncherDebugLog.log(
                    "launcher-gog-analysis-audio installer=$installerName " +
                        "audio_names=$analyzedAudioSummary audio_sizes=$analyzedAudioSizes " +
                        "audio_pair_state=$analyzedPairState",
                )
                withContext(Dispatchers.Main) {
                    format = fmt
                    fileList = files
                    if (fmt == "unknown") {
                        status = "Not a recognized GOG installer"
                        errorMsg = "This file doesn't appear to be a GOG InnoSetup (.exe) or Mac .pkg installer."
                    } else if (files == null || files.isEmpty()) {
                        status = "No game files found in installer"
                        errorMsg = "The installer was recognized as $fmt but contains no game files."
                    } else {
                        val gameFiles = files.filterNot { GogImportBridge.isAudioFile(it.name) }
                        val totalSize = gameFiles.sumOf { it.size }
                        status = "Found ${gameFiles.size} game file(s) (${formatSize(totalSize)})"
                    }
                }
            } catch (e: InsufficientStorageException) {
                Log.e("DXX-GogImport", "Analysis stopped for storage", e)
                withContext(Dispatchers.Main) {
                    copyingInstaller = false
                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                    status = "Not enough free space"
                    errorMsg = e.message
                }
            } catch (e: Exception) {
                Log.e("DXX-GogImport", "Analysis failed", e)
                LauncherDebugLog.log(
                    "launcher-gog-analysis-error installer=$installerName message=${e.message ?: e.javaClass.simpleName}",
                )
                withContext(Dispatchers.Main) {
                    copyingInstaller = false
                    status = "Error: ${e.message}"
                    errorMsg = e.message
                }
            }
        }
    }

    storageFailureMessage?.let { message ->
        StorageFailureDialog(message = message, onDismiss = { storageFailureMessage = null })
    }

    AlertDialog(
        onDismissRequest = {
            if (!processing && !copyingInstaller) {
                tempPath?.let { File(it).delete() }
                cleanupTmpDir(filesDir)
                onDismiss()
            }
        },
        confirmButton = {
            if (!processing && !copyingInstaller) {
                TextButton(onClick = {
                    tempPath?.let { File(it).delete() }
                    cleanupTmpDir(filesDir)
                    onDismiss()
                }) { Text("Close") }
            }
        },
        title = { Text("Import GOG Installer", fontWeight = FontWeight.Bold) },
        text = {
            Column {
                // -- Scrollable area: file listing and status --
                Column(
                    modifier =
                        Modifier
                            .weight(1f, fill = false)
                            .verticalScroll(rememberScrollState()),
                ) {
                    Text(installerName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                    if (format != null && format != "unknown") {
                        Text(
                            "Format: ${if (format == "innosetup") "InnoSetup (.exe)" else "Mac .pkg"}",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(status, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

                    if (copyingInstaller) {
                        Spacer(modifier = Modifier.height(6.dp))
                        val pct = (copyProgressPct * 100f).toInt().coerceIn(0, 100)
                        Text("$pct%", fontSize = 11.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        LinearProgressIndicator(
                            progress = { copyProgressPct.coerceIn(0f, 1f) },
                            modifier = Modifier.fillMaxWidth(),
                        )
                    }

                    // Error message
                    if (errorMsg != null && extractedCount == 0) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(errorMsg!!, fontSize = 11.sp, color = MaterialTheme.colorScheme.error)
                    }

                    // File listing -- game files only
                    fileList?.let { files ->
                        val gameFiles = files.filterNot { GogImportBridge.isAudioFile(it.name) }
                        if (gameFiles.isNotEmpty()) {
                            Spacer(modifier = Modifier.height(8.dp))
                            gameFiles.forEach { f ->
                                Text(
                                    "${f.name} (${formatSize(f.size)})",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }

                    // Show extracted file names
                    if (extractedFileNames.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Extracted files:", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        val displayFiles = extractedFileNames.take(50)
                        displayFiles.forEach { name ->
                            Text(
                                "  $name",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        if (extractedFileNames.size > 50) {
                            Text(
                                "  ... and ${extractedFileNames.size - 50} more",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                // -- Fixed area: checkbox, buttons, progress --
                fileList?.let { files ->
                    val audioFiles = files.filter { GogImportBridge.isAudioFile(it.name) }
                    if (audioFiles.isNotEmpty() ||
                        extractedCount > 0 ||
                        (fileList != null && fileList!!.isNotEmpty() && !processing && extractedCount == 0) ||
                        processing
                    ) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                    }

                    // Audio files checkbox
                    if (audioFiles.isNotEmpty() && extractedCount == 0) {
                        val audioSize = audioFiles.sumOf { it.size }
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Checkbox(
                                checked = includeAudio,
                                onCheckedChange = { includeAudio = it },
                                enabled = !processing,
                            )
                            Text(
                                "Include CD audio (${formatSize(audioSize)})",
                                fontSize = 12.sp,
                                modifier =
                                    Modifier.clickable(enabled = !processing) {
                                        includeAudio = !includeAudio
                                    },
                            )
                        }
                        audioFiles.forEach { f ->
                            Text(
                                "  ${f.name} (${formatSize(f.size)})",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                // Extract button with explanatory text
                if (fileList != null && fileList!!.isNotEmpty() && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "Game files will be extracted to \"${setDir.name}\"" +
                            if (includeAudio) ". CD audio will be configured as the active music source" else "",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                val totalNeeded =
                                    fileList!!.sumOf { f ->
                                        if (!includeAudio && GogImportBridge.isAudioFile(f.name)) 0L else f.size
                                    }
                                try {
                                    ImportStorageGuard.requireFreeSpace(setDir, totalNeeded, "extract $installerName")
                                } catch (e: InsufficientStorageException) {
                                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                    status = "Not enough free space"
                                    return@launch
                                }
                                processing = true
                                status = "Extracting game files\u2026"
                                progressFile = ""
                                progressPct = 0f
                                val filesBefore =
                                    withContext(Dispatchers.IO) {
                                        setDir.list()?.toSet() ?: emptySet()
                                    }
                                withContext(Dispatchers.IO) {
                                    val stagingDir =
                                        File(
                                            setDir.parentFile ?: filesDir,
                                            ".gog-import-${System.nanoTime()}",
                                        )
                                    val sourceKind = if (tempPath != null) "staged" else "fd"
                                    val analyzedAudioEntries =
                                        fileList!!
                                            .asSequence()
                                            .filter { GogImportBridge.isAudioFile(it.name) }
                                            .toList()
                                    val analyzedAudioNames = analyzedAudioEntries.map { File(it.name).name }
                                    val analyzedAudioSummary = summarizeGogAudioFiles(analyzedAudioNames)
                                    val analyzedAudioSizes = summarizeGogAudioEntrySizes(analyzedAudioEntries)
                                    val analyzedPairState = describeGogPairState(analyzedAudioNames)
                                    val freeBeforeBytes = stagingDir.usableSpace
                                    val audioOutputBuckets = mutableMapOf<String, Long>()
                                    val audioProgressStarts = mutableSetOf<String>()
                                    try {
                                        check(stagingDir.mkdirs()) {
                                            "Could not create extraction staging directory"
                                        }
                                        val progress =
                                            object : GogImportBridge.ExtractProgress {
                                                override fun onProgress(
                                                    currentFile: String,
                                                    bytesDone: Long,
                                                    bytesTotal: Long,
                                                ): Int {
                                                    val currentName = discLeafName(currentFile)
                                                    if (includeAudio && GogImportBridge.isAudioFile(currentName)) {
                                                        val outputFile = File(stagingDir, currentName)
                                                        val outputBytes =
                                                            if (outputFile.exists()) {
                                                                outputFile.length()
                                                            } else {
                                                                -1L
                                                            }
                                                        val outputBucket =
                                                            if (outputBytes >= 0L) {
                                                                outputBytes / (64L * 1024L * 1024L)
                                                            } else {
                                                                -1L
                                                            }
                                                        val previousBucket = audioOutputBuckets[currentName]
                                                        val shouldLog =
                                                            audioProgressStarts.add(currentName) ||
                                                                previousBucket == null ||
                                                                outputBucket > previousBucket
                                                        if (shouldLog) {
                                                            audioOutputBuckets[currentName] = outputBucket
                                                            LauncherDebugLog.log(
                                                                "launcher-gog-audio-progress " +
                                                                    "installer=$installerName " +
                                                                    "source=$sourceKind file=$currentName " +
                                                                    "done=$bytesDone total=$bytesTotal " +
                                                                    "exists=${outputFile.exists()} " +
                                                                    "out_bytes=$outputBytes free_bytes=${stagingDir.usableSpace}",
                                                            )
                                                        }
                                                    }
                                                    val pct =
                                                        if (bytesTotal > 0) {
                                                            bytesDone.toFloat() / bytesTotal
                                                        } else {
                                                            0f
                                                        }
                                                    mainHandler.post {
                                                        progressFile = currentFile
                                                        progressPct = pct
                                                    }
                                                    return 0
                                                }
                                            }
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-start " +
                                                "installer=$installerName source=$sourceKind " +
                                                "include_audio=$includeAudio total_entries=${fileList!!.size}",
                                        )
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-start-path " +
                                                "installer=$installerName source=$sourceKind " +
                                                "set_dir=${stagingDir.absolutePath} free_before_bytes=$freeBeforeBytes",
                                        )
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-start-audio " +
                                                "installer=$installerName source=$sourceKind " +
                                                "audio_names=$analyzedAudioSummary audio_sizes=$analyzedAudioSizes " +
                                                "audio_pair_state=$analyzedPairState",
                                        )
                                        var count =
                                            if (tempPath != null) {
                                                GogImportBridge.extractFiles(
                                                    tempPath!!,
                                                    stagingDir.absolutePath,
                                                    progress,
                                                    includeAudio = includeAudio,
                                                    expectedFiles = fileList,
                                                )
                                            } else {
                                                context.contentResolver
                                                    .openFileDescriptor(installerUri, "r")
                                                    ?.use { pfd ->
                                                        GogImportBridge.extractFilesFromFd(
                                                            pfd.fd,
                                                            stagingDir.absolutePath,
                                                            progress,
                                                            includeAudio = includeAudio,
                                                        )
                                                    } ?: -1
                                            }
                                        val requestedCount =
                                            fileList!!.count {
                                                includeAudio || !GogImportBridge.isAudioFile(it.name)
                                            }
                                        if (count != requestedCount) {
                                            LauncherDebugLog.log(
                                                "launcher-gog-extract-incomplete " +
                                                    "installer=$installerName requested=$requestedCount extracted=$count",
                                            )
                                            count = -1
                                        } else if (count > 0) {
                                            publishStagedArchiveFiles(stagingDir, setDir)
                                        }
                                        val srcManager = AudioSourceManager(filesDir)
                                        val hasGog =
                                            if (includeAudio && count > 0) {
                                                registerGogAudioSource(
                                                    srcManager,
                                                    filesDir,
                                                    setDir,
                                                    context,
                                                )
                                            } else {
                                                false
                                            }
                                        if (hasGog) {
                                            enableRedbookInConfig(filesDir, context)
                                        }
                                        val filesAfter = setDir.list()?.toSet() ?: emptySet()
                                        val newFiles = (filesAfter - filesBefore).sorted()
                                        val setAudioNames =
                                            filesAfter
                                                .asSequence()
                                                .filter { GogImportBridge.isAudioFile(it) }
                                                .sortedBy { it.lowercase(Locale.US) }
                                                .toList()
                                        val newAudioNames =
                                            newFiles
                                                .asSequence()
                                                .filter { GogImportBridge.isAudioFile(it) }
                                                .toList()
                                        val missingExpectedAudio =
                                            analyzedAudioNames.filter { expected ->
                                                setAudioNames.none {
                                                    it.equals(expected, ignoreCase = true)
                                                }
                                            }
                                        val newAudioSummary = summarizeGogAudioFiles(newAudioNames)
                                        val setAudioSummary = summarizeGogAudioFiles(setAudioNames)
                                        val expectedAudioState =
                                            describeNamedFileStates(setDir, analyzedAudioNames)
                                        val setPairState = describeGogPairState(setAudioNames)
                                        val freeAfterBytes = stagingDir.usableSpace
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-result " +
                                                "installer=$installerName source=$sourceKind " +
                                                "include_audio=$includeAudio extracted=$count has_gog_pair=$hasGog " +
                                                "new_files=${newFiles.size} new_audio=$newAudioSummary",
                                        )
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-result-audio " +
                                                "installer=$installerName source=$sourceKind " +
                                                "set_audio=$setAudioSummary set_pair_state=$setPairState " +
                                                "missing_expected_audio=${summarizeGogAudioFiles(
                                                    missingExpectedAudio,
                                                )}",
                                        )
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-result-files " +
                                                "installer=$installerName source=$sourceKind " +
                                                "expected_audio_state=$expectedAudioState " +
                                                "free_after_bytes=$freeAfterBytes",
                                        )
                                        withContext(Dispatchers.Main) {
                                            extractedCount = count
                                            extractedFileNames = newFiles
                                            status =
                                                if (count > 0) {
                                                    val msg = "Extracted $count file(s)"
                                                    if (hasGog) {
                                                        "$msg. CD audio source registered and music mode set to Redbook"
                                                    } else {
                                                        msg
                                                    }
                                                } else {
                                                    "No files extracted"
                                                }
                                        }
                                    } catch (e: InsufficientStorageException) {
                                        Log.e("DXX-GogImport", "Extraction stopped for storage", e)
                                        withContext(Dispatchers.Main) {
                                            storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                            status = "Not enough free space"
                                            errorMsg = e.message
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-GogImport", "Extraction failed", e)
                                        LauncherDebugLog.log(
                                            "launcher-gog-extract-error installer=$installerName source=$sourceKind " +
                                                "include_audio=$includeAudio message=${e.message ?: e.javaClass.simpleName}",
                                        )
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                            errorMsg = e.message
                                        }
                                    } finally {
                                        stagingDir.deleteRecursively()
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth().focusRequester(extractFocus),
                    ) {
                        Text("Extract to \u201c${setDir.name}\u201d", fontSize = 13.sp)
                    }
                }

                // Done button
                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            tempPath?.let { File(it).delete() }
                            cleanupTmpDir(filesDir)
                            onImported()
                        },
                        modifier = Modifier.fillMaxWidth().focusRequester(doneFocus),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                // Progress indicator
                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    if (progressFile.isNotEmpty()) {
                        val pctText = if (progressPct > 0f) " (${(progressPct * 100).toInt()}%)" else ""
                        Text(
                            progressFile + pctText,
                            fontSize = 11.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    LinearProgressIndicator(
                        progress = { progressPct },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            }
        },
    )
}

// -- SOW archive import dialog ---------------------------------------------

@Composable
internal fun SowImportDialog(
    sowName: String,
    sowUri: Uri,
    filesDir: File,
    setDir: File,
    context: Context,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var status by remember { mutableStateOf("Preparing\u2026") }
    var processing by remember { mutableStateOf(false) }
    var copyingArchive by remember { mutableStateOf(true) }
    var extractedCount by remember { mutableIntStateOf(0) }
    var tempPath by remember { mutableStateOf<String?>(null) }
    var copyProgressBytes by remember { mutableLongStateOf(0L) }
    var copyProgressTotal by remember { mutableLongStateOf(0L) }
    var extractProgressBytes by remember { mutableLongStateOf(0L) }
    var extractProgressTotal by remember { mutableLongStateOf(0L) }
    var storageFailureMessage by remember { mutableStateOf<String?>(null) }
    val extractFocus = remember { FocusRequester() }
    val doneFocus = remember { FocusRequester() }

    LaunchedEffect(tempPath, processing, extractedCount) {
        when {
            extractedCount > 0 -> doneFocus.requestFocus()
            tempPath != null && !processing -> extractFocus.requestFocus()
        }
    }

    // Copy SOW to temp
    LaunchedEffect(sowUri) {
        withContext(Dispatchers.IO) {
            try {
                val tmpDir = File(filesDir, "tmp")
                tmpDir.mkdirs()
                val tmpFile = File(tmpDir, requireSafeProviderDisplayName(sowName))
                withContext(Dispatchers.Main) {
                    status = "Copying SOW archive..."
                    copyProgressBytes = 0L
                    copyProgressTotal = 0L
                }
                copyUriToFileWithProgress(context, sowUri, tmpFile) { copied, total ->
                    withContext(Dispatchers.Main) {
                        copyProgressBytes = copied
                        copyProgressTotal = total
                    }
                }
                withContext(Dispatchers.Main) {
                    tempPath = tmpFile.absolutePath
                    status = "Ready to extract game files from SOW archive"
                    copyProgressBytes = 0L
                    copyProgressTotal = 0L
                }
            } catch (e: InsufficientStorageException) {
                Log.e("DXX-SowImport", "Copy stopped for storage", e)
                withContext(Dispatchers.Main) {
                    status = "Not enough free space"
                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                }
            } catch (e: Exception) {
                Log.e("DXX-SowImport", "Copy failed", e)
                withContext(Dispatchers.Main) { status = "Error: ${e.message}" }
            } finally {
                withContext(Dispatchers.Main) { copyingArchive = false }
            }
        }
    }

    storageFailureMessage?.let { message ->
        StorageFailureDialog(message = message, onDismiss = { storageFailureMessage = null })
    }

    AlertDialog(
        onDismissRequest = {
            if (!processing && !copyingArchive) {
                tempPath?.let { File(it).delete() }
                cleanupTmpDir(filesDir)
                onDismiss()
            }
        },
        confirmButton = {
            if (!processing && !copyingArchive) {
                TextButton(onClick = {
                    tempPath?.let { File(it).delete() }
                    cleanupTmpDir(filesDir)
                    onDismiss()
                }) { Text("Close") }
            }
        },
        title = { Text("Import SOW Archive", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                Text(sowName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(8.dp))
                Text(status, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)

                if (copyProgressTotal > 0L) {
                    Spacer(modifier = Modifier.height(6.dp))
                    val copyPct = (copyProgressBytes.toFloat() / copyProgressTotal.toFloat()).coerceIn(0f, 1f)
                    LinearProgressIndicator(progress = { copyPct }, modifier = Modifier.fillMaxWidth())
                }

                // Extract button
                if (tempPath != null && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                try {
                                    ImportStorageGuard.requireFreeSpace(
                                        setDir,
                                        tempPath?.let { File(it).length() } ?: 0L,
                                        "extract $sowName",
                                    )
                                } catch (e: InsufficientStorageException) {
                                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                    status = "Not enough free space"
                                    return@launch
                                }
                                processing = true
                                status = "Extracting game files\u2026"
                                extractProgressBytes = 0L
                                extractProgressTotal = 0L
                                withContext(Dispatchers.IO) {
                                    val stagingDir =
                                        File(
                                            setDir.parentFile ?: filesDir,
                                            ".sow-import-${System.nanoTime()}",
                                        )
                                    try {
                                        check(stagingDir.mkdirs()) {
                                            "Could not create extraction staging directory"
                                        }
                                        val progress =
                                            object : DiscImportBridge.ExtractProgress {
                                                override fun onProgress(
                                                    currentFile: String,
                                                    bytesDone: Long,
                                                    bytesTotal: Long,
                                                ): Int {
                                                    val pct =
                                                        if (bytesTotal > 0L) {
                                                            ((bytesDone * 100L) / bytesTotal).toInt()
                                                        } else {
                                                            0
                                                        }
                                                    mainHandler.post {
                                                        status = "Extracting $currentFile ($pct%)"
                                                        extractProgressBytes = bytesDone
                                                        extractProgressTotal = bytesTotal
                                                    }
                                                    return 0
                                                }
                                            }
                                        val count =
                                            DiscImportBridge.extractSowFiles(
                                                tempPath!!,
                                                stagingDir.absolutePath,
                                                progress,
                                            )
                                        if (count > 0) {
                                            publishStagedArchiveFiles(stagingDir, setDir)
                                        }
                                        withContext(Dispatchers.Main) {
                                            extractedCount = count.coerceAtLeast(0)
                                            status =
                                                if (count > 0) {
                                                    "Extracted $count game file(s)"
                                                } else if (count < 0) {
                                                    "Archive extraction failed"
                                                } else {
                                                    "No game files found in archive"
                                                }
                                        }
                                    } catch (e: InsufficientStorageException) {
                                        Log.e("DXX-SowImport", "Extraction stopped for storage", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Not enough free space"
                                            storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-SowImport", "Extraction failed", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                        }
                                    } finally {
                                        stagingDir.deleteRecursively()
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth().focusRequester(extractFocus),
                    ) {
                        Text("Extract to \u201c${setDir.name}\u201d", fontSize = 13.sp)
                    }
                }

                // Done button
                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            tempPath?.let { File(it).delete() }
                            cleanupTmpDir(filesDir)
                            onImported()
                        },
                        modifier = Modifier.fillMaxWidth().focusRequester(doneFocus),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                // Progress indicator
                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    if (extractProgressTotal > 0L) {
                        val extractPct =
                            (extractProgressBytes.toFloat() / extractProgressTotal.toFloat()).coerceIn(0f, 1f)
                        LinearProgressIndicator(progress = { extractPct }, modifier = Modifier.fillMaxWidth())
                    } else {
                        CircularProgressIndicator(modifier = Modifier.size(24.dp))
                    }
                }
            }
        },
    )
}

// -- BIN/CUE disc import dialog --------------------------------------------

/**
 * Dialog for importing a BIN/CUE disc image.
 *
 * Flow:
 *  1. Copies CUE to temp, gets BIN sizes via content resolver
 *  2. Parses CUE to discover tracks (data + audio)
 *  3. Optionally identifies disc via SHA1 hashing
 *  4. Extracts game files from data track
 *  5. Copies BIN to filesDir and registers as audio source
 */
@Composable
internal fun DiscImportDialog(
    cueName: String,
    cueUri: Uri,
    binUris: List<Pair<String, Uri>>,
    filesDir: File,
    setDir: File,
    context: Context,
    onChanged: () -> Unit,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var status by remember { mutableStateOf("Ready to process") }
    var tracks by remember { mutableStateOf<List<DiscImportBridge.CueTrack>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var stagingCue by remember { mutableStateOf(true) }
    var dataExtracted by remember { mutableIntStateOf(0) }
    var progressBytes by remember { mutableLongStateOf(0L) }
    var progressTotal by remember { mutableLongStateOf(0L) }
    var audioRegistered by remember { mutableStateOf(false) }
    var binSizes by remember { mutableStateOf<List<Long>>(emptyList()) }
    var discLabel by remember { mutableStateOf<String?>(null) }
    var discId by remember { mutableStateOf<String?>(null) }
    var legacyDiscId by remember { mutableStateOf(0L) }
    var storageFailureMessage by remember { mutableStateOf<String?>(null) }
    // Temp CUE path for native parsing
    var tempCuePath by remember { mutableStateOf<String?>(null) }
    var orderedBinUris by remember { mutableStateOf(binUris) }
    val extractFocus = remember { FocusRequester() }
    val addAudioFocus = remember { FocusRequester() }
    val doneFocus = remember { FocusRequester() }
    val scrollState = rememberScrollState()
    val inputModeManager = LocalInputModeManager.current

    LaunchedEffect(tracks, processing, dataExtracted, audioRegistered) {
        val hasPendingData = !processing && tracks?.any { it.isData } == true && dataExtracted == 0
        val hasPendingAudio = !processing && tracks?.any { it.isAudio } == true && !audioRegistered
        val nextFocusRequester =
            when {
                hasPendingData -> extractFocus
                hasPendingAudio -> addAudioFocus
                !processing && tracks != null -> doneFocus
                else -> null
            }

        if (nextFocusRequester != null) {
            inputModeManager.requestInputMode(InputMode.Keyboard)
            withFrameNanos { }
            nextFocusRequester.requestFocus()
        }
    }

    // Copy CUE + parse tracks on first composition
    LaunchedEffect(cueUri) {
        withContext(Dispatchers.IO) {
            try {
                Log.i("DXX-DiscImport", "Starting disc import: cue=$cueName, bins=${binUris.size}")
                // Copy CUE file to temp
                val tmpDir = File(filesDir, "tmp")
                tmpDir.mkdirs()
                val tmpCue = File(tmpDir, requireSafeProviderDisplayName(cueName).lowercase(Locale.ROOT))
                LauncherFileCopy.copyUriToFile(
                    context,
                    cueUri,
                    tmpCue,
                    cueName,
                    maxBytes = CD_CUE_MAX_BYTES,
                )
                tempCuePath = tmpCue.absolutePath
                Log.i("DXX-DiscImport", "CUE copied to ${tmpCue.absolutePath} (${tmpCue.length()} bytes)")

                val orderedImages = orderCueEntries(tmpCue, binUris) { it.first }
                if (orderedImages.missingNames.isNotEmpty()) {
                    withContext(Dispatchers.Main) {
                        status = buildMissingDiscImageSelectionMessage(orderedImages.missingNames)
                    }
                    return@withContext
                }
                val selectedBinUris = orderedImages.orderedEntries

                // Get disc image sizes in CUE FILE order
                val parsedBinSizes =
                    selectedBinUris
                        .map { (name, uri) ->
                            val size =
                                ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri)
                                    ?: throw java.io.IOException("Could not determine disc image size: $name")
                            Log.i("DXX-DiscImport", "Disc image '$name' size=$size")
                            size
                        }

                if (parsedBinSizes.isEmpty()) {
                    withContext(Dispatchers.Main) {
                        status =
                            "No disc image files selected \u2014 please select the .cue and all referenced .bin/.img files"
                    }
                    return@withContext
                }

                // Parse CUE
                val parsed = DiscImportBridge.parseCue(tmpCue.absolutePath, parsedBinSizes.toLongArray())
                Log.i("DXX-DiscImport", "parseCue returned ${parsed?.size ?: "null"} tracks")
                withContext(Dispatchers.Main) {
                    orderedBinUris = selectedBinUris
                    binSizes = parsedBinSizes
                    tracks = parsed
                    if (parsed != null) {
                        val dataCount = parsed.count { it.isData }
                        val audioCount = parsed.count { it.isAudio }
                        status =
                            buildDiscImageTrackSummary(
                                dataTrackCount = dataCount,
                                audioTrackCount = audioCount,
                                imageCount = selectedBinUris.size,
                                extraNames = orderedImages.extraNames,
                            )
                    } else {
                        status = "Failed to parse CUE file. Check that every referenced .bin/.img file was selected"
                    }
                }
            } catch (e: InsufficientStorageException) {
                Log.e("DXX-DiscImport", "CUE staging stopped for storage", e)
                withContext(Dispatchers.Main) {
                    status = "Not enough free space"
                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                }
            } catch (e: Exception) {
                Log.e("DXX-DiscImport", "CUE parse failed", e)
                withContext(Dispatchers.Main) { status = "Error: ${e.message}" }
            } finally {
                withContext(Dispatchers.Main) { stagingCue = false }
            }
        }
    }

    storageFailureMessage?.let { message ->
        StorageFailureDialog(message = message, onDismiss = { storageFailureMessage = null })
    }

    AlertDialog(
        onDismissRequest = {
            if (!processing && !stagingCue) {
                tempCuePath?.let { File(it).delete() }
                onDismiss()
            }
        },
        confirmButton = {},
        title = { Text("Import Disc Image Set", fontWeight = FontWeight.Bold) },
        text = {
            Box {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    // Action buttons
                    if (tracks != null && !processing) {
                        // Extract game files from data track
                        val hasDataTrack = tracks?.any { it.isData } == true
                        if (hasDataTrack && dataExtracted == 0) {
                            Button(
                                onClick = {
                                    scope.launch {
                                        try {
                                            val preparedImages =
                                                withContext(Dispatchers.IO) {
                                                    prepareDiscImages(
                                                        context = context,
                                                        tracks = tracks!!,
                                                        imageUris = orderedBinUris,
                                                        imageSizes = binSizes,
                                                    )
                                                }
                                            try {
                                                ImportStorageGuard.requireFreeSpace(
                                                    setDir,
                                                    cueDataTrackPeakStorageBytes(
                                                        tracks!!,
                                                        preparedImages.stagedImageBytes,
                                                    ),
                                                    "extract disc game files",
                                                )
                                            } catch (failure: Exception) {
                                                preparedImages.close()
                                                throw failure
                                            }
                                            processing = true
                                            status = "Extracting game files..."
                                            progressBytes = 0L
                                            progressTotal = 0L
                                            withContext(Dispatchers.IO) {
                                                preparedImages.use { prepared ->
                                                    extractPickedCueDataTracks(
                                                        context = context,
                                                        setDir = setDir,
                                                        tracks = tracks!!,
                                                        orderedBinUris = orderedBinUris,
                                                        preparedImages = prepared,
                                                        postUpdate = { mainHandler.post(it) },
                                                        onStatus = { status = it },
                                                        onProgress = { current, total ->
                                                            progressBytes = current
                                                            progressTotal = total
                                                        },
                                                    )
                                                }
                                            }.also { result ->
                                                dataExtracted = if (result.succeeded) result.totalExtracted else 0
                                                status =
                                                    when {
                                                        !result.succeeded -> {
                                                            "Data track ${result.failedTrackNumber} failed; no files imported"
                                                        }

                                                        result.primaryExtracted > 0 -> {
                                                            buildDiscExtractSummary(
                                                                result.primaryExtracted,
                                                                "disc file(s) from ${result.processedTracks} data track(s)",
                                                                result.sowExtracted,
                                                            )
                                                        }

                                                        else -> {
                                                            "No supported game files found on data tracks"
                                                        }
                                                    }
                                                if (dataExtracted > 0) onChanged()
                                            }
                                        } catch (e: InsufficientStorageException) {
                                            Log.w("DXX-DiscImport", "Disc image extraction stopped for storage", e)
                                            storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                            status = "Not enough free space"
                                        } catch (e: Exception) {
                                            Log.e("DXX-DiscImport", "Disc image extraction failed", e)
                                            status = "Cannot extract disc image: ${e.message}"
                                        }
                                        processing = false
                                    }
                                },
                                modifier = Modifier.fillMaxWidth().focusRequester(extractFocus),
                            ) {
                                Text("Extract Game Files", fontSize = 13.sp)
                            }
                        }

                        // Register as audio source
                        val hasAudioTracks = tracks?.any { it.isAudio } == true
                        if (hasAudioTracks && !audioRegistered) {
                            Spacer(modifier = Modifier.height(4.dp))
                            Button(
                                onClick = {
                                    scope.launch {
                                        processing = true
                                        status = "Registering audio source\u2026"
                                        withContext(Dispatchers.IO) {
                                            try {
                                                val parsedTracks = tracks!!
                                                val audioCount = parsedTracks.count { it.isAudio }

                                                val binNames = mutableListOf<String>()
                                                var firstBinUri: Uri? = null
                                                for ((name, uri) in orderedBinUris) {
                                                    if (!persistReadPermissionForUri(context, uri)) {
                                                        Log.w("DXX-DiscImport", "Could not persist URI for $name")
                                                    }
                                                    binNames.add(name.lowercase())
                                                    if (firstBinUri == null) firstBinUri = uri
                                                }

                                                // Try to identify the disc via SAF fd
                                                try {
                                                    val identifier = DiscIdentifier(context)
                                                    val firstAudio = parsedTracks.first { it.isAudio }
                                                    val binUri = orderedBinUris[firstAudio.fileIndex].second
                                                    val pfd = context.contentResolver.openFileDescriptor(binUri, "r")
                                                    if (pfd != null) {
                                                        val trackBytes = firstAudio.numSectors.toLong() * 2352
                                                        val trackOffset = firstAudio.startSector.toLong() * 2352
                                                        val hash =
                                                            pfd.use {
                                                                java.io.FileInputStream(it.fileDescriptor).use { fis ->
                                                                    DiscIdentifier.sha1Hash(
                                                                        fis,
                                                                        trackOffset,
                                                                        trackBytes,
                                                                    )
                                                                }
                                                            }
                                                        when (hash) {
                                                            is DiscIdentifier.Sha1HashResult.Complete -> {
                                                                val match =
                                                                    identifier.identify(
                                                                        mapOf(firstAudio.trackNum to hash.sha1),
                                                                    )
                                                                if (match.matched) {
                                                                    discLabel = match.label
                                                                    discId = match.disc?.id
                                                                    match.disc?.legacyDiscId?.let {
                                                                        legacyDiscId = java.lang.Long.decode(it)
                                                                    }
                                                                }
                                                            }

                                                            is DiscIdentifier.Sha1HashResult.Failed -> {
                                                                Log.w(
                                                                    "DXX-DiscImport",
                                                                    "Disc track hash failed: ${hash.problem}",
                                                                )
                                                            }

                                                            DiscIdentifier.Sha1HashResult.Canceled -> {
                                                                Log.w("DXX-DiscImport", "Disc track hash canceled")
                                                            }
                                                        }
                                                    }
                                                } catch (e: Exception) {
                                                    Log.w("DXX-DiscImport", "Disc identification failed", e)
                                                }

                                                val srcManager = AudioSourceManager(filesDir)
                                                val id = discId ?: "custom-${System.currentTimeMillis()}"
                                                val existingAudioFileNames = filesDir.list()?.toSet() ?: emptySet()
                                                val sourceFileStem =
                                                    chooseUniqueCdAudioImportStem(
                                                        preferredStem = File(cueName).nameWithoutExtension,
                                                        existingFileNames = existingAudioFileNames,
                                                    )
                                                LauncherDebugLog.log(
                                                    "launcher-cd-import cue=$cueName bins=${orderedBinUris.size} mode=saf-in-place file_stem=$sourceFileStem",
                                                )
                                                val destCue = File(filesDir, "$sourceFileStem.cue")
                                                tempCuePath?.let {
                                                    LauncherFileCopy.copyFileToFile(
                                                        File(it),
                                                        destCue,
                                                        maxBytes = CD_CUE_MAX_BYTES,
                                                    )
                                                }
                                                var trackNames = emptyMap<Int, String>()
                                                try {
                                                    discId?.let { resolvedDiscId ->
                                                        trackNames =
                                                            FingerprintBridge.lookupTrackNames(
                                                                context,
                                                                resolvedDiscId,
                                                            )
                                                        Log.i(
                                                            "DXX-DiscImport",
                                                            "Looked up ${trackNames.size} track names for $resolvedDiscId",
                                                        )
                                                    }
                                                    if (trackNames.isEmpty()) {
                                                        withContext(Dispatchers.Main) {
                                                            status = "Identifying audio tracks..."
                                                        }
                                                        trackNames =
                                                            FingerprintBridge.fingerprintAndMatchDisc(
                                                                context,
                                                                context.contentResolver,
                                                                orderedBinUris.map { it.second },
                                                                parsedTracks,
                                                            )
                                                        Log.i(
                                                            "DXX-DiscImport",
                                                            "Fingerprinted ${trackNames.size} track names via SAF descriptors",
                                                        )
                                                    }
                                                } catch (e: Exception) {
                                                    Log.w("DXX-DiscImport", "Track name identification failed", e)
                                                }

                                                srcManager.addSource(
                                                    AudioSourceManager.AudioSource(
                                                        id = id,
                                                        cuePath = destCue.name,
                                                        binPaths = binNames,
                                                        discLabel = discLabel ?: cueName,
                                                        discId = discId ?: "unknown",
                                                        trackCount = parsedTracks.size,
                                                        audioTrackCount = audioCount,
                                                        audioTrackNumbers =
                                                            parsedTracks.filter { it.isAudio }.map { it.trackNum },
                                                        legacyDiscId = legacyDiscId,
                                                        trackNames = trackNames,
                                                        binContentUri = firstBinUri?.toString(),
                                                        binContentUris = orderedBinUris.map { it.second.toString() },
                                                        cueContentUri = cueUri.toString(),
                                                    ),
                                                )
                                                enableRedbookInConfig(filesDir, context)

                                                withContext(Dispatchers.Main) {
                                                    audioRegistered = true
                                                    status = "Audio source registered" +
                                                        if (discLabel != null) " ($discLabel)" else ""
                                                    onChanged()
                                                }
                                            } catch (e: Exception) {
                                                Log.e("DXX-DiscImport", "Audio registration failed", e)
                                                withContext(Dispatchers.Main) {
                                                    status = "Error: ${e.message}"
                                                }
                                            }
                                        }
                                        processing = false
                                    }
                                },
                                modifier = Modifier.fillMaxWidth().focusRequester(addAudioFocus),
                            ) {
                                Text("Add as Audio Source", fontSize = 13.sp)
                            }
                        }

                        // Done state
                        if (dataExtracted > 0 || audioRegistered) {
                            Spacer(modifier = Modifier.height(8.dp))
                            Button(
                                onClick = {
                                    tempCuePath?.let { File(it).delete() }
                                    onImported()
                                },
                                modifier = Modifier.fillMaxWidth().focusRequester(doneFocus),
                            ) {
                                Text("Done", fontSize = 13.sp)
                            }
                        }
                    }

                    if (!processing) {
                        if (tracks != null) {
                            Spacer(modifier = Modifier.height(4.dp))
                        }
                        TextButton(
                            onClick = onDismiss,
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Close", fontSize = 13.sp) }
                        Spacer(modifier = Modifier.height(12.dp))
                    }

                    Text(cueName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                    if (binUris.isNotEmpty()) {
                        Text(
                            "BIN: ${binUris.joinToString(", ") { it.first }}",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    if (discLabel != null) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            "Identified: $discLabel",
                            fontSize = 13.sp,
                            color = Color(0xFF4CAF50),
                        )
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        status,
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )

                    tracks?.let { trackList ->
                        Spacer(modifier = Modifier.height(8.dp))
                        trackList.forEach { track ->
                            val typeStr = if (track.isData) "DATA" else "AUDIO"
                            val sizeStr = formatSize(track.numSectors.toLong() * 2352)
                            Text(
                                "Track ${track.trackNum}: $typeStr ($sizeStr)" +
                                    if (track.title.isNotEmpty()) " - ${track.title}" else "",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }

                    if (processing) {
                        Spacer(modifier = Modifier.height(8.dp))
                        if (progressTotal > 0L) {
                            val progress = (progressBytes.toFloat() / progressTotal.toFloat()).coerceIn(0f, 1f)
                            LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
                        } else {
                            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                        }
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

@Composable
internal fun IsoImportDialog(
    isoName: String,
    isoUri: Uri,
    setDir: File,
    context: Context,
    onChanged: () -> Unit,
    onImported: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    val mainHandler = remember { android.os.Handler(android.os.Looper.getMainLooper()) }
    var status by remember { mutableStateOf("Ready to process") }
    var fileList by remember { mutableStateOf<List<DiscImportBridge.IsoFile>?>(null) }
    var processing by remember { mutableStateOf(false) }
    var extractedCount by remember { mutableIntStateOf(0) }
    var progressBytes by remember { mutableLongStateOf(0L) }
    var progressTotal by remember { mutableLongStateOf(0L) }
    var storageFailureMessage by remember { mutableStateOf<String?>(null) }
    val extractFocus = remember { FocusRequester() }
    val doneFocus = remember { FocusRequester() }

    LaunchedEffect(fileList, processing, extractedCount) {
        when {
            extractedCount > 0 -> doneFocus.requestFocus()
            fileList != null && !processing -> extractFocus.requestFocus()
        }
    }

    LaunchedEffect(isoUri) {
        withContext(Dispatchers.IO) {
            try {
                val pfd = context.contentResolver.openFileDescriptor(isoUri, "r")
                if (pfd == null) {
                    withContext(Dispatchers.Main) {
                        status = "Could not open ISO image"
                    }
                    return@withContext
                }

                val listed =
                    pfd.use {
                        DiscImportBridge.listIsoImageFiles(it.fd)
                    }
                withContext(Dispatchers.Main) {
                    fileList = listed
                    status =
                        if (listed != null) {
                            "Found ${listed.size} file(s) in ISO image"
                        } else {
                            "Failed to read ISO image"
                        }
                }
            } catch (e: Exception) {
                Log.e("DXX-DiscImport", "ISO scan failed", e)
                withContext(Dispatchers.Main) {
                    status = "Error: ${e.message}"
                }
            }
        }
    }

    storageFailureMessage?.let { message ->
        StorageFailureDialog(message = message, onDismiss = { storageFailureMessage = null })
    }

    AlertDialog(
        onDismissRequest = { if (!processing) onDismiss() },
        confirmButton = {
            if (!processing) {
                TextButton(onClick = onDismiss) { Text("Close") }
            }
        },
        title = { Text("Import ISO Image", fontWeight = FontWeight.Bold) },
        text = {
            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                Text(isoName, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    "Standalone ISO import extracts game data only. CD audio requires a CUE/BIN image.",
                    fontSize = 11.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    status,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                fileList?.let { entries ->
                    if (entries.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        entries.take(12).forEach { entry ->
                            Text(
                                "${entry.path} (${formatSize(entry.size)})",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                        if (entries.size > 12) {
                            Text(
                                "...and ${entries.size - 12} more",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }

                if (fileList != null && !processing && extractedCount == 0) {
                    Spacer(modifier = Modifier.height(12.dp))
                    Button(
                        onClick = {
                            scope.launch {
                                try {
                                    ImportStorageGuard.requireFreeSpace(
                                        setDir,
                                        fileList.orEmpty().sumOf { it.size },
                                        "extract $isoName",
                                    )
                                } catch (e: InsufficientStorageException) {
                                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                    status = "Not enough free space"
                                    return@launch
                                }
                                processing = true
                                status = "Extracting game files..."
                                progressBytes = 0L
                                progressTotal = 0L
                                withContext(Dispatchers.IO) {
                                    try {
                                        val pfd = context.contentResolver.openFileDescriptor(isoUri, "r")
                                        if (pfd != null) {
                                            val progress =
                                                object : DiscImportBridge.ExtractProgress {
                                                    override fun onProgress(
                                                        currentFile: String,
                                                        bytesDone: Long,
                                                        bytesTotal: Long,
                                                    ): Int {
                                                        val pct =
                                                            if (bytesTotal > 0L) {
                                                                ((bytesDone * 100L) / bytesTotal).toInt()
                                                            } else {
                                                                0
                                                            }
                                                        mainHandler.post {
                                                            status = "Extracting $currentFile ($pct%)"
                                                            progressBytes = bytesDone
                                                            progressTotal = bytesTotal
                                                        }
                                                        return 0
                                                    }
                                                }
                                            val isoExtracted =
                                                pfd.use {
                                                    DiscImportBridge.extractIsoImageFiles(
                                                        it.fd,
                                                        setDir.absolutePath,
                                                        progress,
                                                    )
                                                }
                                            val sowExtracted =
                                                if (isoExtracted > 0) {
                                                    postProcessImportedDiscFiles(setDir, progress)
                                                } else {
                                                    0
                                                }
                                            withContext(Dispatchers.Main) {
                                                extractedCount =
                                                    if (sowExtracted < 0) {
                                                        0
                                                    } else {
                                                        isoExtracted.coerceAtLeast(0) + sowExtracted
                                                    }
                                                status =
                                                    when {
                                                        sowExtracted < 0 -> {
                                                            "Could not scan extracted disc files for SOW archives"
                                                        }

                                                        isoExtracted > 0 -> {
                                                            buildDiscExtractSummary(
                                                                isoExtracted,
                                                                "disc file(s)",
                                                                sowExtracted,
                                                            )
                                                        }

                                                        else -> {
                                                            "No supported game files found in ISO image"
                                                        }
                                                    }
                                                if (extractedCount > 0) {
                                                    onChanged()
                                                }
                                            }
                                        } else {
                                            withContext(Dispatchers.Main) {
                                                status = "Could not open ISO image"
                                            }
                                        }
                                    } catch (e: Exception) {
                                        Log.e("DXX-DiscImport", "ISO extract failed", e)
                                        withContext(Dispatchers.Main) {
                                            status = "Extract error: ${e.message}"
                                        }
                                    }
                                }
                                processing = false
                            }
                        },
                        modifier = Modifier.fillMaxWidth().focusRequester(extractFocus),
                    ) {
                        Text("Extract Game Files", fontSize = 13.sp)
                    }
                }

                if (extractedCount > 0) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = onImported,
                        modifier = Modifier.fillMaxWidth().focusRequester(doneFocus),
                    ) {
                        Text("Done", fontSize = 13.sp)
                    }
                }

                if (processing) {
                    Spacer(modifier = Modifier.height(8.dp))
                    if (progressTotal > 0L) {
                        val progress = (progressBytes.toFloat() / progressTotal.toFloat()).coerceIn(0f, 1f)
                        LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
                    } else {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    }
                }
            }
        },
    )
}
