package com.dxxredux.app

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.apache.commons.compress.archivers.sevenz.SevenZFile
import java.io.File
import java.io.FileOutputStream

internal data class FoundFile(
    val name: String,
    val uri: Uri,
)

internal data class ExtractedFile(
    val name: String,
    val tmpFile: File,
    val sha256: String,
    val sizeBytes: Long,
)

internal data class DirectoryImportScanResult(
    val uris: List<Uri>,
    val scannedFileCount: Int,
    val skippedUnknownFileCount: Int,
)

internal data class ZipExtractionResult(
    val files: List<ExtractedFile>,
    val hadAudioFiles: Boolean,
    val error: String? = null,
)

internal fun scanTreeForGameFiles(
    context: Context,
    treeUri: Uri,
): List<FoundFile> {
    val results = mutableListOf<FoundFile>()
    val docId = DocumentsContract.getTreeDocumentId(treeUri)
    val queue = ArrayDeque<String>()
    queue.add(docId)

    while (queue.isNotEmpty()) {
        val parentId = queue.removeFirst()
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId)
        val cursor =
            context.contentResolver.query(
                childrenUri,
                arrayOf(
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                ),
                null,
                null,
                null,
            ) ?: continue

        cursor.use {
            while (it.moveToNext()) {
                val childId = it.getString(0)
                val displayName = it.getString(1) ?: continue
                val mimeType = it.getString(2) ?: ""

                if (mimeType == DocumentsContract.Document.MIME_TYPE_DIR) {
                    queue.add(childId)
                } else if (isDirectGameDataImportName(displayName)) {
                    val fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, childId)
                    results.add(FoundFile(displayName, fileUri))
                }
            }
        }
    }
    return results
}

internal fun isDirectGameDataImportName(
    name: String,
    allGameFileNames: Set<String> = ALL_GAME_FILENAMES,
): Boolean =
    AndroidGameFileExtensions.hasGameExtension(name) ||
        name.lowercase() in allGameFileNames

internal fun scanTreeForImportUris(
    context: Context,
    treeUri: Uri,
): DirectoryImportScanResult {
    val results = mutableListOf<Uri>()
    var scannedFileCount = 0
    var skippedUnknownFileCount = 0
    val docId = DocumentsContract.getTreeDocumentId(treeUri)
    val queue = ArrayDeque<String>()
    queue.add(docId)

    while (queue.isNotEmpty()) {
        val parentId = queue.removeFirst()
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentId)
        val cursor =
            context.contentResolver.query(
                childrenUri,
                arrayOf(
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                ),
                null,
                null,
                null,
            ) ?: continue

        val rows = mutableListOf<ImportTreeRow>()
        cursor.use {
            while (it.moveToNext()) {
                rows.add(
                    ImportTreeRow(
                        documentId = it.getString(0),
                        displayName = it.getString(1),
                        mimeType = it.getString(2) ?: "",
                    ),
                )
            }
        }

        val classification = classifyImportTreeRows(rows, ALL_GAME_FILENAMES)
        queue.addAll(classification.childDirectoryIds)
        scannedFileCount += classification.scannedFileCount
        skippedUnknownFileCount += classification.skippedUnknownFileCount
        for (childId in classification.importableDocumentIds) {
            results.add(DocumentsContract.buildDocumentUriUsingTree(treeUri, childId))
        }
    }
    return DirectoryImportScanResult(
        uris = results,
        scannedFileCount = scannedFileCount,
        skippedUnknownFileCount = skippedUnknownFileCount,
    )
}

internal fun importFile(
    context: Context,
    source: FoundFile,
    destDir: File,
    onProgress: (LauncherCopyProgress) -> Unit = {},
): Boolean =
    try {
        val canonicalName = source.name.lowercase()
        val actualDestDir =
            if (canonicalName.endsWith(".dem")) {
                File(destDir, "demos").also { it.mkdirs() }
            } else {
                destDir
            }
        val destFile = File(actualDestDir, canonicalName)
        ImportStorageGuard.requireFreeSpace(
            actualDestDir,
            ImportStorageGuard.queryUriSizeBytes(context.contentResolver, source.uri) ?: 0L,
            "import ${source.name}",
        )
        LauncherFileCopy.copyUriToFile(context, source.uri, destFile, source.name, onProgress)
        Log.i("DXX-Setup", "Imported ${source.name} -> $canonicalName (${destFile.length()} bytes)")
        true
    } catch (e: InsufficientStorageException) {
        Log.e("DXX-Setup", "Not enough space to import ${source.name}", e)
        ImportStorageGuard.recordFailure(context.filesDir, "Import failed for ${source.name}", e)
        false
    } catch (e: Exception) {
        Log.e("DXX-Setup", "Failed to import ${source.name}", e)
        ImportStorageGuard.recordFailure(context.filesDir, "Import failed for ${source.name}", e)
        false
    }

