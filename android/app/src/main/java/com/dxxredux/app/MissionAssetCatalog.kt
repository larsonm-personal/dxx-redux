package com.dxxredux.app

import java.io.FilterInputStream
import java.io.IOException
import java.io.InputStream
import java.util.Locale
import java.util.zip.ZipInputStream

internal object MissionAssetCatalog {
    fun read(
        role: String,
        input: InputStream,
        label: String,
    ): Set<String>? =
        runCatching {
            when (role) {
                GameFileFormats.MISSION_ZIP_HOG -> readHog(input, label)
                GameFileFormats.MISSION_ZIP_MOD_ARCHIVE -> readZip(input, label)
                else -> null
            }
        }.getOrNull()

    private fun readHog(
        input: InputStream,
        label: String,
    ): Set<String>? {
        val entries = linkedSetOf<String>()
        NativeHogArchive.visit(input, label) { entry, _ -> entries += NativeHogArchive.key(entry.name) }
        return entries
    }

    private fun readZip(
        input: InputStream,
        label: String,
    ): Set<String> {
        val budget = ExtractionBudget()
        val entries = linkedSetOf<String>()
        ZipInputStream(NonClosingInputStream(input)).use { zip ->
            var entry = zip.nextEntry
            while (entry != null) {
                val size = entry.size.coerceAtLeast(0)
                val compressedSize = entry.compressedSize.coerceAtLeast(0)
                budget.registerEntry(if (entry.isDirectory) 0 else size, compressedSize, "$label:${entry.name}")
                if (!entry.isDirectory) {
                    val name =
                        entry.name
                            .replace('\\', '/')
                            .substringAfterLast('/')
                            .trim()
                    if (name.isEmpty()) throw IOException("$label contains an empty entry name")
                    zip.drainUnknownSize(budget, compressedSize, "$label:${entry.name}")
                    entries += name.lowercase(Locale.US)
                }
                zip.closeEntry()
                entry = zip.nextEntry
            }
        }
        return entries
    }

    private fun InputStream.drainUnknownSize(
        budget: ExtractionBudget,
        compressedSize: Long,
        label: String,
    ) {
        val buffer = ByteArray(64 * 1024)
        var readTotal = 0L
        while (true) {
            val read = read(buffer)
            if (read <= 0) return
            readTotal += read
            budget.accountActual(read, readTotal, compressedSize, label)
        }
    }

    private class NonClosingInputStream(
        input: InputStream,
    ) : FilterInputStream(input) {
        override fun close() = Unit
    }
}
