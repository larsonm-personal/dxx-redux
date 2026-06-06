package com.dxxredux.app

import android.widget.Toast
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
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
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
internal fun BoxScope.SetupScrollArrows(scrollState: ScrollState) {
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

@Composable
internal fun GameSectionHeader(
    title: String,
    ready: Boolean,
    expanded: Boolean,
    onToggle: () -> Unit,
    notReadyLabel: String = "\u2717 Missing",
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = title,
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = if (ready) "\u2713 Ready" else notReadyLabel,
            color = if (ready) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.width(8.dp))
        TextButton(
            onClick = onToggle,
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide Files" else "Show Files",
                fontSize = 12.sp,
            )
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )
}

@Composable
internal fun SectionHeader(title: String) {
    Text(
        text = title,
        fontSize = 15.sp,
        fontWeight = FontWeight.SemiBold,
        color = MaterialTheme.colorScheme.onSurface,
        modifier = Modifier.padding(bottom = 4.dp, top = 2.dp),
    )
}

@Composable
internal fun ModsSection(
    filesDir: File,
    setDir: File,
    refreshTrigger: Int,
) {
    val context = LocalContext.current
    val modManager = remember { ModManager(filesDir) }
    var mods by remember { mutableStateOf(modManager.listMods()) }
    var expanded by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<String?>(null) }
    var detailTarget by remember { mutableStateOf<ModManager.ModInfo?>(null) }
    var detailInfo by remember { mutableStateOf<ModManager.ModDetails?>(null) }
    var detailLoading by remember { mutableStateOf(false) }
    val modDownloadProgress = remember { mutableStateMapOf<String, Int>() }
    val scanCache = remember { mutableStateMapOf<String, DxaTextureScanner.ScanResult?>() }
    val scope = rememberCoroutineScope()

    fun logTextureScan(
        mod: ModManager.ModInfo,
        file: File,
        scanResult: DxaTextureScanner.ScanResult,
    ) {
        val summary =
            "mod-dxa-scan file=${mod.filename} bytes=${file.length()} " +
                "textures=${scanResult.textureCount} oversized=${scanResult.oversizedCount} " +
                "max=${scanResult.maxWidth}x${scanResult.maxHeight}"
        if (scanResult.oversizedEntries.isEmpty()) {
            LauncherDebugLog.log(summary)
            return
        }
        val details =
            scanResult.oversizedEntries.joinToString(" | ") {
                "${it.name} ${it.width}x${it.height} pow2=${it.pow2Width}x${it.pow2Height}"
            }
        LauncherDebugLog.log("$summary entries=$details")
    }

    LaunchedEffect(refreshTrigger) {
        modManager.reload()
        mods = modManager.listMods()
    }

    LaunchedEffect(detailTarget?.filename, setDir.absolutePath, refreshTrigger, mods) {
        val target = detailTarget ?: return@LaunchedEffect
        detailInfo = null
        detailLoading = true
        detailInfo =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                modManager.getModDetails(target, setDir)
            }
        detailLoading = false
    }

    LaunchedEffect(expanded) {
        if (expanded) {
            for (mod in mods) {
                if (mod.filename !in scanCache &&
                    mod.filename.contains("textur", ignoreCase = true)
                ) {
                    val file = File(filesDir, "mods/${mod.filename}")
                    val scanResult =
                        withContext(kotlinx.coroutines.Dispatchers.IO) {
                            DxaTextureScanner.scan(file)
                        }
                    if (scanResult != null) {
                        logTextureScan(mod, file, scanResult)
                    }
                    scanCache[mod.filename] = scanResult
                }
            }
        }
    }

    val enabledCount = mods.count { it.enabled }
    val totalCount = mods.size
    val summary = if (totalCount == 0) "none" else "$enabledCount of $totalCount enabled"

    GameSectionHeader(
        title = "Mods",
        ready = true,
        expanded = expanded,
        onToggle = { expanded = !expanded },
        notReadyLabel = summary,
    )

    if (expanded) {
        if (mods.isEmpty()) {
            Text(
                "No mods installed",
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 8.dp),
            )
        } else {
            Text(
                summary,
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            Text(
                "Load order is top to bottom; higher enabled mods take priority when files overlap",
                fontSize = 11.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 6.dp),
            )
            mods.forEachIndexed { index, mod ->
                ModRow(
                    mod = mod,
                    isFirst = index == 0,
                    isLast = index == mods.size - 1,
                    scanResult = scanCache[mod.filename],
                    onToggle = { enabled ->
                        modManager.setEnabled(mod.filename, enabled)
                        mods = modManager.listMods()
                    },
                    onMoveUp = {
                        modManager.moveUp(index)
                        mods = modManager.listMods()
                    },
                    onMoveDown = {
                        modManager.moveDown(index)
                        mods = modManager.listMods()
                    },
                    onDetails = { detailTarget = mod },
                    onDelete = { deleteTarget = mod.filename },
                )
            }
        }

        val installedNames = mods.map { it.filename.lowercase() }.toSet()
        val uninstalled = RECOMMENDED_MODS.filter { it.filename.lowercase() !in installedNames }
        if (uninstalled.isNotEmpty()) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                "Recommended",
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            uninstalled.forEach { rec ->
                RecommendedModRow(
                    rec = rec,
                    progress = modDownloadProgress[rec.filename],
                    onDownload = {
                        val modsDir = File(filesDir, "mods")
                        scope.launch {
                            setupDownloadFile(
                                url = rec.downloadUrl,
                                destDir = modsDir,
                                filename = rec.filename,
                                onProgress = { pct ->
                                    modDownloadProgress[rec.filename] = pct
                                },
                                onDone = { success ->
                                    modDownloadProgress[rec.filename] =
                                        if (success) -2 else -1
                                    if (success) {
                                        val file = File(modsDir, rec.filename)
                                        modManager.importCompleted(
                                            rec.filename,
                                            rec.displayName,
                                            file.length(),
                                            rec.game,
                                        )
                                        mods = modManager.listMods()
                                    } else {
                                        Toast
                                            .makeText(
                                                context,
                                                "Download failed: ${rec.filename}",
                                                Toast.LENGTH_LONG,
                                            ).show()
                                    }
                                },
                            )
                        }
                    },
                )
            }
        }
    }

    deleteTarget?.let { filename ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text("Delete Mod") },
            text = { Text("Remove $filename? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    modManager.deleteMod(filename)
                    mods = modManager.listMods()
                    if (detailTarget?.filename == filename) {
                        detailTarget = null
                        detailInfo = null
                    }
                    deleteTarget = null
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) { Text("Cancel") }
            },
        )
    }

    detailTarget?.let { mod ->
        ModDetailsDialog(
            mod = mod,
            details = detailInfo,
            loading = detailLoading,
            onDismiss = {
                detailTarget = null
                detailInfo = null
            },
        )
    }
}