internal fun getDisplayName(
    context: Context,
    uri: Uri,
): String? =
    try {
        context.contentResolver
            .query(
                uri,
                arrayOf(OpenableColumns.DISPLAY_NAME),
                null,
                null,
                null,
            )?.use { cursor ->
                if (cursor.moveToFirst()) cursor.getString(0) else null
            }
    } catch (e: Exception) {
        null
    }

internal suspend fun extractZipContents(
    context: Context,
    zipUri: Uri,
    tmpDir: File,
    archiveName: String? = null,
    onProgress: suspend (String, Long, Long) -> Unit,
): ZipExtractionResult =
    kotlinx.coroutines.withContext(Dispatchers.IO) {
        tmpDir.mkdirs()
        val results = mutableListOf<ExtractedFile>()
        val sowFiles = mutableListOf<File>()
        var foundAudio = false
        val audioExts = setOf("mp3", "ogg", "flac")
        val expectedDemoFiles =
            archiveName
                ?.let { DemoInstallerPackages.matchByName(it) }
                ?.expectedFiles
                ?.toSet()

        fun shouldKeepGameFile(name: String): Boolean =
            if (expectedDemoFiles != null) {
                name in expectedDemoFiles
            } else {
                name in ALL_GAME_FILENAMES
            }
        try {
            context.contentResolver.openInputStream(zipUri)?.use { raw ->
                openZipInputStreamSkippingPreamble(raw).use { zis ->
                    var entry = zis.nextEntry
                    while (entry != null) {
                        val name = entry.name.substringAfterLast('/').lowercase()
                        if (!entry.isDirectory && !foundAudio) {
                            val ext = name.substringAfterLast('.', "")
                            if (ext in audioExts) foundAudio = true
                        }
                        if (!entry.isDirectory && (shouldKeepGameFile(name) || name.endsWith(".sow"))) {
                            val totalBytes = entry.size.takeIf { it > 0L } ?: 0L
                            kotlinx.coroutines.withContext(Dispatchers.Main) {
                                onProgress(name, 0L, totalBytes)
                            }
                            ImportStorageGuard.requireFreeSpace(
                                tmpDir,
                                totalBytes,
                                "extract $name",
                            )
                            val tmpFile = File(tmpDir, name)
                            val digest = java.security.MessageDigest.getInstance("SHA-256")
                            var size = 0L
                            var lastReported = 0L
                            FileOutputStream(tmpFile).use { out ->
                                val buf = ByteArray(8192)
                                while (true) {
                                    val n = zis.read(buf)
                                    if (n <= 0) break
                                    out.write(buf, 0, n)
                                    digest.update(buf, 0, n)
                                    size += n
                                    if (size - lastReported >= 1024L * 1024L || size == totalBytes) {
                                        lastReported = size
                                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                                            onProgress(name, size, totalBytes)
                                        }
                                    }
                                }
                            }
                            kotlinx.coroutines.withContext(Dispatchers.Main) {
                                onProgress(name, size, totalBytes)
                            }
                            val sha256 = digest.digest().joinToString("") { "%02x".format(it) }
                            if (name.endsWith(".sow")) {
                                sowFiles.add(tmpFile)
                                Log.i("DXX-Setup", "Extracted SOW from archive: $name ($size bytes)")
                            } else {
                                results.add(ExtractedFile(name, tmpFile, sha256, size))
                                Log.i(
                                    "DXX-Setup",
                                    "Extracted from ZIP: $name ($size bytes, sha256=${sha256.take(16)}...)",
                                )
                            }
                        }
                        zis.closeEntry()
                        entry = zis.nextEntry
                    }
                }
            }
            if (sowFiles.isNotEmpty()) {
                val existingNames = results.map { it.name }.toMutableSet()
                val tempFiles = tmpDir.listFiles()?.filter { it.isFile } ?: emptyList()
                for (file in tempFiles) {
                    val lowerName = file.name.lowercase()
                    if (shouldKeepGameFile(lowerName) && lowerName !in existingNames) file.delete()
                }
                for (sowFile in sowFiles.sortedBy { it.name.lowercase() }) {
                    kotlinx.coroutines.withContext(Dispatchers.Main) {
                        onProgress(sowFile.name, 0L, sowFile.length())
                    }
                    val count =
                        DiscImportBridge.extractSowFiles(
                            sowFile.absolutePath,
                            tmpDir.absolutePath,
                            null,
                            appendExisting = true,
                        )
                    if (count < 0) {
                        return@withContext ZipExtractionResult(
                            results,
                            foundAudio,
                            "SOW extraction failed for ${sowFile.name}",
                        )
                    }
                    Log.i("DXX-Setup", "Extracted $count file(s) from nested SOW ${sowFile.name}")
                }
                val extractedFiles = tmpDir.listFiles()?.filter { it.isFile } ?: emptyList()
                for (file in extractedFiles.sortedBy { it.name.lowercase() }) {
                    val lowerName = file.name.lowercase()
                    if (!shouldKeepGameFile(lowerName) || lowerName in existingNames || file.length() <= 1L) continue
                    val sha256 = AssetManifest.computeSha256(file)
                    if (sha256 != null) {
                        results.add(ExtractedFile(lowerName, file, sha256, file.length()))
                        existingNames.add(lowerName)
                        Log.i(
                            "DXX-Setup",
                            "Extracted from nested SOW: $lowerName (${file.length()} bytes, sha256=${sha256.take(
                                16,
                            )}...)",
                        )
                    }
                }
            }
        } catch (e: InsufficientStorageException) {
            Log.e("DXX-Setup", "ZIP extraction ran out of space", e)
            ImportStorageGuard.recordFailure(context.filesDir, "ZIP extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, e.message)
        } catch (e: Exception) {
            Log.e("DXX-Setup", "ZIP extraction failed", e)
            ImportStorageGuard.recordFailure(context.filesDir, "ZIP extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, "ZIP extraction failed: ${e.message}")
        }
        ZipExtractionResult(results, foundAudio)
    }

internal suspend fun matchDemoInstallerPackage(
    context: Context,
    filename: String,
    uri: Uri,
): DemoInstallerPackages.PackageInfo? {
    DemoInstallerPackages.matchByName(filename)?.let { return it }
    val lowerName = filename.lowercase()
    if (!lowerName.endsWith(".zip") &&
        !lowerName.endsWith(".exe") &&
        !lowerName.endsWith(".sit") &&
        !lowerName.endsWith(".hqx")
    ) {
        return null
    }
    val sha256 = computeContentSha256(context, uri) ?: return null
    val matched = DemoInstallerPackages.matchBySha256(sha256) ?: return null
    Log.i("DXX-Setup", "Matched demo installer by hash: $filename -> ${matched.filename}")
    return matched
}

private suspend fun computeContentSha256(
    context: Context,
    uri: Uri,
): String? =
    withContext(Dispatchers.IO) {
        try {
            val digest = java.security.MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(8192)
            context.contentResolver.openInputStream(uri)?.use { input ->
                while (true) {
                    val n = input.read(buffer)
                    if (n <= 0) break
                    digest.update(buffer, 0, n)
                }
            } ?: return@withContext null
            digest.digest().joinToString("") { "%02x".format(it) }
        } catch (e: Exception) {
            Log.w("DXX-Setup", "Failed to hash picked archive $uri", e)
            null
        }
    }

internal suspend fun extractStuffitContents(
    context: Context,
    archiveUri: Uri,
    tmpDir: File,
    archiveName: String? = null,
    onProgress: suspend (String, Long, Long) -> Unit,
): ZipExtractionResult =
    kotlinx.coroutines.withContext(Dispatchers.IO) {
        tmpDir.mkdirs()
        val safeName =
            (archiveName ?: "archive")
                .lowercase()
                .replace(Regex("[^a-z0-9._-]"), "_")
        val workDir = File(tmpDir, ".sit_$safeName")
        if (workDir.exists()) workDir.deleteRecursively()
        workDir.mkdirs()
        val tmpArchive = File(workDir, ".tmp_sit_import")
        val results = mutableListOf<ExtractedFile>()
        val isBinHex = archiveName?.lowercase()?.endsWith(".hqx") == true
        val expectedDemoFiles =
            archiveName
                ?.let { DemoInstallerPackages.matchByName(it) }
                ?.expectedFiles
                ?.toSet()

        fun shouldKeepGameFile(name: String): Boolean =
            if (expectedDemoFiles != null) {
                name in expectedDemoFiles
            } else {
                name in ALL_GAME_FILENAMES
            }

        try {
            ImportStorageGuard.requireFreeSpace(
                workDir,
                ImportStorageGuard.queryUriSizeBytes(context.contentResolver, archiveUri) ?: 0L,
                "stage StuffIt archive",
            )
            if (isBinHex) {
                val total = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, archiveUri) ?: 0L
                kotlinx.coroutines.withContext(Dispatchers.Main) {
                    onProgress("Decoding BinHex archive", 0L, total)
                }
                context.contentResolver.openInputStream(archiveUri)?.use { input ->
                    BinHexDecoder.decodeDataFork(input, tmpArchive)
                } ?: return@withContext ZipExtractionResult(results, false, "Unable to open BinHex archive")
                kotlinx.coroutines.withContext(Dispatchers.Main) {
                    onProgress("Decoding BinHex archive", total, total)
                }
            } else {
                copyUriToFileWithProgress(context, archiveUri, tmpArchive) { copied, total ->
                    kotlinx.coroutines.withContext(Dispatchers.Main) {
                        onProgress("Copying archive", copied, total)
                    }
                }
            }
            kotlinx.coroutines.withContext(Dispatchers.Main) {
                onProgress("Extracting StuffIt archive", 0L, tmpArchive.length())
            }
            val count = DiscImportBridge.extractStuffitFiles(tmpArchive.absolutePath, workDir.absolutePath, null)
            if (count < 0) {
                return@withContext ZipExtractionResult(results, false, "StuffIt extraction failed")
            }
            kotlinx.coroutines.withContext(Dispatchers.Main) {
                onProgress("Extracting StuffIt archive", tmpArchive.length(), tmpArchive.length())
            }
            val extractedFiles = workDir.listFiles()?.filter { it.isFile } ?: emptyList()
            for (file in extractedFiles.sortedBy { it.name.lowercase() }) {
                val lowerName = file.name.lowercase()
                if (!shouldKeepGameFile(lowerName) || file.length() <= 1L) continue
                val sha256 = AssetManifest.computeSha256(file)
                if (sha256 != null) {
                    results.add(ExtractedFile(lowerName, file, sha256, file.length()))
                    Log.i(
                        "DXX-Setup",
                        "Extracted from StuffIt: $lowerName (${file.length()} bytes, sha256=${sha256.take(16)}...)",
                    )
                }
            }
        } catch (e: InsufficientStorageException) {
            Log.e("DXX-Setup", "StuffIt extraction ran out of space", e)
            ImportStorageGuard.recordFailure(context.filesDir, "StuffIt extraction failed", e)
            return@withContext ZipExtractionResult(results, false, e.message)
        } catch (e: Exception) {
            Log.e("DXX-Setup", "StuffIt extraction failed", e)
            ImportStorageGuard.recordFailure(context.filesDir, "StuffIt extraction failed", e)
            return@withContext ZipExtractionResult(results, false, "StuffIt extraction failed: ${e.message}")
        }
        ZipExtractionResult(results, false)
    }

