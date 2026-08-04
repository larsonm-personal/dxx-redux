package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class SafManifestTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun writePublishesCompleteManifestAndReadsEveryJsonEscape() {
        val directory = temporaryFolder.newFolder("manifest")
        val file = File(directory, SafManifest.FILENAME)
        val manifest = SafManifest(file)
        val expected =
            listOf(
                SafManifest.SafFileEntry(
                    filename = "quotes-\"-slash-\\-line-\n-\u2603.hog",
                    contentUri = "content://provider/tree/a%2Fb",
                    sizeBytes = Long.MAX_VALUE,
                ),
            )

        manifest.write(emptyList())
        assertEquals(emptyList<SafManifest.SafFileEntry>(), manifest.read())

        manifest.write(expected)

        assertEquals(expected, manifest.read())
        assertFalse(
            directory.listFiles().orEmpty().any { it.name != SafManifest.FILENAME },
        )
    }

    @Test
    fun readRejectsCompletePrefixWhenDocumentIsTruncated() {
        val file = temporaryFolder.newFile(SafManifest.FILENAME)
        file.writeText(
            """{"files":[{"filename":"first.hog","content_uri":"content://first","size_bytes":1}""",
        )

        assertEquals(emptyList<SafManifest.SafFileEntry>(), SafManifest(file).read())
    }

    @Test
    fun readRejectsWrongFieldsAndAmbiguousFilenames() {
        val file = temporaryFolder.newFile(SafManifest.FILENAME)
        file.writeText(
            """{"files":[{"filename":"same.hog","content_uri":"content://first","size_bytes":1},{"filename":"SAME.HOG","content_uri":"content://second","size_bytes":2}]}""",
        )

        assertEquals(emptyList<SafManifest.SafFileEntry>(), SafManifest(file).read())
    }

    @Test
    fun writeRejectsInvalidEntriesWithoutReplacingCurrentGeneration() {
        val file = temporaryFolder.newFile(SafManifest.FILENAME)
        val original = """{"files":[]}"""
        file.writeText(original)

        assertThrows(IllegalArgumentException::class.java) {
            SafManifest(file).write(
                listOf(SafManifest.SafFileEntry("broken.hog", "content://broken", -1)),
            )
        }

        assertEquals(original, file.readText())
    }

    @Test
    fun interruptedPublicationPreservesPreviousCompleteGeneration() {
        val file = temporaryFolder.newFile(SafManifest.FILENAME)
        val original = """{"files":[]}"""
        file.writeText(original)

        assertThrows(IllegalStateException::class.java) {
            AtomicFilePublication.writeUtf8(file, """{"files":[{"filename":"partial"}]}""") { _, _ ->
                error("injected interruption")
            }
        }

        assertEquals(original, file.readText())
        assertFalse(
            file.parentFile
                ?.listFiles()
                .orEmpty()
                .any { it.name != SafManifest.FILENAME },
        )
    }
}