@Composable
private fun ModDetailsDialog(
    mod: ModManager.ModInfo,
    details: ModManager.ModDetails?,
    loading: Boolean,
    onDismiss: () -> Unit,
) {
    var constituentTarget by remember { mutableStateOf<SectorgameMissionZip.Constituent?>(null) }
    constituentTarget?.let { constituent ->
        MissionZipConstituentDialog(
            constituent = constituent,
            onDismiss = { constituentTarget = null },
        )
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text(mod.displayName, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp),
            ) {
                if (loading || details == null) {
                    Row(
                        modifier = Modifier.padding(vertical = 8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        CircularProgressIndicator(modifier = Modifier.size(18.dp), strokeWidth = 2.dp)
                        Spacer(modifier = Modifier.width(10.dp))
                        Text("Reading mod archive", fontSize = 12.sp)
                    }
                } else {
                    Column(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .verticalScroll(scrollState)
                                .padding(end = 8.dp),
                    ) {
                        Text(
                            details.archivePath,
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurface,
                            modifier = Modifier.padding(bottom = 4.dp),
                        )
                        DetailRow(
                            "Archive",
                            "${details.fileCount} files, ${setupSectionFormatSize(details.archiveSizeBytes)}",
                        )
                        DetailRow("Game", mod.game.uppercase(Locale.US))
                        DetailRow("State", if (mod.enabled) "Enabled" else "Disabled")
                        DetailRow(
                            "Manifest",
                            if (details.missionZip != null) {
                                "Mission ZIP"
                            } else {
                                details.manifestSchema ?: "Not present"
                            },
                        )

                        details.missionZip?.let { missionZip ->
                            DetailRow("Category", missionZip.category.replaceFirstChar { it.uppercase() })
                            DetailRow("Import mode", missionZip.importMode.replace('_', ' '))
                            ModDetailSectionTitle("Mission")
                            DetailRow("Title", missionZip.mission.displayName)
                            missionZip.mission.type?.let { DetailRow("Type", it) }
                            missionZip.mission.author?.let { DetailRow("Author", it) }
                            missionZip.mission.editor?.let { DetailRow("Editor", it) }
                            DetailRow("Levels", missionZip.mission.levelNames.size.toString())
                            if (missionZip.mission.levelNames.isNotEmpty()) {
                                ModDetailLine(missionZip.mission.levelNames.joinToString(", "))
                            }
                        }

                        if (details.problems.isNotEmpty()) {
                            ModDetailSectionTitle("Problems")
                            details.problems.forEach { problem ->
                                ModDetailLine(problem, color = MaterialTheme.colorScheme.error)
                            }
                        }

                        ModDetailSectionTitle("Files")
                        if (details.missionZip != null) {
                            details.missionZip.constituents.forEach { constituent ->
                                TextButton(
                                    onClick = { constituentTarget = constituent },
                                    contentPadding = PaddingValues(horizontal = 0.dp, vertical = 2.dp),
                                    modifier = Modifier.fillMaxWidth(),
                                ) {
                                    Column(modifier = Modifier.fillMaxWidth()) {
                                        Text(
                                            constituent.name,
                                            fontSize = 12.sp,
                                            fontWeight = FontWeight.SemiBold,
                                            color = MaterialTheme.colorScheme.onSurface,
                                        )
                                        Text(
                                            "${missionZipRoleLabel(constituent.role)}, ${
                                                setupSectionFormatSize(
                                                    constituent.sizeBytes,
                                                )
                                            }",
                                            fontSize = 11.sp,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        )
                                    }
                                }
                            }
                        } else if (details.categories.isEmpty()) {
                            ModDetailLine("No readable archive entries")
                        } else {
                            details.categories.forEach { category ->
                                val sizeText =
                                    if (category.sizeBytes > 0) {
                                        ", ${setupSectionFormatSize(category.sizeBytes)}"
                                    } else {
                                        ""
                                    }
                                Text(
                                    "${category.label}: ${category.count} files$sizeText",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.onSurface,
                                    modifier = Modifier.padding(top = 2.dp),
                                )
                                if (category.examples.isNotEmpty()) {
                                    val exampleText =
                                        category.examples.joinToString("; ") +
                                            if (category.examplesTruncated) "; ..." else ""
                                    Text(
                                        exampleText,
                                        fontSize = 11.sp,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                        modifier = Modifier.padding(bottom = 1.dp),
                                    )
                                }
                            }
                        }

                        if (details.missionZip == null) {
                            ModDetailSectionTitle("Patches")
                            if (details.patches.isEmpty()) {
                                ModDetailLine("No metadata patches")
                            } else {
                                details.patches.forEach { patch ->
                                    Text(
                                        patch.path,
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = MaterialTheme.colorScheme.onSurface,
                                        modifier = Modifier.padding(top = 2.dp),
                                    )
                                    val operationText = patch.operationCount?.let { ", $it ops" } ?: ""
                                    ModDetailLine("Size ${setupSectionFormatSize(patch.sizeBytes)}$operationText")
                                    ModDetailLine(
                                        "Affects ${patch.affectedFiles.ifEmpty {
                                            listOf(
                                                "unknown target",
                                            )
                                        }.joinToString(", ")}",
                                    )
                                    if (patch.expectedBaseVersions.isNotEmpty()) {
                                        ModDetailLine("Expects ${patch.expectedBaseVersions.joinToString(", ")}")
                                    }
                                }
                            }
                        }

                        if (details.missionZip == null) {
                            ModDetailSectionTitle("Base Files")
                            if (details.baseRequirements.isEmpty()) {
                                ModDetailLine("No base-file requirements in manifest")
                            } else {
                                details.baseRequirements.forEach { requirement ->
                                    val status =
                                        when {
                                            !requirement.required -> "optional"
                                            requirement.ok -> "match"
                                            requirement.actualSha256 == null -> "missing"
                                            else -> "mismatch"
                                        }
                                    val statusColor =
                                        if (requirement.ok) {
                                            MaterialTheme.colorScheme.onSurfaceVariant
                                        } else {
                                            MaterialTheme.colorScheme.error
                                        }
                                    Text(
                                        "${requirement.filename}: ${requirement.expectedVersion} ($status)",
                                        fontSize = 12.sp,
                                        color = statusColor,
                                        modifier = Modifier.padding(top = 2.dp),
                                    )
                                    baseRequirementSha256Lines(requirement).forEach { line ->
                                        ModDetailLine(line, color = statusColor)
                                    }
                                    if (requirement.reason.isNotBlank()) {
                                        Text(
                                            requirement.reason,
                                            fontSize = 11.sp,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                                            modifier = Modifier.padding(bottom = 1.dp),
                                        )
                                    }
                                }
                            }
                        }

                        if (details.notes.isNotEmpty()) {
                            ModDetailSectionTitle("Notes")
                            details.notes.forEach { note -> ModDetailLine(note) }
                        }
                    }
                    SetupScrollArrows(scrollState)
                }
            }
        },
    )
}