internal suspend fun extract7zContents(
    context: Context,
    archiveUri: Uri,
    tmpDir: File,
    onProgress: suspend (String, Long, Long) -> Unit,
): ZipExtractionResult =
    kotlinx.coroutines.withContext(Dispatchers.IO) {
        tmpDir.mkdirs()
        val results = mutableListOf<ExtractedFile>()
        var foundAudio = false
        val audioExts = setOf("mp3", "ogg", "flac")
        val tmpArchive = File(tmpDir, ".tmp_7z_import")
        try {
            ImportStorageGuard.requireFreeSpace(
                tmpDir,
                ImportStorageGuard.queryUriSizeBytes(context.contentResolver, archiveUri) ?: 0L,
                "stage 7z archive",
            )
            copyUriToFileWithProgress(context, archiveUri, tmpArchive) { copied, total ->
                kotlinx.coroutines.withContext(Dispatchers.Main) {
                    onProgress("Copying archive", copied, total)
                }
            }
            SevenZFile.builder().setFile(tmpArchive).get().use { szf ->
                var entry = szf.nextEntry
                while (entry != null) {
                    val name = entry.name.substringAfterLast('/').lowercase()
                    if (!entry.isDirectory && !foundAudio) {
                        val ext = name.substringAfterLast('.', "")
                        if (ext in audioExts) foundAudio = true
                    }
                    if (!entry.isDirectory && name in ALL_GAME_FILENAMES) {
                        val totalBytes = entry.size.takeIf { it > 0L } ?: 0L
                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                            onProgress(name, 0L, totalBytes)
                        }
                        ImportStorageGuard.requireFreeSpace(
                            tmpDir,
                            totalBytes,
                            "extract $name",
                        )
                        val tmpFile = File(tmpDir, name)
                        val digest = java.security.MessageDigest.getInstance("SHA-256")
                        var size = 0L
                        var lastReported = 0L
                        FileOutputStream(tmpFile).use { out ->
                            val buf = ByteArray(8192)
                            while (true) {
                                val n = szf.read(buf)
                                if (n <= 0) break
                                out.write(buf, 0, n)
                                digest.update(buf, 0, n)
                                size += n
                                if (size - lastReported >= 1024L * 1024L || size == totalBytes) {
                                    lastReported = size
                                    kotlinx.coroutines.withContext(Dispatchers.Main) {
                                        onProgress(name, size, totalBytes)
                                    }
                                }
                            }
                        }
                        kotlinx.coroutines.withContext(Dispatchers.Main) {
                            onProgress(name, size, totalBytes)
                        }
                        val sha256 = digest.digest().joinToString("") { "%02x".format(it) }
                        results.add(ExtractedFile(name, tmpFile, sha256, size))
                        Log.i("DXX-Setup", "Extracted from 7z: $name ($size bytes, sha256=${sha256.take(16)}...)")
                    }
                    entry = szf.nextEntry
                }
            }
        } catch (e: InsufficientStorageException) {
            Log.e("DXX-Setup", "7z extraction ran out of space", e)
            ImportStorageGuard.recordFailure(context.filesDir, "7z extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, e.message)
        } catch (e: Exception) {
            Log.e("DXX-Setup", "7z extraction failed", e)
            ImportStorageGuard.recordFailure(context.filesDir, "7z extraction failed", e)
            return@withContext ZipExtractionResult(results, foundAudio, "7z extraction failed: ${e.message}")
        } finally {
            tmpArchive.delete()
        }
        ZipExtractionResult(results, foundAudio)
    }

