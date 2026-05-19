package com.dxxredux.app

import android.provider.DocumentsContract

internal const val LARGE_IMPORT_DIRECTORY_FILE_COUNT = 200

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
