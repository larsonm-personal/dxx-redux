package com.dxxredux.app

import android.provider.DocumentsContract
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive

internal const val LARGE_IMPORT_DIRECTORY_FILE_COUNT = 200
internal const val MAX_IMPORT_TREE_DEPTH = 32
internal const val MAX_IMPORT_TREE_DIRECTORIES = 512
internal const val MAX_IMPORT_TREE_ROWS = 10_000
internal const val MAX_IMPORT_TREE_RESULTS = 2_000
internal const val IMPORT_TREE_QUERY_TIMEOUT_MS = 10_000L
internal const val IMPORT_TREE_SCAN_TIMEOUT_MS = 30_000L

internal data class ImportTreeRow(
    val documentId: String,
    val displayName: String?,
    val mimeType: String,
)

internal data class ImportTreeRowClassification(
    val childDirectoryIds: List<String>,
    val importableDocumentIds: List<String>,
    val scannedFileCount: Int,
    val skippedUnknownFileCount: Int,
)

internal data class ImportTreeTraversalResult(
    val importableDocumentIds: List<String>,
    val scannedFileCount: Int,
    val skippedUnknownFileCount: Int,
)

internal data class ImportTreeScanLimits(
    val maxDepth: Int = MAX_IMPORT_TREE_DEPTH,
    val maxDirectories: Int = MAX_IMPORT_TREE_DIRECTORIES,
    val maxRows: Int = MAX_IMPORT_TREE_ROWS,
    val maxResults: Int = MAX_IMPORT_TREE_RESULTS,
)

internal class ImportTreeScanException(
    message: String,
) : Exception(message)

private data class PendingImportTreeDirectory(
    val documentId: String,
    val depth: Int,
)

private fun validProviderDocumentId(documentId: String): Boolean =
    documentId.isNotBlank() && documentId.length <= 1024 && documentId.none(Char::isISOControl)

internal suspend fun traverseImportTree(
    rootDocumentId: String,
    allGameFileNames: Set<String>,
    limits: ImportTreeScanLimits = ImportTreeScanLimits(),
    queryChildren: suspend (parentDocumentId: String, remainingRowBudget: Int) -> List<ImportTreeRow>,
): ImportTreeTraversalResult {
    val rootIsValid = validProviderDocumentId(rootDocumentId)
    if (!rootIsValid) {
        throw ImportTreeScanException("provider returned an invalid root document ID")
    }
    if (limits.maxDepth < 0 || limits.maxDirectories < 1 || limits.maxRows < 1 || limits.maxResults < 1) {
        throw IllegalArgumentException("import tree limits must be positive")
    }

    val queue = ArrayDeque<PendingImportTreeDirectory>()
    val seenDocumentIds = mutableSetOf(rootDocumentId)
    val importableDocumentIds = mutableListOf<String>()
    var directoryCount = 1
    var rowCount = 0
    var scannedFileCount = 0
    var skippedUnknownFileCount = 0
    queue.add(PendingImportTreeDirectory(rootDocumentId, 0))

    while (queue.isNotEmpty()) {
        currentCoroutineContext().ensureActive()
        val parent = queue.removeFirst()
        val remainingRowBudget = limits.maxRows - rowCount
        if (remainingRowBudget <= 0) {
            throw ImportTreeScanException("selected folder exceeds the ${limits.maxRows}-row scan limit")
        }
        val rows = queryChildren(parent.documentId, remainingRowBudget)
        if (rows.size > remainingRowBudget) {
            throw ImportTreeScanException("selected folder exceeds the ${limits.maxRows}-row scan limit")
        }
        rowCount += rows.size

        for (row in rows) {
            currentCoroutineContext().ensureActive()
            if (!validProviderDocumentId(row.documentId)) {
                throw ImportTreeScanException("provider returned an invalid document ID")
            }
            if (!seenDocumentIds.add(row.documentId)) {
                throw ImportTreeScanException("provider repeated document ID '${row.documentId}'")
            }
            if (row.mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
                if (parent.depth >= limits.maxDepth) {
                    throw ImportTreeScanException("selected folder exceeds the ${limits.maxDepth}-level depth limit")
                }
                if (directoryCount >= limits.maxDirectories) {
                    throw ImportTreeScanException(
                        "selected folder exceeds the ${limits.maxDirectories}-directory scan limit",
                    )
                }
                directoryCount++
                queue.add(PendingImportTreeDirectory(row.documentId, parent.depth + 1))
                continue
            }

            val displayName = row.displayName ?: continue
            scannedFileCount++
            if (isDirectoryImportCandidateName(displayName, allGameFileNames)) {
                if (importableDocumentIds.size >= limits.maxResults) {
                    throw ImportTreeScanException("selected folder exceeds the ${limits.maxResults}-file import limit")
                }
                importableDocumentIds.add(row.documentId)
            } else {
                skippedUnknownFileCount++
            }
        }
    }

    return ImportTreeTraversalResult(
        importableDocumentIds = importableDocumentIds,
        scannedFileCount = scannedFileCount,
        skippedUnknownFileCount = skippedUnknownFileCount,
    )
}

internal fun classifyImportTreeRows(
    rows: List<ImportTreeRow>,
    allGameFileNames: Set<String>,
): ImportTreeRowClassification {
    val childDirectoryIds = mutableListOf<String>()
    val importableDocumentIds = mutableListOf<String>()
    var scannedFileCount = 0
    var skippedUnknownFileCount = 0

    for (row in rows) {
        if (row.mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
            childDirectoryIds.add(row.documentId)
            continue
        }

        val displayName = row.displayName ?: continue
        scannedFileCount += 1
        if (isDirectoryImportCandidateName(displayName, allGameFileNames)) {
            importableDocumentIds.add(row.documentId)
        } else {
            skippedUnknownFileCount += 1
        }
    }

    return ImportTreeRowClassification(
        childDirectoryIds = childDirectoryIds,
        importableDocumentIds = importableDocumentIds,
        scannedFileCount = scannedFileCount,
        skippedUnknownFileCount = skippedUnknownFileCount,
    )
}

internal fun isDirectoryImportCandidateName(
    name: String,
    allGameFileNames: Set<String>,
): Boolean {
    val lowercaseName = name.lowercase()
    return lowercaseName.endsWith(".zip") ||
        lowercaseName.endsWith(".7z") ||
        lowercaseName.endsWith(".rar") ||
        lowercaseName.endsWith(".cue") ||
        lowercaseName.endsWith(".iso") ||
        lowercaseName.endsWith(".bin") ||
        lowercaseName.endsWith(".exe") ||
        lowercaseName.endsWith(".pkg") ||
        lowercaseName.endsWith(".sow") ||
        isLauncherDxaFilename(name) ||
        AndroidGameFileExtensions.hasGameExtension(name) ||
        lowercaseName in allGameFileNames ||
        AndroidGameFileExtensions.isGogAudioFile(name)
}

internal fun largeDirectoryImportWarning(
    scannedFileCount: Int,
    skippedUnknownFileCount: Int,
): String? {
    if (scannedFileCount <= LARGE_IMPORT_DIRECTORY_FILE_COUNT) {
        return null
    }
    return if (skippedUnknownFileCount > 0) {
        "Scanned $scannedFileCount files and skipped $skippedUnknownFileCount unrelated entries"
    } else {
        "Scanned $scannedFileCount files in selected folder"
    }
}
