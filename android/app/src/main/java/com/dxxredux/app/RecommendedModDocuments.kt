package com.dxxredux.app

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.runInterruptible
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URI
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.CRC32
import java.util.zip.ZipFile

/** Published docs are separate from installed mods; never import or activate a bundle */
internal class RecommendedModDocuments(
    private val cacheDir: File,
    private val connect: (String) -> HttpURLConnection = { URI(it).toURL().openConnection() as HttpURLConnection },
) {
    data class Loaded(
        val archive: File,
        val documents: List<PackageDocument>,
        val warning: String? = null,
    )

    companion object {
        // Keep these bounds aligned with mod_documentation.py
        const val MAX_BUNDLE_BYTES = 16L * 1024 * 1024
        const val MAX_EXPANDED_BYTES = 32L * 1024 * 1024
        const val MAX_ENTRIES = 128

        fun documentationUrl(downloadUrl: String): String {
            val uri = URI(downloadUrl)
            require(uri.scheme == "https" && uri.host != null && uri.rawQuery == null && uri.rawFragment == null)
            require(uri.rawPath.isNotEmpty() && !uri.rawPath.endsWith('/'))
            return "$downloadUrl.readme.zip"
        }

        fun validate(
            file: File,
            packageStem: String,
        ): List<PackageDocument> {
            require(file.length() <= MAX_BUNDLE_BYTES) { "Documentation download is too large" }
            val budget = ExtractionBudget(MAX_EXPANDED_BYTES, MAX_EXPANDED_BYTES, MAX_ENTRIES)
            val paths = mutableSetOf<String>()
            val documents = mutableListOf<PackageDocument>()
            ZipFile(file).use { zip ->
                for (entry in zip.entries().asSequence()) {
                    val path = entry.name.replace('\\', '/')
                    val parts = path.trimEnd('/').split('/')
                    require(
                        parts.none {
                            it.isEmpty() || it == "." || it == ".." || ':' in it ||
                                it.any(Char::isISOControl)
                        },
                    ) {
                        "Unsafe documentation path"
                    }
                    require(paths.add(path.trimEnd('/').lowercase(Locale.US))) { "Duplicate documentation path" }
                    budget.registerEntry(entry.size, entry.compressedSize, path)
                    if (entry.isDirectory) continue
                    require(PackageDocuments.isReadmeCandidate(path)) { "Bundle contains a non-document file" }
                    val crc = CRC32()
                    var total = 0L
                    zip.getInputStream(entry).use { input ->
                        val buffer = ByteArray(8192)
                        while (true) {
                            val count = input.read(buffer)
                            if (count < 0) break
                            total += count
                            budget.accountActual(count, total, entry.compressedSize, path)
                            crc.update(buffer, 0, count)
                        }
                    }
                    require(total == entry.size && crc.value == entry.crc) { "Damaged documentation file" }
                    documents += PackageDocument(entry.name, total)
                }
            }
            require(documents.isNotEmpty()) { "No documents were published for this mod" }
            return PackageDocuments.order(documents, packageStem)
        }
    }

    suspend fun load(
        downloadUrl: String,
        packageName: String,
    ): Loaded =
        withContext(Dispatchers.IO) {
            val url = documentationUrl(downloadUrl)
            val key =
                MessageDigest
                    .getInstance(
                        "SHA-256",
                    ).digest(url.toByteArray())
                    .joinToString("") { "%02x".format(it) }
            check(cacheDir.isDirectory || cacheDir.mkdirs()) { "Could not create documentation cache" }
            val cached = File(cacheDir, "$key.zip")
            val temporary = File.createTempFile("$key-", ".part", cacheDir)
            val stem = packageName.substringBeforeLast('.')
            try {
                // Re-fetch the small bundle each time a new docs flow opens: stable release assets can change
                runInterruptible {
                    val connection = connect(url)
                    connection.connectTimeout = 10_000
                    connection.readTimeout = 10_000
                    connection.useCaches = false
                    try {
                        when (connection.responseCode) {
                            200 -> Unit

                            404 -> throw IOException("Published documentation is not available for this mod")

                            else -> throw IOException(
                                "Could not download documentation (HTTP ${connection.responseCode})",
                            )
                        }
                        require(
                            (connection.getHeaderField("Content-Length")?.toLongOrNull() ?: -1L) <= MAX_BUNDLE_BYTES,
                        ) { "Documentation download is too large" }
                        connection.inputStream.use { input ->
                            temporary.outputStream().use { output ->
                                val budget = ExtractionBudget(MAX_BUNDLE_BYTES, MAX_BUNDLE_BYTES)
                                val buffer = ByteArray(8192)
                                var total = 0L
                                while (true) {
                                    val count = input.read(buffer)
                                    if (count < 0) break
                                    total += count
                                    budget.accountActual(count, total, -1, "Documentation download")
                                    output.write(buffer, 0, count)
                                }
                            }
                        }
                    } finally {
                        connection.disconnect()
                    }
                }
                val documents = runInterruptible { validate(temporary, stem) }
                currentCoroutineContext().ensureActive()
                AtomicFilePublication.publishFile(temporary, cached)
                Loaded(cached, documents)
            } catch (error: Exception) {
                currentCoroutineContext().ensureActive()
                val saved = runCatching { validate(cached, stem) }.getOrNull() ?: throw error
                Loaded(cached, saved, "Saved copy: could not check for updates. ${error.message.orEmpty()}")
            } finally {
                temporary.delete()
            }
        }
}