@Composable
private fun MissionZipConstituentDialog(
    constituent: SectorgameMissionZip.Constituent,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text(constituent.name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            Column {
                DetailRow("Category", launcherFileTypeLabel(constituent.name))
                DetailRow("Type", describeExtension(constituent.name))
                DetailRow("Role", missionZipRoleLabel(constituent.role))
                DetailRow("Path", constituent.path)
                DetailRow("Size", setupSectionFormatSize(constituent.sizeBytes))
                if (constituent.compressedSizeBytes > 0) {
                    DetailRow("Compressed", setupSectionFormatSize(constituent.compressedSizeBytes))
                }
            }
        },
    )
}

private fun missionZipRoleLabel(role: String): String =
    when (role) {
        "mission_descriptor" -> "Mission descriptor"
        "mission_hog" -> "Mission assets"
        "mod_archive" -> "Bundled mod archive"
        "documentation" -> "Documentation"
        else -> "Other file"
    }

@Composable
private fun ModDetailSectionTitle(text: String) {
    Text(
        text,
        fontSize = 13.sp,
        fontWeight = FontWeight.SemiBold,
        color = MaterialTheme.colorScheme.primary,
        modifier = Modifier.padding(top = 10.dp, bottom = 2.dp),
    )
}

@Composable
private fun ModDetailLine(
    text: String,
    color: Color = MaterialTheme.colorScheme.onSurfaceVariant,
) {
    Text(
        "- $text",
        fontSize = 11.sp,
        color = color,
        modifier = Modifier.padding(bottom = 1.dp),
    )
}

