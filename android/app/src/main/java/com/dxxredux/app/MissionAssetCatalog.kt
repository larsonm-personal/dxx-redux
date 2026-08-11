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
        if (input.readExact(3)?.toString(Charsets.US_ASCII) != "DHF") return null
        val budget = ExtractionBudget()
        val entries = linkedSetOf<String>()
        while (true) {
            val nameBytes = input.readUpTo(13)
            if (nameBytes.isEmpty()) return entries
            if (nameBytes.size != 13) return null
            val sizeBytes = input.readExact(4) ?: return null
            val size = littleEndianUnsignedInt(sizeBytes)
            val name = archiveLeafName(nameBytes) ?: return null
            budget.registerEntry(size, size, "$label:$name")
            input.drainExact(size, budget, "$label:$name") ?: return null
            entries += name.lowercase(Locale.US)
        }
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

    private fun InputStream.drainExact(
        count: Long,
        budget: ExtractionBudget,
        label: String,
    ): Unit? {
        val buffer = ByteArray(64 * 1024)
        var remaining = count
        var readTotal = 0L
        while (remaining > 0) {
            val read = read(buffer, 0, minOf(buffer.size.toLong(), remaining).toInt())
            if (read <= 0) return null
            readTotal += read
            budget.accountActual(read, readTotal, count, label)
            remaining -= read
        }
        return Unit
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

    private fun InputStream.readExact(count: Int): ByteArray? = readUpTo(count).takeIf { it.size == count }

    private fun InputStream.readUpTo(count: Int): ByteArray {
        val bytes = ByteArray(count)
        var readTotal = 0
        while (readTotal < count) {
            val read = read(bytes, readTotal, count - readTotal)
            if (read <= 0) break
            readTotal += read
        }
        return if (readTotal == count) bytes else bytes.copyOf(readTotal)
    }

    private fun littleEndianUnsignedInt(bytes: ByteArray): Long =
        (bytes[0].toLong() and 0xff) or
            ((bytes[1].toLong() and 0xff) shl 8) or
            ((bytes[2].toLong() and 0xff) shl 16) or
            ((bytes[3].toLong() and 0xff) shl 24)

    private fun archiveLeafName(bytes: ByteArray): String? {
        val end = bytes.indexOf(0).takeIf { it >= 0 } ?: bytes.size
        val name = bytes.copyOfRange(0, end).toString(Charsets.US_ASCII).trim()
        return name.takeIf { it.isNotEmpty() && '/' !in it && '\\' !in it }
    }

    private class NonClosingInputStream(
        input: InputStream,
    ) : FilterInputStream(input) {
        override fun close() = Unit
    }
}
