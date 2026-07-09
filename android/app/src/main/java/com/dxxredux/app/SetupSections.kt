package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.nio.charset.Charset
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.floor
import kotlin.math.log10
import kotlin.math.pow

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
private fun BoxScope.SetupHorizontalScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            modifier = Modifier.align(Alignment.CenterStart).padding(start = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Filled.KeyboardArrowLeft,
                contentDescription = "Scroll left",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            modifier = Modifier.align(Alignment.CenterEnd).padding(end = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Filled.KeyboardArrowRight,
                contentDescription = "Scroll right",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

private data class MissionZipViewAction(
    val label: String,
    val onClick: () -> Unit,
)

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
    var detailProgress by remember { mutableStateOf<MetadataLoadProgress?>(null) }
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

    LaunchedEffect(detailTarget?.filename, setDir.absolutePath, refreshTrigger) {
        val target = detailTarget ?: return@LaunchedEffect
        detailInfo = null
        detailLoading = true
        detailProgress = MetadataLoadProgress("Reading mod metadata", 1, 3)
        detailInfo =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                modManager.getModDetails(target, setDir)
            }
        detailProgress = MetadataLoadProgress("Preparing metadata view", 2, 3)
        detailLoading = false
        detailProgress = null
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
            loadProgress = detailProgress,
            refreshTrigger = refreshTrigger,
            setDir = setDir,
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
    loadProgress: MetadataLoadProgress?,
    refreshTrigger: Int,
    setDir: File,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var textViewTarget by remember { mutableStateOf<MissionZip.Constituent?>(null) }
    var constituentTarget by remember { mutableStateOf<MissionZip.Constituent?>(null) }
    var levelMetadataTarget by remember { mutableStateOf<LevelMetadataTarget?>(null) }
    var musicCatalogTarget by remember { mutableStateOf<MissionZipMusicCatalog?>(null) }
    val topLevelMetadataTargets =
        remember(details?.archivePath, details?.missionZip, setDir.absolutePath, mod.displayName, mod.game) {
            details?.let {
                it.missionZip?.let { missionZip ->
                    LevelMetadataTargets.missionZipTargets(it.archivePath, setDir, missionZip)
                } ?: listOfNotNull(
                    LevelMetadataTargets.genericZip(
                        it.archivePath,
                        setDir,
                        mod.displayName,
                        mod.game,
                    ),
                )
            }
        }.orEmpty()
    levelMetadataTarget?.let { target ->
        LevelMetadataDialog(
            target = target,
            onDismiss = { levelMetadataTarget = null },
        )
    }
    textViewTarget?.let { constituent ->
        MissionZipTextDialog(
            constituent = constituent,
            archivePath = details?.archivePath,
            onDismiss = { textViewTarget = null },
        )
    }

    fun openExternalConstituent(constituent: MissionZip.Constituent) {
        val archivePath = details?.archivePath
        scope.launch {
            openMissionZipExternalDocument(context, archivePath, constituent)
        }
    }
    constituentTarget?.let { constituent ->
        MissionZipConstituentDialog(
            constituent = constituent,
            archivePath = details?.archivePath,
            setDir = setDir,
            refreshTrigger = refreshTrigger,
            viewAction =
                missionZipViewAction(
                    constituent = constituent,
                    onViewText = {
                        textViewTarget = constituent
                        constituentTarget = null
                    },
                    onViewExternal = {
                        constituentTarget = null
                        openExternalConstituent(constituent)
                    },
                ),
            onDismiss = { constituentTarget = null },
        )
    }
    musicCatalogTarget?.let { catalog ->
        MissionZipMusicDialog(
            catalog = catalog,
            onDismiss = { musicCatalogTarget = null },
        )
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    mod.displayName,
                    fontWeight = FontWeight.Bold,
                    fontSize = 16.sp,
                    modifier = Modifier.weight(1f),
                )
                details?.missionZip?.readme?.let { readme ->
                    TextButton(
                        onClick = {
                            if (MissionZip.isInlineReadmeCandidate(readme.name)) {
                                textViewTarget = readme
                            } else if (MissionZip.isExternalReadmeCandidate(readme.name)) {
                                openExternalConstituent(readme)
                            }
                        },
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                        modifier = Modifier.height(32.dp),
                    ) {
                        Text("View readme", fontSize = 12.sp)
                    }
                }
            }
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
                    MetadataLoadProgressView(
                        loadProgress ?: MetadataLoadProgress("Reading mod archive", 0, 1),
                    )
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

                        when (topLevelMetadataTargets.size) {
                            1 -> {
                                val target = topLevelMetadataTargets.single()
                                LevelMetadataButton(
                                    onClick = { levelMetadataTarget = target },
                                    modifier = Modifier.padding(bottom = 6.dp),
                                )
                            }

                            else -> {
                                if (topLevelMetadataTargets.isNotEmpty()) {
                                    ModDetailSectionTitle("Level metadata")
                                    topLevelMetadataTargets.forEach { target ->
                                        LevelMetadataButton(
                                            label = target.displayName,
                                            onClick = { levelMetadataTarget = target },
                                            modifier = Modifier.padding(bottom = 6.dp),
                                        )
                                    }
                                }
                            }
                        }

                        details.missionZipMusic?.let { catalog ->
                            val trackCount = catalog.sources.sumOf { it.tracks.size }
                            OutlinedButton(
                                onClick = { musicCatalogTarget = catalog },
                                shape = MaterialTheme.shapes.small,
                                contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .padding(bottom = 6.dp),
                            ) {
                                Column(modifier = Modifier.fillMaxWidth()) {
                                    Text(
                                        "Music tracks",
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = MaterialTheme.colorScheme.onSurface,
                                    )
                                    Text(
                                        "$trackCount entries across ${catalog.sources.size} source(s)",
                                        fontSize = 11.sp,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            }
                        }

                        details.missionZip?.let { missionZip ->
                            DetailRow("Category", missionZip.category.replaceFirstChar { it.uppercase() })
                            DetailRow("Import mode", missionZip.importMode.replace('_', ' '))
                            details.missionZipExtraction?.let { extraction ->
                                DetailRow("Extracted", "Yes")
                                DetailRow("Original archive", extraction.sourceArchiveName)
                                DetailRow("Archive format", extraction.archiveFormat.uppercase(Locale.US))
                                DetailRow("Extracted files", extraction.fileCount.toString())
                                DetailRow("Extracted size", setupSectionFormatSize(extraction.extractedSizeBytes))
                                DetailRow("Extracted path", extraction.rootPath)
                            }
                            ModDetailSectionTitle("Mission")
                            DetailRow("Title", missionZip.mission.displayName)
                            missionZip.mission.type?.let { DetailRow("Type", it) }
                            missionZip.mission.author?.let { DetailRow("Author", it) }
                            missionZip.mission.editor?.let { DetailRow("Editor", it) }
                            DetailRow(
                                "Levels",
                                missionZip.mission.levelNames.size
                                    .toString(),
                            )
                            if (missionZip.mission.levelNames.isNotEmpty()) {
                                ModDetailLine(missionZip.mission.levelNames.joinToString(", "))
                            }
                            if (missionZip.mission.secretLevelNames.isNotEmpty()) {
                                DetailRow(
                                    "Secret levels",
                                    missionZip.mission.secretLevelNames.size
                                        .toString(),
                                )
                                ModDetailLine(missionZip.mission.secretLevelNames.joinToString(", "))
                            }
                            if (missionZip.mission.assetReferences.isNotEmpty()) {
                                ModDetailSectionTitle("Referenced assets")
                                missionZip.mission.assetReferences.forEach { (label, value) ->
                                    ModDetailLine("$label: $value")
                                }
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
                            val fileListScrollState = rememberScrollState()
                            Column(
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .heightIn(max = 260.dp)
                                        .verticalScroll(fileListScrollState),
                            ) {
                                details.missionZip.constituents.forEach { constituent ->
                                    OutlinedButton(
                                        onClick = { constituentTarget = constituent },
                                        shape = MaterialTheme.shapes.small,
                                        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline),
                                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                                        modifier =
                                            Modifier
                                                .fillMaxWidth()
                                                .padding(bottom = 4.dp),
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

private const val MISSION_ZIP_LOCAL_MATCH_TASK_ID = "local-match"
private const val MISSION_ZIP_BULK_LOOKUP_TASK_ID = "bulk-lookup"

internal data class MissionZipMusicAnalysisProgress(
    val label: String,
    val completed: Int,
    val total: Int,
    val resultCount: Int? = null,
) {
    val fraction: Float?
        get() = total.takeIf { it > 0 }?.let { (completed.toFloat() / it.toFloat()).coerceIn(0f, 1f) }
}

@Composable
private fun MissionZipMusicDialog(
    catalog: MissionZipMusicCatalog,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val stageManager = remember(context.cacheDir) { MissionZipMusicStageManager(context.cacheDir) }
    val fingerprintCache = remember(context.filesDir) { MissionZipAudioFingerprintCache(context.filesDir) }
    val prefs =
        remember(context) {
            context.getSharedPreferences("dxx_prefs", android.content.Context.MODE_PRIVATE)
        }
    val allowAcoustIdLookups =
        remember(context) {
            prefs.getBoolean(PREF_ALLOW_ACOUSTID_WEB_LOOKUPS, false)
        }
    val acoustIdAvailable =
        remember(context, allowAcoustIdLookups) {
            allowAcoustIdLookups && AcoustIdClient.configure(context)
        }
    var cachedFingerprints by remember(catalog.archivePath) {
        mutableStateOf(fingerprintCache.cachedEntries(catalog))
    }
    var previewTarget by remember { mutableStateOf<Pair<MissionZipMusicTrack, File>?>(null) }
    var midiPreviewTarget by remember { mutableStateOf<MissionZipMusicTrack?>(null) }
    var stagingTrackId by remember { mutableStateOf<String?>(null) }
    var stagingProblem by remember { mutableStateOf<String?>(null) }
    var musicProgress by remember { mutableStateOf<MissionZipMusicAnalysisProgress?>(null) }
    var storageFailureMessage by remember { mutableStateOf<String?>(null) }
    val fingerprintableTracks =
        remember(catalog) {
            catalog.sources
                .flatMap { it.tracks }
                .filter { MissionZipAudioFingerprintCache.isFingerprintSupported(it) }
        }
    val busy = stagingTrackId != null

    suspend fun identifyTrack(track: MissionZipMusicTrack): MissionZipAudioFingerprintCache.Entry? =
        withContext(kotlinx.coroutines.Dispatchers.IO) {
            try {
                stageManager.cleanupOldFiles()
                val staged = stageManager.stageCompressedAudioTrack(catalog, track) ?: return@withContext null
                fingerprintCache.identifyLocal(context, catalog, track, staged)
            } catch (e: InsufficientStorageException) {
                withContext(kotlinx.coroutines.Dispatchers.Main) {
                    storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                    stagingProblem = "Not enough free space"
                    musicProgress = null
                }
                null
            } catch (_: Exception) {
                null
            }
        }

    suspend fun lookupTrack(track: MissionZipMusicTrack): MissionZipAudioFingerprintCache.Entry? =
        withContext(kotlinx.coroutines.Dispatchers.IO) {
            if (!acoustIdAvailable) return@withContext null
            val cached = identifyTrack(track) ?: return@withContext null
            try {
                if (cached.hasAcoustIdLookup) {
                    cached
                } else {
                    val webName =
                        AcoustIdClient.lookupFingerprint(
                            cached.chromaprint,
                            maxOf(1, cached.durationMs / 1000),
                        )
                    fingerprintCache.recordAcoustIdResult(
                        cached,
                        webName,
                        if (webName.isNullOrBlank()) {
                            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_NO_MATCH
                        } else {
                            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK
                        },
                    )
                }
            } catch (_: Exception) {
                fingerprintCache.recordAcoustIdResult(
                    cached,
                    null,
                    MissionZipAudioFingerprintCache.ACOUSTID_STATUS_FAILED,
                )
            }
        }

    LaunchedEffect(catalog.archivePath, fingerprintableTracks.joinToString("|") { it.id }) {
        val pendingTracks = missionZipMusicTracksNeedingLocalAnalysis(fingerprintableTracks, cachedFingerprints)
        if (pendingTracks.isEmpty()) return@LaunchedEffect
        stagingProblem = null
        musicProgress =
            MissionZipMusicAnalysisProgress(
                label = "Generating chromaprints",
                completed = 0,
                total = pendingTracks.size,
            )
        stagingTrackId = MISSION_ZIP_LOCAL_MATCH_TASK_ID
        var completed = 0
        var matched = 0
        val updates = mutableMapOf<String, MissionZipAudioFingerprintCache.Entry>()
        for (track in pendingTracks) {
            val result = identifyTrack(track)
            completed++
            if (result != null) {
                updates[track.id] = result
                if (result.hasLocalMatch) matched++
            }
            cachedFingerprints = cachedFingerprints + updates
            musicProgress =
                MissionZipMusicAnalysisProgress(
                    label = "Generating chromaprints",
                    completed = completed,
                    total = pendingTracks.size,
                )
        }
        stagingTrackId = null
        cachedFingerprints = cachedFingerprints + updates
        musicProgress =
            MissionZipMusicAnalysisProgress(
                label = "Bundled database matches",
                completed = pendingTracks.size,
                total = pendingTracks.size,
                resultCount = matched,
            )
    }

    previewTarget?.let { (track, file) ->
        val matchLine = cachedFingerprints[track.id]?.let { missionZipMusicFingerprintLine(it) }
        AudioFilePreviewDialog(
            title = "Track Preview",
            audioFile = file,
            lines =
                buildList {
                    add(AudioFilePreviewLine("File: ${track.displayName}"))
                    add(AudioFilePreviewLine("Source: ${track.archiveEntryPath}"))
                    if (matchLine != null) add(AudioFilePreviewLine(matchLine, primary = true))
                    add(AudioFilePreviewLine("Staged: ${file.absolutePath}", small = true))
                },
            onDismiss = { previewTarget = null },
        )
    }
    midiPreviewTarget?.let { track ->
        MidiBytesPreviewDialog(
            title = "MIDI Preview",
            trackName = track.displayName,
            detailLines =
                listOf(
                    "Source: ${track.archiveEntryPath}",
                    missionZipMusicTrackSubtitle(track),
                ),
            isHmp = track.extension == "hmp" || track.extension == "hmq",
            loadBytes = { stageManager.readMidiTrackBytes(catalog, track) },
            onDismiss = { midiPreviewTarget = null },
        )
    }
    storageFailureMessage?.let { message ->
        StorageFailureDialog(message = message, onDismiss = { storageFailureMessage = null })
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text("Music tracks", fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Column(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp)
                        .verticalScroll(scrollState),
            ) {
                Text(
                    catalog.archivePath,
                    fontSize = 11.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(bottom = 6.dp),
                )
                stagingProblem?.let { problem ->
                    Text(
                        problem,
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.error,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                }
                musicProgress?.let { progress ->
                    val fraction = progress.fraction
                    Text(
                        formatMissionZipMusicProgress(progress),
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                    if (fraction != null) {
                        LinearProgressIndicator(
                            progress = { fraction },
                            modifier = Modifier.padding(bottom = 6.dp).fillMaxWidth().height(4.dp),
                        )
                    } else {
                        LinearProgressIndicator(modifier = Modifier.padding(bottom = 6.dp).fillMaxWidth().height(4.dp))
                    }
                }
                if (acoustIdAvailable && fingerprintableTracks.isNotEmpty()) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        modifier = Modifier.fillMaxWidth().padding(bottom = 6.dp),
                    ) {
                        OutlinedButton(
                            onClick = {
                                stagingProblem = null
                                musicProgress =
                                    MissionZipMusicAnalysisProgress(
                                        label = "Looking up AcoustID matches",
                                        completed = 0,
                                        total = fingerprintableTracks.size,
                                    )
                                stagingTrackId = MISSION_ZIP_BULK_LOOKUP_TASK_ID
                                scope.launch {
                                    var completed = 0
                                    var webMatches = 0
                                    val updates = mutableMapOf<String, MissionZipAudioFingerprintCache.Entry>()
                                    for (track in fingerprintableTracks) {
                                        val result = lookupTrack(track)
                                        completed++
                                        if (result != null) {
                                            updates[track.id] = result
                                            if (result.acoustIdLookupStatus ==
                                                MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK
                                            ) {
                                                webMatches++
                                            }
                                        }
                                        cachedFingerprints = cachedFingerprints + updates
                                        musicProgress =
                                            MissionZipMusicAnalysisProgress(
                                                label = "Looking up AcoustID matches",
                                                completed = completed,
                                                total = fingerprintableTracks.size,
                                            )
                                    }
                                    stagingTrackId = null
                                    cachedFingerprints = cachedFingerprints + updates
                                    musicProgress =
                                        MissionZipMusicAnalysisProgress(
                                            label = "AcoustID web matches",
                                            completed = completed,
                                            total = fingerprintableTracks.size,
                                            resultCount = webMatches,
                                        )
                                }
                            },
                            enabled = !busy,
                            shape = MaterialTheme.shapes.small,
                            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp),
                            modifier = Modifier.fillMaxWidth().height(32.dp).tvFocusBorder(),
                        ) {
                            Text("Lookup all", fontSize = 11.sp)
                        }
                    }
                }
                catalog.sources.forEach { source ->
                    ModDetailSectionTitle(source.label)
                    if (source.containerPath.isNotBlank()) {
                        ModDetailLine(source.containerPath)
                    }
                    source.tracks.forEach { track ->
                        val cachedFingerprint = cachedFingerprints[track.id]
                        val decodedName = cachedFingerprint?.let(::missionZipMusicDecodedName)
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .padding(top = 3.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            val fingerprintAnalysisActive =
                                stagingTrackId == MISSION_ZIP_LOCAL_MATCH_TASK_ID &&
                                    MissionZipAudioFingerprintCache.isFingerprintSupported(track) &&
                                    cachedFingerprint == null
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    track.displayName,
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.onSurface,
                                )
                                if (!decodedName.isNullOrBlank()) {
                                    Text(
                                        "Decoded: $decodedName",
                                        fontSize = 11.sp,
                                        color = MaterialTheme.colorScheme.primary,
                                    )
                                }
                            }
                            if (track.playable &&
                                (
                                    track.kind == MissionZipMusic.KIND_COMPRESSED_AUDIO ||
                                        track.kind == MissionZipMusic.KIND_MIDI
                                )
                            ) {
                                TextButton(
                                    onClick = {
                                        stagingProblem = null
                                        if (track.kind == MissionZipMusic.KIND_MIDI) {
                                            midiPreviewTarget = track
                                        } else {
                                            stagingTrackId = track.id
                                            scope.launch {
                                                val staged =
                                                    try {
                                                        withContext(kotlinx.coroutines.Dispatchers.IO) {
                                                            stageManager.cleanupOldFiles()
                                                            stageManager.stageCompressedAudioTrack(catalog, track)
                                                        }
                                                    } catch (e: InsufficientStorageException) {
                                                        storageFailureMessage = ImportStorageGuard.messageForFailure(e)
                                                        null
                                                    }
                                                stagingTrackId = null
                                                if (staged == null) {
                                                    stagingProblem = "Could not stage ${track.displayName} for preview"
                                                } else {
                                                    previewTarget = track to staged
                                                }
                                            }
                                        }
                                    },
                                    enabled = stagingTrackId == null,
                                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                                    modifier = Modifier.height(28.dp).tvFocusBorder(),
                                ) {
                                    Text(
                                        when {
                                            stagingTrackId == track.id -> "Staging"
                                            fingerprintAnalysisActive -> "Analyzing"
                                            else -> "Preview"
                                        },
                                        fontSize = 11.sp,
                                    )
                                }
                                if (MissionZipAudioFingerprintCache.isFingerprintSupported(track)) {
                                    if (acoustIdAvailable) {
                                        TextButton(
                                            onClick = {
                                                stagingProblem = null
                                                stagingTrackId = track.id
                                                scope.launch {
                                                    val result = lookupTrack(track)
                                                    stagingTrackId = null
                                                    if (result == null) {
                                                        stagingProblem = "Could not look up ${track.displayName}"
                                                    } else {
                                                        cachedFingerprints = cachedFingerprints + (track.id to result)
                                                    }
                                                }
                                            },
                                            enabled = stagingTrackId == null,
                                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                                            modifier = Modifier.height(28.dp).tvFocusBorder(),
                                        ) {
                                            Text(
                                                if (stagingTrackId == track.id) "Working" else "Lookup",
                                                fontSize = 11.sp,
                                            )
                                        }
                                    }
                                }
                            }
                        }
                        ModDetailLine(missionZipMusicTrackSubtitle(track))
                        if (cachedFingerprint != null) {
                            ModDetailLine(missionZipMusicFingerprintLine(cachedFingerprint))
                        }
                    }
                }
            }
        },
    )
}

private fun missionZipMusicTrackSubtitle(track: MissionZipMusicTrack): String =
    buildList {
        add(
            when (track.kind) {
                MissionZipMusic.KIND_SONG_REFERENCE -> "Song list reference"
                MissionZipMusic.KIND_MIDI -> "MIDI/HMP track"
                MissionZipMusic.KIND_COMPRESSED_AUDIO -> "Audio track"
                else -> track.kind.replace('_', ' ')
            },
        )
        if (track.extension.isNotBlank()) add(track.extension.uppercase(Locale.US))
        if (track.sizeBytes > 0) add(setupSectionFormatSize(track.sizeBytes))
        if (track.hogEntryName != null) add("inside ${track.archiveEntryPath}")
        track.nestedEntryPath?.let { add(it) }
    }.joinToString(" - ")

internal fun missionZipMusicTracksNeedingLocalAnalysis(
    tracks: List<MissionZipMusicTrack>,
    cachedFingerprints: Map<String, MissionZipAudioFingerprintCache.Entry>,
): List<MissionZipMusicTrack> = tracks.filter { it.id !in cachedFingerprints }

internal fun missionZipMusicDecodedName(entry: MissionZipAudioFingerprintCache.Entry): String? =
    entry.localMatchName
        ?.trim()
        ?.takeIf { it.isNotBlank() && it != "[unknown] - [untitled]" }
        ?: entry.acoustIdName
            ?.trim()
            ?.takeIf { it.isNotBlank() && it != "[unknown] - [untitled]" }

private fun missionZipMusicFingerprintLine(entry: MissionZipAudioFingerprintCache.Entry): String =
    buildString {
        if (entry.localMatchName.isNullOrBlank()) {
            append("Fingerprint cached; not in bundled database")
        } else {
            append("Matched: ${entry.localMatchName}")
            entry.localMatchTrack?.let { append(" (Track $it)") }
            entry.localMatchConfidence?.let { append(" [${(it * 100).toInt()}%]") }
        }
        if (entry.durationMs > 0) {
            append(" - ${formatMissionZipMusicDuration(entry.durationMs)}")
        }
        when (entry.acoustIdLookupStatus) {
            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK -> {
                val name = entry.acoustIdName.orEmpty().ifBlank { "matched" }
                append(" - AcoustID: $name")
            }

            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_NO_MATCH -> {
                append(" - AcoustID: no web match")
            }

            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_FAILED -> {
                append(" - AcoustID: lookup failed")
            }
        }
    }

internal fun formatMissionZipMusicProgress(progress: MissionZipMusicAnalysisProgress): String {
    val base = "${progress.label} ${progress.completed.coerceAtLeast(0)}/${progress.total.coerceAtLeast(0)}"
    val resultCount = progress.resultCount ?: return base
    return "$base, $resultCount matched"
}

private fun formatMissionZipMusicDuration(durationMs: Int): String {
    val totalSeconds = ((durationMs + 500L) / 1000L).coerceAtLeast(0L)
    val minutes = totalSeconds / 60L
    val seconds = totalSeconds % 60L
    return if (minutes > 0L) "${minutes}m${seconds}s" else "${seconds}s"
}

private data class MetadataDetailLoadResult(
    val metadata: GameFileMetadata.Summary?,
    val levelMetadataTarget: LevelMetadataTarget?,
)

@Composable
private fun MissionZipConstituentDialog(
    constituent: MissionZip.Constituent,
    archivePath: String?,
    setDir: File,
    refreshTrigger: Int = 0,
    viewAction: MissionZipViewAction?,
    onDismiss: () -> Unit,
) {
    var levelMetadataTarget by remember { mutableStateOf<LevelMetadataTarget?>(null) }
    var metadataResult by remember(archivePath, constituent.path, setDir.absolutePath, refreshTrigger) {
        mutableStateOf<MetadataDetailLoadResult?>(null)
    }
    var metadataProgress by remember(archivePath, constituent.path, refreshTrigger) {
        mutableStateOf(MetadataLoadProgress("Locating metadata source", 0, 4))
    }

    LaunchedEffect(archivePath, constituent.path, setDir.absolutePath, refreshTrigger) {
        val archive = archivePath
        if (archive == null) {
            metadataResult = MetadataDetailLoadResult(null, null)
            return@LaunchedEffect
        }
        metadataResult = null
        metadataProgress = MetadataLoadProgress("Locating metadata source", 0, 4)
        val extractedEntry =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                missionZipExtractedStoreForArchivePath(archive)
                    ?.extractedEntryForArchiveEntry(archive, constituent.path)
            }
        metadataProgress =
            MetadataLoadProgress(
                if (extractedEntry != null) {
                    "Reading extracted file metadata"
                } else {
                    "Reading archive entry metadata"
                },
                1,
                4,
            )
        val metadata =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                runCatching {
                    extractedEntry
                        ?.let { GameFileMetadata.summarizeLocalFile(it.file) }
                        ?: GameFileMetadata.summarizeZipConstituent(File(archive), constituent.path, constituent.name)
                }.getOrNull()
            }
        metadataProgress = MetadataLoadProgress("Preparing level metadata entry", 2, 4)
        val target =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                runCatching { LevelMetadataTargets.zipConstituent(archive, setDir, constituent, metadata) }.getOrNull()
            }
        metadataProgress = MetadataLoadProgress("Preparing metadata view", 3, 4)
        metadataResult = MetadataDetailLoadResult(metadata, target)
        metadataProgress = MetadataLoadProgress("Metadata ready", 4, 4)
    }

    levelMetadataTarget?.let { target ->
        LevelMetadataDialog(
            target = target,
            onDismiss = { levelMetadataTarget = null },
        )
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        dismissButton =
            viewAction?.let { action ->
                {
                    TextButton(onClick = action.onClick) { Text(action.label) }
                }
            },
        title = {
            Text(constituent.name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp),
            ) {
                Column(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .verticalScroll(scrollState)
                            .padding(end = 8.dp, bottom = 4.dp),
                ) {
                    DetailRow("Category", launcherFileTypeLabel(constituent.name))
                    DetailRow("Type", describeExtension(constituent.name))
                    DetailRow("Role", missionZipRoleLabel(constituent.role))
                    DetailRow("Path", constituent.path)
                    DetailRow("Size", setupSectionFormatSize(constituent.sizeBytes))
                    if (constituent.compressedSizeBytes > 0) {
                        DetailRow("Compressed", setupSectionFormatSize(constituent.compressedSizeBytes))
                    }
                    val loadedMetadata = metadataResult
                    if (loadedMetadata == null) {
                        MetadataLoadProgressView(
                            metadataProgress,
                            modifier = Modifier.padding(top = 4.dp, bottom = 6.dp),
                        )
                    } else {
                        loadedMetadata.levelMetadataTarget?.let { target ->
                            LevelMetadataButton(
                                onClick = { levelMetadataTarget = target },
                                modifier = Modifier.padding(top = 4.dp, bottom = 6.dp),
                            )
                        }
                        FileMetadataDetails(loadedMetadata.metadata)
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

@Composable
private fun MissionZipTextDialog(
    constituent: MissionZip.Constituent,
    archivePath: String?,
    onDismiss: () -> Unit,
) {
    val content =
        remember(archivePath, constituent.path) {
            readMissionZipTextFile(archivePath, constituent)
        }
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text(constituent.name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            val scrollState = rememberScrollState()
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp),
            ) {
                SelectionContainer {
                    Column(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .verticalScroll(scrollState)
                                .padding(end = 8.dp),
                    ) {
                        content.problem?.let { problem ->
                            Text(
                                problem,
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.error,
                                modifier = Modifier.padding(bottom = 6.dp),
                            )
                        }
                        if (content.truncated) {
                            Text(
                                "Showing first ${setupSectionFormatSize(1024L * 1024L)}",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.padding(bottom = 6.dp),
                            )
                        }
                        Text(
                            content.text.ifEmpty { "(empty text file)" },
                            fontSize = 12.sp,
                            fontFamily = FontFamily.Monospace,
                            color = MaterialTheme.colorScheme.onSurface,
                        )
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

private fun readMissionZipTextFile(
    archivePath: String?,
    constituent: MissionZip.Constituent,
): MissionZip.TextFileContent {
    if (archivePath ==
        null
    ) {
        return MissionZip.TextFileContent("", truncated = false, problem = "Mission ZIP is missing")
    }
    missionZipExtractedStoreForArchivePath(archivePath)
        ?.extractedEntryForArchiveEntry(archivePath, constituent.path)
        ?.let { return readExtractedMissionZipTextFile(it.file, constituent.path) }
    return MissionZip.readTextFile(File(archivePath), constituent.path)
}

private fun readExtractedMissionZipTextFile(
    file: File,
    path: String,
    maxBytes: Long = 1024L * 1024L,
): MissionZip.TextFileContent {
    if (!MissionZip.isInlineReadmeCandidate(path)) {
        return MissionZip.TextFileContent("", truncated = false, problem = "Only .txt files can be viewed")
    }
    if (!file.isFile) return MissionZip.TextFileContent("", truncated = false, problem = "Text file is missing")
    return try {
        val limit = maxBytes.coerceAtLeast(1L).coerceAtMost((Int.MAX_VALUE - 1).toLong()).toInt()
        val bytes =
            file.inputStream().use { input ->
                val buffer = ByteArray(limit + 1)
                var total = 0
                while (total < buffer.size) {
                    val read = input.read(buffer, total, buffer.size - total)
                    if (read <= 0) break
                    total += read
                }
                buffer.copyOf(total)
            }
        val truncated = bytes.size > limit
        val keptBytes = bytes.copyOf(minOf(bytes.size, limit))
        val utf8 = keptBytes.toString(Charsets.UTF_8)
        val text = if ('\uFFFD' in utf8) keptBytes.toString(Charset.forName("windows-1252")) else utf8
        MissionZip.TextFileContent(text, truncated)
    } catch (e: Exception) {
        MissionZip.TextFileContent("", truncated = false, problem = e.message ?: e.javaClass.simpleName)
    }
}

private fun missionZipViewAction(
    constituent: MissionZip.Constituent,
    onViewText: () -> Unit,
    onViewExternal: () -> Unit,
): MissionZipViewAction? =
    when {
        MissionZip.isInlineReadmeCandidate(constituent.name) -> {
            MissionZipViewAction("View", onViewText)
        }

        MissionZip.isExternalReadmeCandidate(constituent.name) -> {
            MissionZipViewAction("View external", onViewExternal)
        }

        else -> {
            null
        }
    }

private const val FILE_PROVIDER_AUTHORITY = "com.dxxredux.app.fileprovider"
private const val FILE_VIEW_CACHE_MAX_AGE_MS = 24L * 60L * 60L * 1000L
private const val FILE_VIEW_CACHE_MAX_BYTES = 64L * 1024L * 1024L

private suspend fun openMissionZipExternalDocument(
    context: Context,
    archivePath: String?,
    constituent: MissionZip.Constituent,
) {
    if (archivePath == null) {
        Toast.makeText(context, "Mission ZIP is missing", Toast.LENGTH_SHORT).show()
        return
    }
    try {
        val uri =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                extractMissionZipConstituentToCache(context, File(archivePath), constituent)
            }
        openExternalFile(context, uri, MissionZip.externalViewMimeType(constituent.name), constituent.name)
    } catch (e: Exception) {
        Toast.makeText(context, "Could not open ${constituent.name}: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}

private fun extractMissionZipConstituentToCache(
    context: Context,
    archive: File,
    constituent: MissionZip.Constituent,
): Uri {
    val viewDir = File(context.cacheDir, "file_view")
    viewDir.mkdirs()
    cleanupFileViewCache(viewDir)

    val copy = File(viewDir, missionZipCacheFilename(archive, constituent))
    copy.delete()
    val extracted =
        missionZipExtractedStoreForArchivePath(archive.absolutePath)
            ?.extractedEntryForArchiveEntry(archive.absolutePath, constituent.path)
            ?.file
    if (extracted != null) {
        extracted.copyTo(copy, overwrite = true)
    } else {
        ArchiveFiles.open(archive).use { archiveFile ->
            val entry =
                archiveFile.findEntry(constituent.path) ?: throw IllegalArgumentException("Document file is missing")
            if (entry.isDirectory) throw IllegalArgumentException("Document path is a directory")
            archiveFile.openInputStream(entry).use { input ->
                FileOutputStream(copy).use { output ->
                    input.copyTo(output)
                }
            }
        }
    }
    copy.setLastModified(System.currentTimeMillis())
    cleanupFileViewCache(viewDir, keepFile = copy)
    return androidx.core.content.FileProvider
        .getUriForFile(context, FILE_PROVIDER_AUTHORITY, copy)
}

private fun cleanupFileViewCache(
    viewDir: File,
    keepFile: File? = null,
) {
    val files = viewDir.listFiles()?.filter { it.isFile } ?: return
    val keepPath = keepFile?.absolutePath
    val cutoff = System.currentTimeMillis() - FILE_VIEW_CACHE_MAX_AGE_MS
    files
        .filter { it.absolutePath != keepPath && it.lastModified() < cutoff }
        .forEach { it.delete() }

    var remaining =
        viewDir
            .listFiles()
            ?.filter { it.isFile }
            .orEmpty()
    var totalBytes = remaining.sumOf { it.length().coerceAtLeast(0L) }
    if (totalBytes <= FILE_VIEW_CACHE_MAX_BYTES) return
    for (file in remaining.sortedBy { it.lastModified() }) {
        if (file.absolutePath == keepPath) continue
        val size = file.length().coerceAtLeast(0L)
        if (file.delete()) totalBytes -= size
        if (totalBytes <= FILE_VIEW_CACHE_MAX_BYTES) break
    }
}

private fun missionZipCacheFilename(
    archive: File,
    constituent: MissionZip.Constituent,
): String {
    val key = "${archive.absolutePath}:${constituent.path}"
    val prefix = Integer.toHexString(key.hashCode())
    val leaf = GameFileFormats.leafName(constituent.name).ifBlank { "document" }
    val safeLeaf = leaf.replace(Regex("[^A-Za-z0-9._-]"), "_").ifBlank { "document" }
    return "$prefix-$safeLeaf"
}

private fun openExternalFile(
    context: Context,
    uri: Uri,
    mimeType: String,
    displayName: String,
) {
    try {
        val intent =
            Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(uri, mimeType)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            }
        context.startActivity(intent)
    } catch (_: Exception) {
        Toast.makeText(context, "No viewer available for $displayName", Toast.LENGTH_SHORT).show()
    }
}

private fun missionZipRoleLabel(role: String): String = GameFileFormats.missionZipRoleLabel(role)

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
        FileDetailDialog(
            status = status,
            setDir = setDir,
            refreshTrigger = refreshTrigger,
            onDismiss = { detailStatus = null },
        )
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

internal fun missionDescriptorForStatus(
    status: FileStatus,
    setDir: File,
): GameFileFormats.MissionDescriptor? {
    val name = status.foundName ?: status.manifestEntry?.filename ?: status.info.filename
    if (!GameFileFormats.isMissionDescriptor(name)) return null
    val localName = status.foundName ?: status.manifestEntry?.filename ?: return null
    val actualName = findFile(setDir, localName) ?: localName
    val file = File(setDir, actualName)
    if (!file.isFile) return null
    return runCatching {
        GameFileFormats.parseMissionDescriptor(name, file.readText())
    }.getOrNull()
}

internal fun gameFileMetadataForStatus(
    status: FileStatus,
    setDir: File,
): GameFileMetadata.Summary? {
    val name = status.foundName ?: status.manifestEntry?.filename ?: status.info.filename
    if (!GameFileFormats.isMetadataInspectable(name)) return null
    val localName = status.foundName ?: status.manifestEntry?.filename ?: return null
    val actualName = findFile(setDir, localName) ?: localName
    val file = File(setDir, actualName)
    if (!file.isFile) return null
    return runCatching {
        GameFileMetadata.summarizeLocalFile(file)
    }.getOrNull()
}

@Composable
internal fun FileDetailDialog(
    status: FileStatus,
    setDir: File,
    refreshTrigger: Int = 0,
    onDismiss: () -> Unit,
    onDelete: (() -> Unit)? = null,
) {
    val entry = status.manifestEntry
    val name = status.foundName ?: status.info.filename
    val description = descriptionForFile(name)
    val missionDescriptor =
        remember(name, setDir.absolutePath, status.found, status.manifestEntry?.filename, refreshTrigger) {
            missionDescriptorForStatus(status, setDir)
        }
    val metadataSourceFile =
        remember(name, setDir.absolutePath, status.found, status.manifestEntry?.filename, refreshTrigger) {
            val localName = status.foundName ?: status.manifestEntry?.filename
            val actualName = localName?.let { findFile(setDir, it) ?: it }
            val file = actualName?.let { File(setDir, it) }
            file?.takeIf {
                it.isFile &&
                    (GameFileFormats.isMetadataInspectable(it.name) || LevelMetadataTargets.canAnalyzeFile(it.name))
            }
        }
    var fileMetadataResult by remember(metadataSourceFile?.absolutePath, refreshTrigger) {
        mutableStateOf(
            if (metadataSourceFile == null) {
                MetadataDetailLoadResult(null, null)
            } else {
                null
            },
        )
    }
    var fileMetadataProgress by remember(metadataSourceFile?.absolutePath, refreshTrigger) {
        mutableStateOf(MetadataLoadProgress("Locating file metadata", 0, 3))
    }
    LaunchedEffect(metadataSourceFile?.absolutePath, setDir.absolutePath, refreshTrigger) {
        val file = metadataSourceFile
        if (file == null) {
            fileMetadataResult = MetadataDetailLoadResult(null, null)
            return@LaunchedEffect
        }
        fileMetadataResult = null
        fileMetadataProgress = MetadataLoadProgress("Reading file metadata", 1, 3)
        val metadata =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                if (GameFileFormats.isMetadataInspectable(file.name)) {
                    runCatching { GameFileMetadata.summarizeLocalFile(file) }.getOrNull()
                } else {
                    null
                }
            }
        fileMetadataProgress = MetadataLoadProgress("Preparing level metadata entry", 2, 3)
        val target =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                runCatching { LevelMetadataTargets.directFile(file, setDir, metadata) }.getOrNull()
            }
        fileMetadataResult = MetadataDetailLoadResult(metadata, target)
        fileMetadataProgress = MetadataLoadProgress("Metadata ready", 3, 3)
    }
    val fileMetadata = fileMetadataResult?.metadata
    val levelMetadataTarget = fileMetadataResult?.levelMetadataTarget
    val isMissing = !status.found && entry != null
    val isExternal = entry?.isExternal == true
    var confirmingDelete by remember { mutableStateOf(false) }
    var confirmingForget by remember { mutableStateOf(false) }
    var showingLevelMetadata by remember { mutableStateOf(false) }
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

    if (showingLevelMetadata && levelMetadataTarget != null) {
        LevelMetadataDialog(
            target = levelMetadataTarget,
            onDismiss = { showingLevelMetadata = false },
        )
    }

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
                    if (fileMetadataResult == null) {
                        MetadataLoadProgressView(
                            fileMetadataProgress,
                            modifier = Modifier.padding(top = 4.dp, bottom = 6.dp),
                        )
                    } else {
                        levelMetadataTarget?.let {
                            LevelMetadataButton(
                                onClick = { showingLevelMetadata = true },
                                modifier = Modifier.padding(top = 4.dp, bottom = 6.dp),
                            )
                        }
                    }
                    missionDescriptor?.let { mission ->
                        DetailRow("Title", mission.displayName)
                        mission.type?.let { DetailRow("Mission type", it) }
                        mission.author?.let { DetailRow("Author", it) }
                        mission.editor?.let { DetailRow("Editor", it) }
                        DetailRow("Levels", mission.levelNames.size.toString())
                        if (mission.levelNames.isNotEmpty()) {
                            DetailRow("Level names", mission.levelNames.joinToString(", "))
                        }
                        if (mission.secretLevelNames.isNotEmpty()) {
                            DetailRow("Secret levels", mission.secretLevelNames.size.toString())
                            DetailRow("Secret level names", mission.secretLevelNames.joinToString(", "))
                        }
                        mission.assetReferences.forEach { (label, value) ->
                            DetailRow(label, value)
                        }
                    }
                    FileMetadataDetails(fileMetadata)

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
private fun LevelMetadataButton(
    label: String = "Level metadata",
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    OutlinedButton(
        onClick = onClick,
        shape = MaterialTheme.shapes.small,
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
        modifier = modifier.fillMaxWidth(),
    ) {
        Text(label, fontSize = 12.sp)
    }
}

@Composable
private fun LevelMetadataDialog(
    target: LevelMetadataTarget,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    var result by remember(target) { mutableStateOf<LevelMetadataResult?>(null) }
    var loading by remember(target) { mutableStateOf(true) }
    var progress by remember(target) {
        mutableStateOf(MetadataLoadProgress("Preparing analysis files", 0, 5))
    }

    LaunchedEffect(target) {
        loading = true
        result = null
        progress = MetadataLoadProgress("Preparing analysis files", 0, 5)
        result =
            LevelMetadataAnalyzer.analyze(context, target) { update ->
                withContext(kotlinx.coroutines.Dispatchers.Main) {
                    progress = update
                }
            }
        loading = false
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text(target.displayName, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp),
            ) {
                val scrollState = rememberScrollState()
                Column(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .verticalScroll(scrollState)
                            .padding(end = 8.dp),
                ) {
                    if (loading) {
                        MetadataLoadProgressView(progress)
                    } else {
                        LevelMetadataResultContent(result)
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

@Composable
private fun LevelMetadataResultContent(result: LevelMetadataResult?) {
    if (result == null) {
        ModDetailLine("No analysis result", color = MaterialTheme.colorScheme.error)
        return
    }
    DetailRow("Status", result.status.replaceFirstChar { it.uppercase() })
    if (result.game.isNotBlank()) {
        DetailRow("Game", result.game.uppercase(Locale.US))
    }
    if (result.missionName.isNotBlank()) {
        DetailRow("Mission", result.missionName)
    }
    if (result.coopStarts.isNotBlank()) {
        DetailRow("Coop starts", result.coopStarts)
    }
    result.problems.forEach { problem ->
        ModDetailLine(problem, color = MaterialTheme.colorScheme.error)
    }
    result.diagnostics.forEach { diagnostic ->
        ModDetailLine(diagnostic)
    }
    if (result.levels.isEmpty()) return

    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
    ModDetailSectionTitle("Levels")
    LevelMetadataTable(result.levels)
}

@Composable
private fun LevelMetadataTable(levels: List<LevelMetadataLevelRow>) {
    val horizontal = rememberScrollState()
    val vertical = rememberScrollState()
    var selectedLevel by remember(levels) { mutableStateOf<LevelMetadataLevelRow?>(null) }

    selectedLevel?.let { row ->
        LevelMetadataLevelDialog(row = row, onDismiss = { selectedLevel = null })
    }

    Box(
        modifier =
            Modifier
                .fillMaxWidth(),
    ) {
        Column(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .horizontalScroll(horizontal),
        ) {
            LevelMetadataTableRow(
                level = "Level",
                name = "Name",
                robots = "Robots",
                hostages = "Hostages",
                secrets = "Secrets",
                matcens = "Matcens",
                energy = "Energy",
                volume = "Volume",
                travel = "Travel",
                bold = true,
            )
            HorizontalDivider(modifier = Modifier.width(824.dp).padding(bottom = 2.dp))
            Box(modifier = Modifier.heightIn(max = 260.dp)) {
                Column(
                    modifier =
                        Modifier
                            .verticalScroll(vertical)
                            .padding(end = 8.dp),
                ) {
                    levels.forEach { row ->
                        LevelMetadataTableRow(
                            level = if (row.secret) "S${-row.levelNum}" else row.levelNum.toString(),
                            name = row.levelName.ifBlank { row.levelFile },
                            robots = row.robots.toString(),
                            hostages = row.hostages.toString(),
                            secrets = row.secrets.toString(),
                            matcens = row.matcens.toString(),
                            energy = row.energyCenters.toString(),
                            volume =
                                row.mineVolumeText.ifBlank {
                                    if (row.mineVolumeNormalized > 0.0) {
                                        formatLevelMetadataVolumeMultiplier(row.mineVolumeNormalized)
                                    } else {
                                        "n/a"
                                    }
                                },
                            travel = row.travelTimeText,
                            problem = row.metadataProblem(),
                            note = row.metadataSummaryNote(),
                            onDetails = { selectedLevel = row },
                        )
                    }
                }
                SetupScrollArrows(vertical)
            }
        }
        SetupHorizontalScrollArrows(horizontal)
    }
}

@Composable
private fun LevelMetadataTableRow(
    level: String,
    name: String,
    robots: String,
    hostages: String,
    secrets: String,
    matcens: String,
    energy: String,
    volume: String,
    travel: String,
    bold: Boolean = false,
    problem: String? = null,
    note: String? = null,
    onDetails: (() -> Unit)? = null,
) {
    val weight = if (bold) FontWeight.SemiBold else FontWeight.Normal
    Row(
        modifier =
            Modifier
                .widthIn(min = 824.dp)
                .padding(vertical = 2.dp),
        verticalAlignment = Alignment.Top,
    ) {
        Text(level, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(44.dp))
        Text(name, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(184.dp))
        Text(robots, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(60.dp))
        Text(hostages, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(68.dp))
        Text(secrets, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(60.dp))
        Text(matcens, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(64.dp))
        Text(energy, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(64.dp))
        Text(volume, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(76.dp))
        Text(travel, fontSize = 11.sp, fontWeight = weight, modifier = Modifier.width(76.dp))
        Box(modifier = Modifier.width(44.dp), contentAlignment = Alignment.TopStart) {
            if (bold) {
                Text("Route", fontSize = 11.sp, fontWeight = weight)
            } else if (onDetails != null) {
                IconButton(onClick = onDetails, modifier = Modifier.size(24.dp)) {
                    Icon(Icons.Filled.Info, "Level route", modifier = Modifier.size(15.dp))
                }
            }
        }
    }
    problem?.let {
        Text(
            it,
            fontSize = 10.sp,
            color = MaterialTheme.colorScheme.error,
            modifier = Modifier.padding(start = 44.dp, bottom = 2.dp),
        )
    }
    note?.let {
        Text(
            it,
            fontSize = 10.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(start = 44.dp, bottom = 2.dp),
        )
    }
}

@Composable
private fun LevelMetadataLevelDialog(
    row: LevelMetadataLevelRow,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text(row.levelName.ifBlank { row.levelFile }, fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            Box(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .heightIn(max = 420.dp),
            ) {
                val scrollState = rememberScrollState()
                Column(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .verticalScroll(scrollState)
                            .padding(end = 8.dp),
                ) {
                    DetailRow("Level", formatLevelMetadataLevelNumber(row))
                    if (row.levelFile.isNotBlank()) {
                        DetailRow("File", row.levelFile)
                    }
                    DetailRow("Status", row.status.replaceFirstChar { it.uppercase() })
                    DetailRow("Travel", row.travelTimeText.ifBlank { "n/a" })
                    if (row.travelStatus != "ok" && row.travelProblem.isNotBlank()) {
                        ModDetailLine(
                            "Travel ${row.travelStatus}: ${row.travelProblem}",
                            MaterialTheme.colorScheme.error,
                        )
                    }
                    if (row.travelNote.isNotBlank()) {
                        ModDetailLine(row.travelNote)
                    }
                    row
                        .metadataNotes()
                        .filter { it != row.travelNote }
                        .forEach { note -> ModDetailLine(note) }
                    ModDetailSectionTitle("Path")
                    if (row.routeStatus.isNotBlank()) {
                        DetailRow("Route status", row.routeStatus.replaceFirstChar { it.uppercase() })
                    }
                    if (row.routeProblem.isNotBlank()) {
                        ModDetailLine(row.routeProblem, MaterialTheme.colorScheme.error)
                    }
                    if (row.routeSteps.isEmpty()) {
                        ModDetailLine("No route steps")
                    } else {
                        row.routeSteps.forEachIndexed { displayIndex, step ->
                            Text(
                                "${displayIndex + 1}. ${step.routeStepTitle()}",
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.onSurface,
                                modifier = Modifier.padding(bottom = 1.dp),
                            )
                            step.routeStepDetails().takeIf { it.isNotBlank() }?.let { details ->
                                Text(
                                    details,
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.padding(start = 16.dp, bottom = 3.dp),
                                )
                            }
                            step.routeOpenSummary().takeIf { it.isNotBlank() }?.let { opens ->
                                Text(
                                    opens,
                                    fontSize = 10.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    modifier = Modifier.padding(start = 16.dp, bottom = 3.dp),
                                )
                            }
                        }
                    }
                }
                SetupScrollArrows(scrollState)
            }
        },
    )
}

private fun formatLevelMetadataLevelNumber(row: LevelMetadataLevelRow): String =
    if (row.secret) {
        "S${-row.levelNum}"
    } else {
        row.levelNum.toString()
    }

private fun formatLevelMetadataVolumeMultiplier(value: Double): String {
    if (value <= 0.0) return ""
    val (displayValue, decimals) =
        if (value >= 10.0) {
            val scale = 10.0.pow(floor(log10(value)) - 1.0)
            floor(value / scale + 0.5) * scale to 0
        } else {
            value to if (value >= 1.0) 1 else 2
        }
    return "%.${decimals}fx".format(Locale.US, displayValue)
}

private fun LevelMetadataLevelRow.metadataProblem(): String? {
    val messages =
        buildList {
            addAll(problems)
            if (travelStatus != "ok" && travelProblem.isNotBlank()) {
                add("Travel $travelStatus: $travelProblem")
            }
            if (routeStatus.isNotBlank() && routeStatus != "ok") {
                add(
                    if (routeProblem.isNotBlank()) {
                        "Route $routeStatus: $routeProblem"
                    } else {
                        "Route $routeStatus"
                    },
                )
            }
        }
    return messages.joinToString("; ").takeIf { it.isNotBlank() }
}

private fun LevelMetadataLevelRow.metadataSummaryNote(): String? =
    metadataNotes()
        .filter { it != guidebotPlacementNote }
        .joinToString("; ") {
            "note: $it"
        }.takeIf { it.isNotBlank() }

private fun LevelMetadataLevelRow.metadataNotes(): List<String> {
    val messages =
        buildList {
            addAll(notes)
            if (travelNote.isNotBlank() && travelNote !in notes) {
                add(travelNote)
            }
            if (guidebotPlacementNote.isNotBlank() && guidebotPlacementNote !in this) {
                add(guidebotPlacementNote)
            }
            if (guidebotNote.isNotBlank() && guidebotNote !in this) {
                add(guidebotNote)
            }
        }
    return messages.distinct()
}

private fun LevelMetadataRouteStep.routeStepTitle(): String =
    routeActivationLabel().ifBlank { label.ifBlank { routeStepFallbackLabel() } }

private fun LevelMetadataRouteStep.routeStepDetails(): String =
    buildList {
        val title = routeStepTitle()
        if (kind == "trigger" && trigger >= 0 && !title.contains("trigger $trigger", ignoreCase = true)) {
            add("trigger $trigger")
        }
        if (triggerType.isNotBlank() && kind == "trigger") {
            add(triggerType.replace('_', ' '))
        }
        if (seg >= 0) add("segment $seg")
        if (side >= 0) add("side $side")
        if (wall >= 0 && kind == "trigger") add("wall $wall")
        if (distance > 0.0) add("${"%.0f".format(Locale.US, distance)} units")
    }.joinToString(", ")

private fun LevelMetadataRouteStep.routeActivationLabel(): String =
    when (activationKind) {
        "pickup_key" -> {
            key
                .ifBlank { "" }
                .replaceFirstChar { it.uppercase() }
                .let { if (it.isBlank()) "Collect key" else "Collect $it key" }
        }

        "shoot_switch" -> {
            "Shoot switch"
        }

        "fly_through_trigger" -> {
            "Fly through trigger"
        }

        "activate_switch" -> {
            "Activate switch"
        }

        "open_hidden_door" -> {
            "Open hidden wall door"
        }

        "destroy_reactor" -> {
            "Destroy reactor"
        }

        "destroy_boss" -> {
            "Destroy boss"
        }

        "enter_exit" -> {
            "Enter exit"
        }

        else -> {
            ""
        }
    }

private fun LevelMetadataRouteStep.routeStepFallbackLabel(): String =
    when (kind) {
        "start" -> "Start"
        "key" -> key.ifBlank { "Key" }.replaceFirstChar { it.uppercase() } + " key"
        "trigger" -> "Trigger"
        "reactor" -> "Reactor"
        "boss" -> "Boss"
        "exit" -> "Exit"
        "hidden_door" -> "Hidden door"
        "hostage" -> "Hostage"
        else -> kind.ifBlank { "Step" }.replace('_', ' ').replaceFirstChar { it.uppercase() }
    }

private fun LevelMetadataRouteStep.routeOpenSummary(): String =
    opens
        .takeIf { it.isNotEmpty() }
        ?.joinToString(prefix = "Opens: ") { link ->
            buildList {
                if (link.seg >= 0) add("segment ${link.seg}")
                if (link.side >= 0) add("side ${link.side}")
                if (link.wall >= 0) add("wall ${link.wall}")
            }.joinToString(" ").ifBlank { "target" }
        }.orEmpty()

@Composable
private fun FileMetadataDetails(metadata: GameFileMetadata.Summary?) {
    metadata ?: return
    var showContentsDialog by remember(metadata) { mutableStateOf(false) }
    if (showContentsDialog) {
        MetadataContentsDialog(
            entries = metadata.contents,
            onDismiss = { showContentsDialog = false },
        )
    }
    HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
    ModDetailSectionTitle("Contents")
    DetailRow("Format", metadata.format)
    DetailRow("Scope", metadata.scope)
    if (metadata.game != "Unknown") {
        DetailRow("Game", metadata.game)
    }
    metadata.detailRows.forEach { row -> DetailRow(row.first, row.second) }
    if (metadata.categories.isNotEmpty()) {
        metadata.categories.take(6).forEach { category ->
            val sizeText =
                if (category.sizeBytes > 0) {
                    ", ${setupSectionFormatSize(category.sizeBytes)}"
                } else {
                    ""
                }
            ModDetailLine("${category.label}: ${category.count}$sizeText")
        }
    }
    if (metadata.contents.isNotEmpty()) {
        ModDetailSectionTitle("Contents preview")
        MetadataContentsBox(metadata.contents.take(5), maxHeight = 132.dp)
        if (metadata.contents.size > 5) {
            OutlinedButton(
                onClick = { showContentsDialog = true },
                shape = MaterialTheme.shapes.small,
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp),
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(bottom = 4.dp),
            ) {
                Text("View entire contents", fontSize = 12.sp)
            }
        }
    }
    metadata.notes.forEach { note -> ModDetailLine(note) }
    metadata.problems.forEach { problem ->
        ModDetailLine(problem, color = MaterialTheme.colorScheme.error)
    }
}

@Composable
private fun MetadataContentsDialog(
    entries: List<GameFileMetadata.EntrySummary>,
    onDismiss: () -> Unit,
) {
    var sortMode by remember { mutableStateOf(MetadataContentSort.NameAscending) }
    val sortedEntries =
        remember(entries, sortMode) {
            when (sortMode) {
                MetadataContentSort.NameAscending -> {
                    entries.sortedBy { it.name.lowercase(Locale.US) }
                }

                MetadataContentSort.NameDescending -> {
                    entries.sortedByDescending { it.name.lowercase(Locale.US) }
                }

                MetadataContentSort.SizeAscending -> {
                    entries.sortedWith(
                        compareBy<GameFileMetadata.EntrySummary> { it.sizeBytes }.thenBy {
                            it.name.lowercase(
                                Locale.US,
                            )
                        },
                    )
                }

                MetadataContentSort.SizeDescending -> {
                    entries.sortedWith(
                        compareByDescending<GameFileMetadata.EntrySummary> { it.sizeBytes }
                            .thenBy { it.name.lowercase(Locale.US) },
                    )
                }

                MetadataContentSort.Type -> {
                    entries.sortedWith(
                        compareBy<GameFileMetadata.EntrySummary> { it.role.lowercase(Locale.US) }
                            .thenBy { it.name.lowercase(Locale.US) },
                    )
                }
            }
        }

    AlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Text("Explore contents", fontWeight = FontWeight.Bold, fontSize = 16.sp)
        },
        text = {
            Column(modifier = Modifier.fillMaxWidth()) {
                Text(
                    "${entries.size} entries",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(bottom = 6.dp),
                )
                MetadataSortControls(sortMode, onSelect = { sortMode = it })
                MetadataContentsBox(sortedEntries, maxHeight = 360.dp)
            }
        },
    )
}

@Composable
private fun MetadataSortControls(
    selected: MetadataContentSort,
    onSelect: (MetadataContentSort) -> Unit,
) {
    Column(modifier = Modifier.padding(bottom = 6.dp)) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            MetadataSortChip(MetadataContentSort.NameAscending, selected, onSelect, Modifier.weight(1f))
            MetadataSortChip(MetadataContentSort.NameDescending, selected, onSelect, Modifier.weight(1f))
            MetadataSortChip(MetadataContentSort.Type, selected, onSelect, Modifier.weight(1f))
        }
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            MetadataSortChip(MetadataContentSort.SizeAscending, selected, onSelect, Modifier.weight(1f))
            MetadataSortChip(MetadataContentSort.SizeDescending, selected, onSelect, Modifier.weight(1f))
        }
    }
}

@Composable
private fun MetadataSortChip(
    mode: MetadataContentSort,
    selected: MetadataContentSort,
    onSelect: (MetadataContentSort) -> Unit,
    modifier: Modifier = Modifier,
) {
    FilterChip(
        selected = mode == selected,
        onClick = { onSelect(mode) },
        label = {
            Text(mode.label, fontSize = 11.sp, maxLines = 1)
        },
        modifier = modifier.height(34.dp),
    )
}

private enum class MetadataContentSort(
    val label: String,
) {
    NameAscending("Name A-Z"),
    NameDescending("Name Z-A"),
    SizeAscending("Size up"),
    SizeDescending("Size down"),
    Type("Type"),
}

@Composable
private fun MetadataContentsBox(
    entries: List<GameFileMetadata.EntrySummary>,
    maxHeight: Dp,
) {
    val scrollState = rememberScrollState()
    Surface(
        tonalElevation = 1.dp,
        shape = MaterialTheme.shapes.small,
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.45f),
        modifier =
            Modifier
                .fillMaxWidth()
                .heightIn(max = maxHeight)
                .padding(bottom = 4.dp),
    ) {
        SelectionContainer {
            Column(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .verticalScroll(scrollState)
                        .padding(start = 8.dp, top = 6.dp, end = 8.dp, bottom = 10.dp),
            ) {
                entries.forEach { entry ->
                    val sizeText =
                        if (entry.sizeBytes > 0) {
                            " - ${setupSectionFormatSize(entry.sizeBytes)}"
                        } else {
                            ""
                        }
                    Text(
                        "${entry.name} - ${entry.role}$sizeText",
                        fontSize = 11.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(bottom = 2.dp),
                    )
                }
            }
        }
    }
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