private fun baseRequirementSha256Lines(requirement: ModManager.ModBaseRequirement): List<String> {
    val result = mutableListOf("Expected sha256=${requirement.expectedSha256}")
    if (!requirement.ok && requirement.actualSha256 != null) {
        result += "Actual sha256=${requirement.actualSha256}"
    }
    return result
}

@Composable
internal fun DemosSection(
    setDir: File,
    refreshTrigger: Int,
    onRefresh: () -> Unit,
) {
    val demosDir = File(setDir, "demos")
    var demoFiles by remember { mutableStateOf(emptyList<File>()) }
    var expanded by remember { mutableStateOf(false) }
    var deleteAllConfirm by remember { mutableStateOf(false) }
    var deleteSingleTarget by remember { mutableStateOf<File?>(null) }

    LaunchedEffect(refreshTrigger) {
        demoFiles =
            (
                demosDir.listFiles()?.filter {
                    it.isFile && it.name.lowercase().endsWith(".dem")
                } ?: emptyList()
            ).sortedBy { it.name.lowercase() }
    }

    if (demoFiles.isEmpty()) return

    val totalSize = demoFiles.sumOf { it.length() }
    val summary = "${demoFiles.size} demos, ${setupSectionFormatSize(totalSize)}"

    Spacer(modifier = Modifier.height(16.dp))

    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "Demos",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier.weight(1f),
        )
        Text(
            text = summary,
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.width(8.dp))
        TextButton(
            onClick = { expanded = !expanded },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide" else "Show",
                fontSize = 12.sp,
            )
        }
        TextButton(
            onClick = { deleteAllConfirm = true },
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
            modifier = Modifier.height(24.dp),
        ) {
            Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )

    if (expanded) {
        demoFiles.forEach { file ->
            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(start = 8.dp, bottom = 2.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = file.name,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.weight(1f),
                )
                Text(
                    text = setupSectionFormatSize(file.length()),
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                TextButton(
                    onClick = { deleteSingleTarget = file },
                    contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                    modifier = Modifier.height(24.dp),
                ) {
                    Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
                }
            }
        }
    }

    if (deleteAllConfirm) {
        AlertDialog(
            onDismissRequest = { deleteAllConfirm = false },
            title = { Text("Delete All Demos") },
            text = { Text("Remove all ${demoFiles.size} demo files? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    demoFiles.forEach { it.delete() }
                    deleteAllConfirm = false
                    onRefresh()
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteAllConfirm = false }) { Text("Cancel") }
            },
        )
    }

    deleteSingleTarget?.let { file ->
        AlertDialog(
            onDismissRequest = { deleteSingleTarget = null },
            title = { Text("Delete Demo") },
            text = { Text("Remove ${file.name}? This cannot be undone") },
            confirmButton = {
                TextButton(onClick = {
                    file.delete()
                    deleteSingleTarget = null
                    onRefresh()
                }) { Text("Delete") }
            },
            dismissButton = {
                TextButton(onClick = { deleteSingleTarget = null }) { Text("Cancel") }
            },
        )
    }
}