internal fun cleanupTmpDir(filesDir: File) {
    val tmpDir = File(filesDir, "tmp")
    if (tmpDir.exists()) tmpDir.deleteRecursively()
}

internal fun cleanupTmpDirWithReport(filesDir: File): List<String> {
    val tmpDir = File(filesDir, "tmp")
    val leftovers = tmpDir.listFiles()?.map { it.name } ?: emptyList()
    if (leftovers.isNotEmpty()) tmpDir.deleteRecursively()
    return leftovers
}

internal suspend fun copyUriToFileWithProgress(
    context: Context,
    uri: Uri,
    dest: File,
    onProgress: suspend (Long, Long) -> Unit = { _, _ -> },
) {
    val totalBytes = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri) ?: 0L
    var copiedBytes = 0L
    var lastReported = 0L
    onProgress(0L, totalBytes)
    context.contentResolver.openInputStream(uri)?.use { input ->
        FileOutputStream(dest).use { output ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                val n = input.read(buffer)
                if (n <= 0) break
                output.write(buffer, 0, n)
                copiedBytes += n.toLong()
                if (copiedBytes - lastReported >= 1024L * 1024L || copiedBytes == totalBytes) {
                    lastReported = copiedBytes
                    onProgress(copiedBytes, totalBytes)
                }
            }
        }
    } ?: throw java.io.IOException("Could not open selected file")
    onProgress(copiedBytes, totalBytes)
}
