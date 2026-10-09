package com.dxxredux.app

import android.provider.DocumentsContract
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ImportTreeScannerTest {
    @Test
    fun archiveCandidatesShareSupportedAdmission() {
        for (extension in listOf("zip", "7z", "rar", "sit", "hqx")) {
            for (suffix in listOf(
                extension,
                extension.uppercase(java.util.Locale.ROOT),
                extension.replaceFirstChar(Char::uppercase),
            )) {
                val name = "bundle.$suffix"
                assertTrue(name, GameFileFormats.isImportArchive(name))
                assertTrue(name, isDirectoryImportCandidateName(name, emptySet()))
            }
        }
        for (name in listOf(
            "bundle.tar",
            "bundle.gz",
            "bundle.zip.txt",
            "bundle.sitx",
            "bundle.hqx2",
            "notes.txt",
            "bundle",
        )) {
            assertEquals(name, false, GameFileFormats.isImportArchive(name))
            assertEquals(name, false, isDirectoryImportCandidateName(name, emptySet()))
        }
        for (name in listOf(
            "disc.cue",
            "disc.iso",
            "disc.bin",
            "setup.exe",
            "setup.pkg",
            "disc.sow",
            "mod.dxa (1)",
            "descent.hog",
            "disc.gog",
            "disc.inst",
        )) {
            assertEquals(name, false, GameFileFormats.isImportArchive(name))
            assertTrue(name, isDirectoryImportCandidateName(name, emptySet()))
        }
    }

    @Test
    fun traversesAllSupportedArchiveNames() =
        runBlocking {
            val names =
                listOf("bundle.zip", "bundle.7z", "bundle.rar", "bundle.sit", "bundle.hqx")
                    .flatMap { listOf(it, it.uppercase(java.util.Locale.ROOT)) }
            val rows =
                names.mapIndexed {
                    index,
                    name,
                    ->
                    ImportTreeRow("archive-$index", name, "application/octet-stream")
                }
            val queried = mutableListOf<String>()
            val result =
                traverseImportTree("root", emptySet()) { documentId, _ ->
                    queried.add(documentId)
                    if (documentId == "root") {
                        listOf(ImportTreeRow("archives", "archives", DocumentsContract.Document.MIME_TYPE_DIR))
                    } else {
                        rows + ImportTreeRow("notes", "notes.txt", "text/plain")
                    }
                }
            assertEquals(listOf("root", "archives"), queried)
            assertEquals(rows.map { it.documentId }, result.importableDocumentIds)
            assertEquals(11, result.scannedFileCount)
            assertEquals(1, result.skippedUnknownFileCount)
        }

    @Test
    fun acceptsArbitraryDirectMissionDataFilenames() {
        assertEquals(true, isDirectGameDataImportName("D2X.MN2", emptySet()))
        assertEquals(true, isDirectGameDataImportName("PANIC.HOG", emptySet()))
        assertEquals(true, isDirectGameDataImportName("PANIC.MN2", emptySet()))
        assertEquals(false, isDirectGameDataImportName("README.TXT", emptySet()))
    }

    @Test
    fun classifiesCueBinIsoGogSowAndAudioRows() {
        val result =
            classifyImportTreeRows(
                rows =
                    listOf(
                        ImportTreeRow(
                            documentId = "dir-1",
                            displayName = "disc-folder",
                            mimeType = DocumentsContract.Document.MIME_TYPE_DIR,
                        ),
                        ImportTreeRow("cue-1", "descent2.cue", "application/octet-stream"),
                        ImportTreeRow("bin-1", "descent2.bin", "application/octet-stream"),
                        ImportTreeRow("iso-1", "descent2.iso", "application/octet-stream"),
                        ImportTreeRow("gog-installer", "setup.exe", "application/octet-stream"),
                        ImportTreeRow("sow-1", "descent.sow", "application/octet-stream"),
                        ImportTreeRow("mod-1", "uud1sp.dxa (1)", "application/octet-stream"),
                        ImportTreeRow("audio-bin", "descent_ii.gog", "application/octet-stream"),
                        ImportTreeRow("audio-cue", "descent_ii.inst", "application/octet-stream"),
                        ImportTreeRow("not-mod", "not_a_mod.dxarchive", "application/octet-stream"),
                        ImportTreeRow("ignored", "notes.txt", "text/plain"),
                        ImportTreeRow("nameless", null, "application/octet-stream"),
                    ),
                allGameFileNames = emptySet(),
            )

        assertEquals(listOf("dir-1"), result.childDirectoryIds)
        assertEquals(
            listOf("cue-1", "bin-1", "iso-1", "gog-installer", "sow-1", "mod-1", "audio-bin", "audio-cue"),
            result.importableDocumentIds,
        )
        assertEquals(10, result.scannedFileCount)
        assertEquals(2, result.skippedUnknownFileCount)
    }

    @Test
    fun keepsConfiguredExactGameNamesWithoutCommonImportExtensions() {
        val result =
            classifyImportTreeRows(
                rows = listOf(ImportTreeRow("palette-1", "groupa.256", "application/octet-stream")),
                allGameFileNames = setOf("groupa.256"),
            )

        assertEquals(listOf("palette-1"), result.importableDocumentIds)
        assertEquals(1, result.scannedFileCount)
        assertEquals(0, result.skippedUnknownFileCount)
    }

    @Test
    fun warnsOnlyWhenDirectoryScanCrossesThreshold() {
        assertNull(largeDirectoryImportWarning(LARGE_IMPORT_DIRECTORY_FILE_COUNT, 50))
        assertEquals(
            "Scanned 201 files and skipped 150 unrelated entries",
            largeDirectoryImportWarning(201, 150),
        )
        assertEquals(
            "Scanned 201 files in selected folder",
            largeDirectoryImportWarning(201, 0),
        )
    }

    @Test
    fun traversesFiniteTreeBreadthFirstAndClassifiesFiles() =
        runBlocking {
            val queriedDirectories = mutableListOf<String>()
            val children =
                mapOf(
                    "root" to
                        listOf(
                            ImportTreeRow("dir-a", "A", DocumentsContract.Document.MIME_TYPE_DIR),
                            ImportTreeRow("root-hog", "root.hog", "application/octet-stream"),
                            ImportTreeRow("notes", "notes.txt", "text/plain"),
                        ),
                    "dir-a" to listOf(ImportTreeRow("child-mn2", "child.mn2", "application/octet-stream")),
                )

            val result =
                traverseImportTree("root", emptySet()) { documentId, _ ->
                    queriedDirectories.add(documentId)
                    children[documentId].orEmpty()
                }

            assertEquals(listOf("root", "dir-a"), queriedDirectories)
            assertEquals(listOf("root-hog", "child-mn2"), result.importableDocumentIds)
            assertEquals(3, result.scannedFileCount)
            assertEquals(1, result.skippedUnknownFileCount)
        }

    @Test
    fun rejectsCyclesAndRepeatedDocumentIdentities() =
        runBlocking {
            val failure =
                runCatching {
                    traverseImportTree("root", emptySet()) { documentId, _ ->
                        when (documentId) {
                            "root" -> listOf(ImportTreeRow("child", "child", DocumentsContract.Document.MIME_TYPE_DIR))
                            else -> listOf(ImportTreeRow("root", "root", DocumentsContract.Document.MIME_TYPE_DIR))
                        }
                    }
                }.exceptionOrNull()

            assertTrue(failure is ImportTreeScanException)
            assertTrue(failure?.message?.contains("repeated document ID") == true)
        }

    @Test
    fun enforcesTraversalBudgetsAtTheBoundary() =
        runBlocking {
            val limits = ImportTreeScanLimits(maxDepth = 1, maxDirectories = 2, maxRows = 2, maxResults = 1)
            val accepted =
                traverseImportTree("root", emptySet(), limits) { _, remainingRows ->
                    assertEquals(2, remainingRows)
                    listOf(ImportTreeRow("one", "one.hog", "application/octet-stream"))
                }
            assertEquals(listOf("one"), accepted.importableDocumentIds)

            val failure =
                runCatching {
                    traverseImportTree("root", emptySet(), limits) { _, _ ->
                        listOf(
                            ImportTreeRow("one", "one.hog", "application/octet-stream"),
                            ImportTreeRow("two", "two.mn2", "application/octet-stream"),
                        )
                    }
                }.exceptionOrNull()
            assertTrue(failure is ImportTreeScanException)
            assertTrue(failure?.message?.contains("1-file import limit") == true)

            val rowFailure =
                runCatching {
                    traverseImportTree("root", emptySet(), limits) { _, remainingRows ->
                        List(remainingRows + 1) { index ->
                            ImportTreeRow("row-$index", "row-$index.txt", "text/plain")
                        }
                    }
                }.exceptionOrNull()
            assertTrue(rowFailure is ImportTreeScanException)
            assertTrue(rowFailure?.message?.contains("2-row scan limit") == true)

            val depthFailure =
                runCatching {
                    traverseImportTree("root", emptySet(), limits.copy(maxDepth = 0)) { _, _ ->
                        listOf(ImportTreeRow("child", "child", DocumentsContract.Document.MIME_TYPE_DIR))
                    }
                }.exceptionOrNull()
            assertTrue(depthFailure is ImportTreeScanException)
            assertTrue(depthFailure?.message?.contains("0-level depth limit") == true)

            val directoryFailure =
                runCatching {
                    traverseImportTree("root", emptySet(), limits.copy(maxDirectories = 1)) { _, _ ->
                        listOf(ImportTreeRow("child", "child", DocumentsContract.Document.MIME_TYPE_DIR))
                    }
                }.exceptionOrNull()
            assertTrue(directoryFailure is ImportTreeScanException)
            assertTrue(directoryFailure?.message?.contains("1-directory scan limit") == true)
        }

    @Test
    fun propagatesCancellationFromTraversal() =
        runBlocking {
            val scan =
                async {
                    traverseImportTree("root", emptySet()) { _, _ ->
                        awaitCancellation()
                    }
                }

            withTimeout(1_000) {
                scan.cancelAndJoin()
            }
            assertTrue(scan.isCancelled)
        }
}