@Composable
private fun ModRow(
    mod: ModManager.ModInfo,
    isFirst: Boolean,
    isLast: Boolean,
    scanResult: DxaTextureScanner.ScanResult? = null,
    onToggle: (Boolean) -> Unit,
    onMoveUp: () -> Unit,
    onMoveDown: () -> Unit,
    onDetails: () -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(start = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(
            checked = mod.enabled,
            onCheckedChange = onToggle,
            modifier = Modifier.size(20.dp).tvFocusBorder(),
        )
        Spacer(modifier = Modifier.width(6.dp))
        Column(
            modifier =
                Modifier
                    .weight(1f)
                    .clickable(onClick = onDetails)
                    .padding(vertical = 2.dp),
        ) {
            Text(
                text = mod.displayName,
                fontSize = 12.sp,
                color =
                    if (mod.enabled) {
                        MaterialTheme.colorScheme.onSurface
                    } else {
                        MaterialTheme.colorScheme.onSurfaceVariant
                    },
            )
            Text(
                text = "${setupSectionFormatSize(mod.sizeBytes)} - ${mod.game.uppercase()}",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (scanResult != null && scanResult.oversizedCount > 0) {
                Text(
                    text =
                        "${scanResult.oversizedCount} of ${scanResult.textureCount} textures exceed " +
                            "${DxaTextureScanner.ENGINE_TEXTURE_CAP}px " +
                            "(max ${scanResult.maxWidth}x${scanResult.maxHeight}) -- will be skipped",
                    fontSize = 10.sp,
                    color = Color(0xFFF44336),
                )
            } else if (scanResult != null && scanResult.textureCount > 0) {
                Text(
                    text = "${scanResult.textureCount} textures, max ${scanResult.maxWidth}x${scanResult.maxHeight}",
                    fontSize = 10.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        IconButton(onClick = onDetails, modifier = Modifier.size(24.dp)) {
            Icon(Icons.Filled.Info, "Mod details", modifier = Modifier.size(15.dp))
        }
        if (!isFirst) {
            IconButton(onClick = onMoveUp, modifier = Modifier.size(24.dp)) {
                Icon(Icons.Filled.KeyboardArrowUp, "Move up", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        if (!isLast) {
            IconButton(onClick = onMoveDown, modifier = Modifier.size(24.dp)) {
                Icon(Icons.Filled.KeyboardArrowDown, "Move down", modifier = Modifier.size(16.dp))
            }
        } else {
            Spacer(modifier = Modifier.size(24.dp))
        }
        TextButton(
            onClick = onDelete,
            contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
            modifier = Modifier.height(24.dp),
        ) {
            Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
        }
    }
}

@Composable
private fun RecommendedModRow(
    rec: RecommendedMod,
    progress: Int?,
    onDownload: () -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(start = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(rec.displayName, fontSize = 12.sp)
            Text(
                "${rec.description} - ${rec.game.uppercase()}",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(modifier = Modifier.width(8.dp))
        when (progress) {
            null -> {
                Button(
                    onClick = onDownload,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp),
                ) { Text("Download", fontSize = 11.sp) }
            }

            in 0..100 -> {
                Text(
                    "$progress%",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.width(40.dp),
                )
            }

            -1 -> {
                Text("Error", fontSize = 12.sp, color = Color(0xFFF44336))
            }

            -2 -> {
                Text(
                    "\u2713",
                    fontSize = 14.sp,
                    color = Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
internal fun MusicInfoSection(
    filesDir: File,
    setDir: File,
    refreshTrigger: Int,
    hasMidiSource: Boolean = false,
    onEditMusic: () -> Unit = {},
) {
    val audioSrcManager = remember(refreshTrigger) { AudioSourceManager(filesDir) }
    var audioSources by remember(refreshTrigger) { mutableStateOf(audioSrcManager.getSources()) }
    val hasCdAudio = audioSources.isNotEmpty()
    var expanded by remember { mutableStateOf(false) }
    var detailStatus by remember { mutableStateOf<FileStatus?>(null) }

    val context = LocalContext.current
    val prefs = context.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
    val musicMode = prefs.getString("music_mode", "cd") ?: "cd"
    val modeLabel =
        when (musicMode) {
            "midi" -> "MIDI"
            "cd" -> "CD Audio"
            "files" -> "Audio Files"
            else -> "CD Audio"
        }

    val musicReady =
        when (musicMode) {
            "midi" -> hasMidiSource
            "cd" -> hasCdAudio
            "files" -> true
            else -> hasCdAudio
        }

    val musicLabel =
        when {
            musicReady -> "\u2713 Ready"
            musicMode == "cd" && hasMidiSource -> "\u2717 Missing, will use MIDI"
            else -> "\u2717 Missing"
        }

    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(top = 8.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "Music",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = modeLabel,
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = musicLabel,
            color = if (musicReady) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.width(4.dp))
        TextButton(
            onClick = onEditMusic,
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text("Edit", fontSize = 12.sp)
        }
        TextButton(
            onClick = { expanded = !expanded },
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
            modifier = Modifier.height(28.dp),
        ) {
            Text(
                text = if (expanded) "Hide" else "Files",
                fontSize = 12.sp,
            )
        }
    }
    HorizontalDivider(
        color = MaterialTheme.colorScheme.outlineVariant,
        modifier = Modifier.padding(bottom = 4.dp),
    )
    if (expanded) {
        Text(
            text =
                "MIDI audio is supported from game files. " +
                    "Redbook audio from CUE disc images with .bin/.img tracks is supported.",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 4.dp, end = 4.dp, bottom = 8.dp),
        )
        if (audioSources.isNotEmpty()) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(
                "Audio Sources:",
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(start = 4.dp, bottom = 4.dp),
            )
            audioSources.forEachIndexed { index, src ->
                Row(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .padding(start = 8.dp, bottom = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Checkbox(
                        checked = src.enabled,
                        onCheckedChange = { checked ->
                            audioSrcManager.setEnabled(src.id, checked)
                            audioSources = audioSrcManager.getSources()
                        },
                        modifier = Modifier.size(20.dp).tvFocusBorder(),
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
                            onClick = {
                                val ids = audioSources.map { it.id }.toMutableList()
                                ids[index] = ids[index - 1].also { ids[index - 1] = ids[index] }
                                audioSrcManager.reorder(ids)
                                audioSources = audioSrcManager.getSources()
                            },
                            modifier = Modifier.size(24.dp),
                        ) {
                            Icon(
                                Icons.Filled.KeyboardArrowUp,
                                "Move up",
                                modifier = Modifier.size(16.dp),
                            )
                        }
                    } else {
                        Spacer(modifier = Modifier.size(24.dp))
                    }
                    if (index < audioSources.size - 1) {
                        IconButton(
                            onClick = {
                                val ids = audioSources.map { it.id }.toMutableList()
                                ids[index] = ids[index + 1].also { ids[index + 1] = ids[index] }
                                audioSrcManager.reorder(ids)
                                audioSources = audioSrcManager.getSources()
                            },
                            modifier = Modifier.size(24.dp),
                        ) {
                            Icon(
                                Icons.Filled.KeyboardArrowDown,
                                "Move down",
                                modifier = Modifier.size(16.dp),
                            )
                        }
                    } else {
                        Spacer(modifier = Modifier.size(24.dp))
                    }
                    TextButton(
                        onClick = {
                            audioSrcManager.removeSource(src.id)
                            audioSources = audioSrcManager.getSources()
                        },
                        contentPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp),
                        modifier = Modifier.height(24.dp),
                    ) {
                        Text("\u2717", fontSize = 12.sp, color = Color(0xFFFF5252))
                    }
                }
            }
        }
    }
    detailStatus?.let { status ->
        FileDetailDialog(status = status, onDismiss = { detailStatus = null })
    }
}

private fun setupSectionFormatTimestamp(millis: Long): String {
    val sdf = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US)
    return sdf.format(Date(millis))
}

private fun setupSectionFormatSize(bytes: Long): String =
    when {
        bytes >= 1_073_741_824 -> "%.2f GB".format(bytes / 1_073_741_824.0)
        bytes >= 1_048_576 -> "%.1f MB".format(bytes / 1_048_576.0)
        bytes >= 1_024 -> "%.0f KB".format(bytes / 1_024.0)
        else -> "$bytes B"
    }

@Composable
internal fun FileDetailDialog(
    status: FileStatus,
    onDismiss: () -> Unit,
    onDelete: (() -> Unit)? = null,
) {
    val entry = status.manifestEntry
    val name = status.foundName ?: status.info.filename
    val description = descriptionForFile(name)
    val isMissing = !status.found && entry != null
    val isExternal = entry?.isExternal == true
    var confirmingDelete by remember { mutableStateOf(false) }
    var confirmingForget by remember { mutableStateOf(false) }
    val closeFocus = remember { FocusRequester() }
    val okFocus = remember { FocusRequester() }

    if (confirmingForget) {
        RequestLauncherControllerFocus(okFocus, true, "forget-$name")
        AlertDialog(
            onDismissRequest = { confirmingForget = false },
            title = { Text("Forget File") },
            text = { Text("Forget $name? This removes the launcher record. The original file is not deleted") },
            confirmButton = {
                TextButton(
                    onClick = {
                        confirmingForget = false
                        onDelete?.invoke()
                    },
                    modifier = Modifier.focusRequester(okFocus).tvFocusBorder(),
                ) { Text("OK") }
            },
            dismissButton = {
                TextButton(onClick = { confirmingForget = false }) { Text("Cancel") }
            },
        )
        return
    }

    RequestLauncherControllerFocus(closeFocus, true, name)

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(
                onClick = onDismiss,
                modifier = Modifier.focusRequester(closeFocus).tvFocusBorder(),
            ) { Text("Close") }
        },
        dismissButton =
            if (onDelete != null) {
                {
                    if (status.safUri != null) {
                        TextButton(onClick = onDelete, modifier = Modifier.tvFocusBorder()) {
                            Text("Unlink", color = MaterialTheme.colorScheme.error)
                        }
                    } else if (isExternal) {
                        TextButton(onClick = { confirmingForget = true }, modifier = Modifier.tvFocusBorder()) {
                            Text("Forget", color = MaterialTheme.colorScheme.error)
                        }
                    } else if (!confirmingDelete) {
                        TextButton(onClick = { confirmingDelete = true }, modifier = Modifier.tvFocusBorder()) {
                            Text("Delete from data folder?", color = MaterialTheme.colorScheme.error)
                        }
                    } else {
                        TextButton(onClick = onDelete, modifier = Modifier.tvFocusBorder()) {
                            Text("Are you sure? Delete", color = MaterialTheme.colorScheme.error)
                        }
                    }
                }
            } else {
                null
            },
        title = {
            Text(name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    DetailRow("Category", description)
                    DetailRow("Type", describeExtension(name))

                    val statusText =
                        when {
                            status.found -> "Found"
                            isMissing -> "Error: not found"
                            else -> "Missing"
                        }
                    DetailRow("Status", statusText)
                    if (status.info.required) {
                        DetailRow("Required", "Yes")
                    }

                    if (status.safUri != null) {
                        DetailRow("Location", "leave-in-place (linked)")
                        if (status.safSizeBytes > 0) {
                            DetailRow("Size", setupSectionFormatSize(status.safSizeBytes))
                        }
                    } else if (isExternal && entry.sourceUri != null) {
                        DetailRow("Location", entry.sourceUri)
                    } else if (entry != null) {
                        DetailRow("Location", "(in data folder)")
                    }

                    if (entry != null) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                        DetailRow("File on disk", entry.filename)
                        DetailRow("Size", setupSectionFormatSize(entry.sizeBytes))
                        DetailRow("Imported", setupSectionFormatTimestamp(entry.importedAt))

                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))

                        Text(
                            "SHA-256",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        SelectionContainer {
                            Text(
                                entry.sha256,
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurface,
                                modifier = Modifier.padding(bottom = 4.dp),
                            )
                        }

                        if (entry.versionName != null) {
                            DetailRow("Version match", entry.versionName)
                        } else {
                            DetailRow("Version match", "Unknown (#${entry.shortHash})")
                        }

                        if (isMissing) {
                            HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                            Text(
                                "This file was previously imported but is no longer on disk.",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.error,
                            )
                        }
                    } else if (!status.found) {
                        HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
                        if (status.info.alternatives.isNotEmpty()) {
                            DetailRow(
                                "Alternatives",
                                status.info.alternatives.joinToString(", "),
                            )
                        }
                        if (status.info.downloadUrl != null) {
                            DetailRow("Download", status.info.downloadUrl)
                        }
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

@Composable
internal fun DetailRow(
    label: String,
    value: String,
) {
    Row(modifier = Modifier.padding(vertical = 2.dp)) {
        Text(
            "$label: ",
            fontSize = 12.sp,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(
            value,
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }
}

@Composable
internal fun FileStatusRow(
    status: FileStatus,
    onClick: (() -> Unit)? = null,
) {
    val isMissing = !status.found && status.manifestEntry != null
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .let { if (onClick != null) it.clickable(onClick = onClick) else it }
                .padding(vertical = 1.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text =
                when {
                    status.found -> "\u2713"
                    isMissing -> "\u26A0"
                    else -> "\u2717"
                },
            color =
                when {
                    status.found -> Color(0xFF4CAF50)
                    isMissing -> Color(0xFFFF9800)
                    else -> Color(0xFFF44336)
                },
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp),
        )

        val name = status.foundName ?: status.info.filename
        val altHint =
            if (!status.found && !isMissing && status.info.alternatives.isNotEmpty()) {
                " (or ${status.info.alternatives.joinToString(", ")})"
            } else {
                ""
            }
        val versionHint =
            if (status.found && status.manifestEntry != null) {
                " [${status.manifestEntry.versionDisplay}]"
            } else {
                ""
            }
        val missingHint = if (isMissing) " [Error: not found]" else ""
        Text(
            text = "$name \u2014 ${status.info.description}$altHint$versionHint$missingHint",
            color =
                when {
                    status.found -> MaterialTheme.colorScheme.onSurface
                    isMissing -> Color(0xFFFF9800)
                    else -> MaterialTheme.colorScheme.onSurfaceVariant
                },
            fontSize = 13.sp,
            maxLines = 1,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
internal fun DownloadableFileRow(
    status: FileStatus,
    progress: Int?,
    onDownload: () -> Unit,
    onInfo: (() -> Unit)? = null,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "\u2717",
            color = Color(0xFFFFA726),
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.width(20.dp),
        )

        Text(
            text = "${status.info.filename} \u2014 ${status.info.description}",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 13.sp,
            maxLines = 1,
            modifier =
                Modifier
                    .weight(1f)
                    .then(if (onInfo != null) Modifier.clickable(onClick = onInfo) else Modifier),
        )

        Spacer(modifier = Modifier.width(8.dp))

        when (progress) {
            null -> {
                Button(
                    onClick = onDownload,
                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    modifier = Modifier.height(28.dp),
                ) {
                    Text("Download", fontSize = 11.sp)
                }
            }

            in 0..100 -> {
                Text(
                    text = "$progress%",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.width(40.dp),
                )
            }

            -1 -> {
                Text(
                    text = "Error",
                    fontSize = 12.sp,
                    color = Color(0xFFF44336),
                )
            }

            -2 -> {
                Text(
                    text = "\u2713",
                    fontSize = 14.sp,
                    color = Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
internal fun MissingFilesHelp() {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors =
            CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.errorContainer,
            ),
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(
                text = "Missing Required Files",
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onErrorContainer,
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text =
                    "Copy D2 files (from Steam/GOG) and/or D1 files to the app:\n" +
                        "  adb push <file> /data/data/com.dxxredux.app/files/\n" +
                        "Filenames are matched case-insensitively.\n" +
                        "Either Descent 2 or Descent 1 files are needed to launch.",
                color = MaterialTheme.colorScheme.onErrorContainer,
                fontSize = 12.sp,
                lineHeight = 16.sp,
            )
        }
    }
}
