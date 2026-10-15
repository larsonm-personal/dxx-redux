package com.dxxredux.app

import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.ByteArrayOutputStream
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class PackageDocumentsTest {
    @get:Rule val temporary = TemporaryFolder()

    private fun zip(vararg entries: Pair<String, String>): ByteArray =
        ByteArrayOutputStream()
            .also { bytes ->
                ZipOutputStream(bytes).use { zip ->
                    entries.forEach { (path, text) ->
                        zip.putNextEntry(ZipEntry(path))
                        zip.write(text.toByteArray())
                        zip.closeEntry()
                    }
                }
            }.toByteArray()

    @Test fun metadataDiscoversDocsWithoutAMissionAndPreservesOriginalPaths() {
        val root = temporary.newFolder()
        val mods = File(root, "mods").apply { mkdirs() }
        val archive =
            File(mods, "pack.dxa").apply {
                writeBytes(
                    zip(
                        "docs/README" to "Original text",
                        "README.dxx-redux.md" to "# Redux notes",
                        "author/original.RTF" to "{\\rtf1 original}",
                        "other/README" to "Second original",
                        "sound.raw" to "asset",
                    ),
                )
            }
        val mod = ModManager.ModInfo(archive.name, "Pack", false, 0, archive.length(), "d1", 0)
        val details = ModManager(root).getModDetails(mod, root)
        assertNull(details.missionZip)
        assertEquals(
            listOf(
                "docs/README",
                "other/README",
                "README.dxx-redux.md",
                "author/original.RTF",
            ),
            details.documents.map {
                it.path
            },
        )
        assertEquals("# Redux notes", PackageDocuments.readTextFile(archive, "README.dxx-redux.md").text)
        assertEquals("Original text", PackageDocuments.readTextFile(archive, "docs/README").text)
        assertTrue(details.documents.single { it.isReduxNotes }.isReduxNotes)
        assertEquals("application/rtf", PackageDocuments.externalViewMimeType("original.RTF"))
    }

    @Test fun sharesMissionPreferenceAndRetainsAllDocuments() {
        fun order(vararg names: String) =
            PackageDocuments
                .order(
                    names.mapIndexed { i, name ->
                        PackageDocument(
                            name,
                            (
                                i +
                                    1
                            ).toLong(),
                        )
                    },
                    "pack",
                ).map { it.path }
        assertEquals(
            listOf("README.md", "pack.txt", "z.txt", "README.pdf"),
            order("z.txt", "README.pdf", "README.md", "pack.txt"),
        )
        assertEquals("pack.txt", order("larger.txt", "pack.txt", "largest.txt").first())
        assertEquals("largest.txt", order("small.txt", "largest.txt").first())
        assertEquals(listOf("README.pdf", "z.rtf"), order("z.rtf", "README.pdf"))
        assertTrue(order("texture.tga", "not-a-readme", "docs.zip").isEmpty())
    }

    @Test fun rejectsInvalidBundlesBeforePublication() {
        val archive = temporary.newFile("docs.zip")
        val invalid =
            listOf(
                zip("../README.md" to "escape"),
                zip("/README.md" to "absolute"),
                zip("C:/README.md" to "drive"),
                zip("Readme.txt" to "one", "README.TXT" to "two"),
                zip("sound.raw" to "not docs"),
                zip(),
                zip(*(0..RecommendedModDocuments.MAX_ENTRIES).map { "doc$it.txt" to "x" }.toTypedArray()),
                byteArrayOf(1, 2, 3),
            )
        invalid.forEach { bytes ->
            archive.writeBytes(bytes)
            assertThrows(Exception::class.java) { RecommendedModDocuments.validate(archive, "pack") }
        }
        archive.writeBytes(zip("readme.md" to "a".repeat(2_000_000)))
        assertThrows(Exception::class.java) { RecommendedModDocuments.validate(archive, "pack") }
    }

    @Test fun archiveAndExtractedTextHaveTheSameTruncationAndEncoding() {
        val archive = temporary.newFile("text.zip").apply { writeBytes(zip("README" to "Original author text")) }
        val extracted = temporary.newFile("README").apply { writeText("Original author text") }
        val inArchive = PackageDocuments.readTextFile(archive, "README", 8)
        val onDisk = PackageDocuments.readExtractedDocument(extracted, "README", 8)
        assertEquals(inArchive, onDisk)
        assertEquals("Original", onDisk.text)
        assertTrue(onDisk.truncated)
        assertNull(onDisk.problem)
    }

    private class Response(
        url: String,
        private val body: ByteArray,
        private val status: Int,
    ) : HttpURLConnection(URL(url)) {
        var closed = false

        override fun connect() {}

        override fun disconnect() {
            closed = true
        }

        override fun usingProxy() = false

        override fun getResponseCode() = status

        override fun getHeaderField(name: String): String? =
            if (name ==
                "Content-Length"
            ) {
                body.size.toString()
            } else {
                null
            }

        override fun getInputStream() = body.inputStream()
    }

    @Test fun refreshesStableUrlsAndFallsBackOnlyToValidatedSavedDocs() =
        runBlocking {
            val cache = temporary.newFolder()
            var body = zip("README.md" to "first")
            var status = 200
            val requests = mutableListOf<Response>()
            val store = RecommendedModDocuments(cache) { url -> Response(url, body, status).also { requests += it } }
            val url = "https://example.com/release/pack.dxa"
            val first = store.load(url, "pack.dxa")
            assertNull(first.warning)
            assertEquals("$url.readme.zip", requests.single().url.toString())
            body = zip("README.md" to "updated", "original.rtf" to "original")
            val updated = store.load(url, "pack.dxa")
            assertEquals(2, updated.documents.size)
            assertEquals("updated", PackageDocuments.readTextFile(updated.archive, "README.md").text)
            body = byteArrayOf(0)
            val saved = store.load(url, "pack.dxa")
            assertNotNull(saved.warning)
            assertEquals("updated", PackageDocuments.readTextFile(saved.archive, "README.md").text)
            status = 404
            assertNotNull(store.load(url, "pack.dxa").warning)
            try {
                store.load("https://example.com/different-release/pack.dxa", "pack.dxa")
                fail("A different URL must not reuse the saved copy")
            } catch (expected: java.io.IOException) {
                assertTrue(expected.message!!.contains("not available"))
            }
            assertTrue(requests.all { it.closed })
            assertTrue(cache.listFiles()!!.none { it.extension == "part" })
        }

    @Test fun derivesCompanionFromCanonicalAssetUrl() {
        assertEquals(
            "https://example.com/v1/pack%20one.dxa.readme.zip",
            RecommendedModDocuments.documentationUrl("https://example.com/v1/pack%20one.dxa"),
        )
        assertThrows(IllegalArgumentException::class.java) {
            RecommendedModDocuments.documentationUrl("https://example.com/a?token=x")
        }
        assertThrows(
            IllegalArgumentException::class.java,
        ) { RecommendedModDocuments.documentationUrl("http://example.com/a") }
    }
}
