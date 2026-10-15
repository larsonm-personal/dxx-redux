package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.util.Locale

/** Keep the selection composed below the viewer, preserving its scroll and navigation state */
@Composable
internal fun PackageDocumentDialog(
    title: String,
    archivePath: String,
    documents: List<PackageDocument>,
    onDismiss: () -> Unit,
    published: Boolean = false,
    warning: String? = null,
    onRetry: (() -> Unit)? = null,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val single = documents.singleOrNull()
    var selected by remember(archivePath, documents) {
        mutableStateOf(single?.takeIf { PackageDocuments.isInlineReadmeCandidate(it.path) })
    }
    val caption = if (published) "$title\nPublished documentation" else title

    fun open(document: PackageDocument) {
        if (PackageDocuments.isInlineReadmeCandidate(document.path)) {
            selected = document
        } else {
            scope.launch { openPackageExternalDocument(context, archivePath, document) }
        }
    }
    LaunchedEffect(archivePath, documents) {
        if (single != null && PackageDocuments.isExternalReadmeCandidate(single.path)) open(single)
    }
    // Retain a reopen action for external-only docs, including when no viewer is installed
    if (single == null || selected == null) {
        NavigationAlertDialog(
            onDismissRequest = onDismiss,
            title = { Text("Documents") },
            confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
            text = {
                val scroll = rememberScrollState()
                Box(Modifier.fillMaxWidth().heightIn(max = 420.dp)) {
                    Column(
                        Modifier.verticalScroll(scroll).padding(end = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(caption, style = MaterialTheme.typography.titleSmall)
                        DocumentCacheNotice(warning, onRetry)
                        documents.forEach { document ->
                            OutlinedButton(onClick = { open(document) }, modifier = Modifier.fillMaxWidth()) {
                                Column(Modifier.fillMaxWidth()) {
                                    if (document.isReduxNotes) Text("Redux adaptation notes")
                                    Text(document.path)
                                    val type =
                                        GameFileFormats
                                            .extensionOf(
                                                document.path,
                                            ).ifEmpty { "text" }
                                            .uppercase(Locale.US)
                                    Text(type, style = MaterialTheme.typography.labelSmall)
                                }
                            }
                        }
                        if (documents.isEmpty()) Text("No documents found")
                    }
                    SetupScrollArrows(scroll)
                }
            },
        )
    }
    selected?.let { document ->
        PackageDocumentTextDialog(
            document = document,
            archivePath = archivePath,
            caption = caption,
            warning = warning,
            onRetry = onRetry,
            onDismiss = { if (single != null) onDismiss() else selected = null },
        )
    }
}

@Composable
internal fun DocumentCacheNotice(
    warning: String?,
    onRetry: (() -> Unit)?,
) {
    if (warning != null) {
        Text(warning, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
        if (onRetry != null) TextButton(onClick = onRetry) { Text("Retry") }
    }
}

internal suspend fun loadRecommendedModDocuments(
    cacheDir: File,
    mod: RecommendedMod,
) = RecommendedModDocuments(cacheDir).load(mod.downloadUrl, mod.filename)

@Composable
internal fun RecommendedModDocumentsDialog(
    mod: RecommendedMod,
    onDismiss: () -> Unit,
    loadDocuments: suspend (File, RecommendedMod) -> RecommendedModDocuments.Loaded = ::loadRecommendedModDocuments,
) {
    val context = LocalContext.current
    var attempt by remember(mod.downloadUrl) { mutableIntStateOf(0) }
    var loaded by remember(mod.downloadUrl) { mutableStateOf<RecommendedModDocuments.Loaded?>(null) }
    var problem by remember(mod.downloadUrl) { mutableStateOf<String?>(null) }
    LaunchedEffect(mod.downloadUrl, attempt) {
        loaded = null
        problem = null
        try {
            loaded = loadDocuments(File(context.cacheDir, "mod-documents"), mod)
        } catch (cancelled: CancellationException) {
            throw cancelled
        } catch (error: Exception) {
            problem = error.message ?: "Could not load documentation"
        }
    }
    val result = loaded
    if (result != null) {
        key(attempt) {
            PackageDocumentDialog(
                mod.displayName,
                result.archive.absolutePath,
                result.documents,
                onDismiss,
                published = true,
                warning = result.warning,
                onRetry = { attempt++ },
            )
        }
    } else {
        NavigationAlertDialog(
            onDismissRequest = onDismiss,
            title = { Text("Published documentation") },
            confirmButton = { TextButton(onClick = onDismiss) { Text("Close") } },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(mod.displayName)
                    val error = problem
                    if (error == null) {
                        CircularProgressIndicator()
                        Text("Loading documents")
                    } else {
                        Text(error, color = MaterialTheme.colorScheme.error)
                        TextButton(onClick = { attempt++ }) { Text("Retry") }
                    }
                }
            },
        )
    }
}

