package com.dxxredux.app

import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import okhttp3.MediaType
import okhttp3.OkHttpClient
import okhttp3.Protocol
import okhttp3.Response
import okhttp3.ResponseBody
import okhttp3.ResponseBody.Companion.toResponseBody
import okio.Buffer
import okio.BufferedSource
import okio.Source
import okio.Timeout
import okio.buffer
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.io.IOException
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class SoundfontDownloadTest {
    @get:Rule val temporary = TemporaryFolder()
    private val preferences = memoryPreferences()
    private val entry = SoundfontDownload(
        "Example bank", "https://github.com/example/banks/releases/download/v1/bank.sf2",
        "Example description", "https://github.com/original/bank", "MIT license text",
    )

    private fun downloader(body: () -> ResponseBody, code: Int = 200): SoundfontDownloader =
        SoundfontDownloader(OkHttpClient.Builder().addInterceptor { chain ->
            assertEquals(entry.url, chain.request().url.toString())
            Response.Builder().request(chain.request()).protocol(Protocol.HTTP_1_1)
                .code(code).message("Test response").body(body()).build()
        }.build())

    @Test fun catalogUsesReleaseAssetsAndDescriptorsRequireCompleteHttpsMetadata() {
        val catalog = SoundfontCatalog.entries
        assertTrue(catalog.isNotEmpty())
        assertEquals(catalog.size, catalog.map { it.name }.distinct().size)
        for (font in catalog) {
            assertTrue(font.url.startsWith("https://github.com/"))
            assertTrue(font.url.contains("/releases/download/"))
            assertTrue(font.url.endsWith(".sf2"))
            assertEquals(font, SoundfontDownload.fromJson(font.toJson()))
        }
        for (invalid in listOf<() -> SoundfontDownload>(
            { entry.copy(url = "http://example.com/bank.sf2") },
            { entry.copy(websiteUrl = "file:///bank.txt") },
            { entry.copy(license = " ") },
            { entry.copy(description = "") },
        )) {
            try {
                invalid()
                fail("Invalid catalog entry accepted")
            } catch (_: IllegalArgumentException) {
            }
        }
    }

    @Test fun downloadImportsDeduplicatesAndLeavesPreferencesUntilActivation() = runBlocking {
        val store = SoundfontStore(temporary.root, preferences)
        val bytes = byteArrayOf(1, 2, 3, 4)
        var required = 0L
        var progress = 0L
        val downloader = downloader({ bytes.toResponseBody() })
        val font = downloader.download(entry, store, { required = it }, { it.readBytes().contentEquals(bytes) }, { n, _ -> progress = n })
        assertEquals(4L, required)
        assertEquals(4L, progress)
        assertEquals("", store.read().selected)
        assertEquals("ymfm", store.read().renderer)
        assertEquals(entry.name, font.name)
        assertEquals(font, downloader.download(entry, store, {}, { true }, { _, _ -> }))
        store.select(font.id) { File(it).readBytes().contentEquals(bytes) }
        val reopened = SoundfontStore(temporary.root, preferences)
        assertEquals(listOf(font), reopened.read().fonts)
        assertEquals(entry, reopened.read().fonts.single().download)
        assertEquals(font.id, reopened.read().selected)
        assertEquals("ymfm", reopened.read().renderer)
    }

    @Test fun downloadMetadataSurvivesLocalReimportAndPreferenceResets() = runBlocking {
        val store = SoundfontStore(temporary.root, preferences)
        val bytes = byteArrayOf(3, 6, 9)
        val local = store.import(bytes.inputStream(), "Local name") { true }
        val font = downloader({ bytes.toResponseBody() }).download(entry, store, {}, { true }, { _, _ -> })
        assertEquals(local.id, font.id)
        assertEquals(entry, font.download)
        assertEquals(font, store.import(bytes.inputStream(), "Reimport") { true })
        store.select(font.id) { true }
        for (preset in GameSettingsPreset.entries) {
            preset.resetMidiPreferences(store) { _, _ -> true }
            assertEquals(entry, SoundfontStore(temporary.root, preferences).read().fonts.single().download)
        }
        // The retained information does not require a matching catalog entry
        assertFalse(SoundfontCatalog.entries.any { it.url == entry.url })
    }

    @Test fun rejectedDownloadsPreserveSelectedAssetAndCleanTemporaryFiles() = runBlocking {
        val store = SoundfontStore(temporary.root, preferences)
        val old = store.import(byteArrayOf(7).inputStream(), "Existing") { true }
        store.select(old.id) { true }
        val cases = listOf(
            downloader({ "Not found".toResponseBody() }, 404),
            downloader({ "<html>Not an SF2 release asset</html>".toResponseBody() }),
            downloader({ generatedBody(8, 12) }), // Truncated declared response
            downloader({ generatedBody(1, SoundfontStore.MAX_BYTES + 1) }),
            downloader({ generatedBody(SoundfontStore.MAX_BYTES + 1, -1) }),
        )
        for ((index, candidate) in cases.withIndex()) {
            var validated = false
            try {
                candidate.download(entry, store, {}, {
                    validated = true
                    index != 1
                }, { _, _ -> })
                fail("Invalid download accepted")
            } catch (_: IOException) {
            }
            assertEquals("Only the HTML case should reach SF2 validation", index == 1, validated)
            assertEquals(listOf(old), store.read().fonts)
            assertEquals(old.id, store.read().selected)
            assertFalse(File(temporary.root, "soundfonts").listFiles()!!.any { it.name.startsWith("import-") })
        }
    }

    @Test fun cancellationKeepsSelectionResponsiveAndDiscardsPartialDownload() = runBlocking {
        val store = SoundfontStore(temporary.root, preferences)
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val closed = CountDownLatch(1)
        val executor = Executors.newSingleThreadExecutor()
        val body = object : ResponseBody() {
            private val stream = object : Source {
                override fun timeout(): Timeout = Timeout.NONE
                override fun close() { closed.countDown() }
                override fun read(sink: Buffer, byteCount: Long): Long {
                    entered.countDown()
                    check(release.await(5, TimeUnit.SECONDS))
                    return -1
                }
            }.buffer()
            override fun contentType(): MediaType? = null
            override fun contentLength(): Long = -1
            override fun source(): BufferedSource = stream
        }
        val job = launch(start = CoroutineStart.UNDISPATCHED) {
            downloader({ body }).download(entry, store, {}, { true }, { _, _ -> })
            fail("Cancelled download completed")
        }
        try {
            assertTrue(entered.await(5, TimeUnit.SECONDS))
            // A stalled network read must not hold the store's preference lock
            assertEquals("ymfm", executor.submit<String> { store.read().renderer }.get(2, TimeUnit.SECONDS))
            job.cancelAndJoin()
        } finally {
            release.countDown()
            executor.shutdownNow()
        }
        assertTrue(closed.await(5, TimeUnit.SECONDS))
        assertTrue(store.read().fonts.isEmpty())
        assertEquals("", store.read().selected)
        assertFalse(File(temporary.root, "soundfonts").listFiles()!!.any { it.name.startsWith("import-") })
    }

    private fun generatedBody(bytes: Long, declaredLength: Long): ResponseBody = object : ResponseBody() {
        private var remaining = bytes
        private val stream = object : Source {
            override fun timeout(): Timeout = Timeout.NONE
            override fun close() {}
            override fun read(sink: Buffer, byteCount: Long): Long {
                if (remaining == 0L) return -1
                val n = minOf(remaining, byteCount, 8192).toInt()
                sink.write(ByteArray(n))
                remaining -= n
                return n.toLong()
            }
        }.buffer()
        override fun contentType(): MediaType? = null
        override fun contentLength(): Long = declaredLength
        override fun source(): BufferedSource = stream
    }
}
