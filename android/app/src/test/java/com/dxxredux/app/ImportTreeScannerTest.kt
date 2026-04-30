package com.dxxredux.app

import android.provider.DocumentsContract
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ImportTreeScannerTest {
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
                        ImportTreeRow("audio-bin", "descent_ii.gog", "application/octet-stream"),
                        ImportTreeRow("audio-cue", "descent_ii.inst", "application/octet-stream"),
                        ImportTreeRow("ignored", "notes.txt", "text/plain"),
                        ImportTreeRow("nameless", null, "application/octet-stream"),
                    ),
                allGameFileNames = emptySet(),
            )

        assertEquals(listOf("dir-1"), result.childDirectoryIds)
        assertEquals(
            listOf("cue-1", "bin-1", "iso-1", "gog-installer", "sow-1", "audio-bin", "audio-cue"),
            result.importableDocumentIds,
        )
        assertEquals(8, result.scannedFileCount)
        assertEquals(1, result.skippedUnknownFileCount)
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
}