@Composable
internal fun PackageDocumentTextDialog(
    document: PackageDocument,
    archivePath: String?,
    onDismiss: () -> Unit,
    caption: String? = null,
    warning: String? = null,
    onRetry: (() -> Unit)? = null,
) {
    var content by remember(archivePath, document.path) {
        mutableStateOf<MissionZip.TextFileContent?>(null)
    }
    LaunchedEffect(archivePath, document.path) {
        content = null
        content = withContext(kotlinx.coroutines.Dispatchers.IO) { readPackageTextFile(archivePath, document) }
    }
    NavigationAlertDialog(
        onDismissRequest = onDismiss,
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("Close") }
        },
        title = {
            Column {
                if (document.isReduxNotes) Text("Redux adaptation notes", style = MaterialTheme.typography.titleSmall)
                Text(document.name, fontWeight = FontWeight.Bold, fontSize = 16.sp)
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
                SelectionContainer {
                    Column(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .verticalScroll(scrollState)
                                .padding(end = 8.dp),
                    ) {
                        caption?.let { Text(it, style = MaterialTheme.typography.labelLarge) }
                        DocumentCacheNotice(warning, onRetry)
                        val loadedContent = content
                        if (loadedContent == null) {
                            MetadataLoadProgressView(MetadataLoadProgress("Reading text file", 0, 1))
                            return@Column
                        }
                        loadedContent.problem?.let { problem ->
                            Text(
                                problem,
                                fontSize = 12.sp,
                                color = MaterialTheme.colorScheme.error,
                                modifier = Modifier.padding(bottom = 6.dp),
                            )
                        }
                        if (loadedContent.truncated) {
                            Text(
                                "Showing first ${formatBinarySize(1024L * 1024L)}",
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.padding(bottom = 6.dp),
                            )
                        }
                        Text(
                            loadedContent.text.ifEmpty { "(empty text file)" },
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

private fun readPackageTextFile(
    archivePath: String?,
    document: PackageDocument,
): MissionZip.TextFileContent {
    if (archivePath ==
        null
    ) {
        return MissionZip.TextFileContent("", truncated = false, problem = "Package archive is missing")
    }
    missionZipExtractedStoreForArchivePath(archivePath)
        ?.extractedEntryForArchiveEntry(archivePath, document.path)
        ?.let { return PackageDocuments.readExtractedDocument(it.file, document.path) }
    return PackageDocuments.readTextFile(File(archivePath), document.path)
}

internal suspend fun openPackageExternalDocument(
    context: Context,
    archivePath: String?,
    document: PackageDocument,
) {
    if (archivePath == null) {
        Toast.makeText(context, "Package archive is missing", Toast.LENGTH_SHORT).show()
        return
    }
    try {
        val uri =
            withContext(kotlinx.coroutines.Dispatchers.IO) {
                extractPackageDocumentToCache(context, File(archivePath), document)
            }
        openExternalFile(context, uri, PackageDocuments.externalViewMimeType(document.name), document.name)
    } catch (e: kotlinx.coroutines.CancellationException) {
        throw e
    } catch (e: Exception) {
        Toast.makeText(context, "Could not open ${document.name}: ${e.message}", Toast.LENGTH_SHORT).show()
    }
}

private fun extractPackageDocumentToCache(
    context: Context,
    archive: File,
    document: PackageDocument,
): Uri =
    FileProviderGrantStore.publish(
        context,
        FileProviderGrantStore.FILE_VIEW,
        document.name,
        document.sizeBytes,
    ) { temporary ->
        val extracted =
            missionZipExtractedStoreForArchivePath(archive.absolutePath)
                ?.extractedEntryForArchiveEntry(archive.absolutePath, document.path)
                ?.file
        if (extracted != null) {
            LauncherFileCopy.copyFileToFile(extracted, temporary, maxBytes = FileProviderGrantStore.MAX_ROOT_BYTES)
        } else {
            ArchiveFiles.open(archive).use { archiveFile ->
                val entry =
                    archiveFile.findEntry(document.path)
                        ?: throw IllegalArgumentException("Document file is missing")
                if (entry.isDirectory) throw IllegalArgumentException("Document path is a directory")
                archiveFile.openInputStream(entry).use { input ->
                    FileOutputStream(temporary).use { output ->
                        LauncherFileCopy.copyStream(
                            input,
                            output,
                            entry.sizeBytes,
                            document.name,
                            maxBytes = FileProviderGrantStore.MAX_ROOT_BYTES,
                        )
                    }
                }
            }
        }
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
