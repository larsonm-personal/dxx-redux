package com.dxxredux.app

import java.io.IOException
import java.io.InputStream
import java.io.OutputStream

internal object ExtractionLimits {
    const val MAX_ENTRY_BYTES = 512L * 1024L * 1024L
    const val MAX_TOTAL_BYTES = 2L * 1024L * 1024L * 1024L
    const val MAX_METADATA_BYTES = 64L * 1024L * 1024L
    const val MAX_DESCRIPTOR_BYTES = 1024L * 1024L
    const val MAX_ZIP_PREAMBLE_BYTES = 16L * 1024L * 1024L
    const val MAX_SEVEN_Z_DECODER_MEMORY_KIB = 256 * 1024
    const val MAX_ENTRIES = 4096
    const val MAX_RATIO = 1000L
}

internal class ExtractionBudget(
    private val maxEntryBytes: Long = ExtractionLimits.MAX_ENTRY_BYTES,
    private val maxTotalBytes: Long = ExtractionLimits.MAX_TOTAL_BYTES,
    private val maxEntries: Int = ExtractionLimits.MAX_ENTRIES,
    private val maxRatio: Long = ExtractionLimits.MAX_RATIO,
) {
    private var entries = 0
    private var declaredBytes = 0L
    private var actualBytes = 0L

    fun registerEntry(
        declaredSize: Long,
        compressedSize: Long,
        label: String,
    ) {
        entries++
        if (entries > maxEntries) {
            throw IOException("Archive exceeds $maxEntries entries")
        }
        if (declaredSize > maxEntryBytes) {
            throw IOException("$label exceeds $maxEntryBytes bytes")
        }
        if (declaredSize > 0 && compressedSize > 0 && !ratioAllowed(declaredSize, compressedSize)) {
            throw IOException("$label exceeds the $maxRatio:1 expansion ratio")
        }
        if (declaredSize > 0) {
            if (declaredSize > maxTotalBytes || declaredBytes > maxTotalBytes - declaredSize) {
                throw IOException("Archive exceeds $maxTotalBytes declared bytes")
            }
            declaredBytes += declaredSize
        }
    }

    fun accountActual(
        addedBytes: Int,
        entryBytes: Long,
        compressedSize: Long,
        label: String,
    ) {
        if (entryBytes > maxEntryBytes) {
            throw IOException("$label exceeds $maxEntryBytes bytes")
        }
        if (entryBytes > 0 && compressedSize > 0 && !ratioAllowed(entryBytes, compressedSize)) {
            throw IOException("$label exceeds the $maxRatio:1 expansion ratio")
        }
        if (addedBytes < 0 || addedBytes.toLong() > maxTotalBytes || actualBytes > maxTotalBytes - addedBytes) {
            throw IOException("Archive exceeds $maxTotalBytes extracted bytes")
        }
        actualBytes += addedBytes
    }

    private fun ratioAllowed(
        expanded: Long,
        compressed: Long,
    ): Boolean {
        val quotient = expanded / compressed
        return quotient < maxRatio || (quotient == maxRatio && expanded % compressed == 0L)
    }
}

internal fun InputStream.readBytesBounded(
    maxBytes: Long,
    label: String,
    budget: ExtractionBudget? = null,
    compressedSize: Long = -1,
): ByteArray {
    require(maxBytes in 1..Int.MAX_VALUE.toLong())
    val output = java.io.ByteArrayOutputStream(minOf(maxBytes, 64L * 1024L).toInt())
    val buffer = ByteArray(64 * 1024)
    var total = 0L
    while (true) {
        val count = read(buffer)
        if (count <= 0) break
        if (total > maxBytes - count) throw IOException("$label exceeds $maxBytes bytes")
        budget?.accountActual(count, total + count, compressedSize, label)
        output.write(buffer, 0, count)
        total += count
    }
    return output.toByteArray()
}

internal fun InputStream.copyToBounded(
    output: OutputStream,
    budget: ExtractionBudget,
    compressedSize: Long,
    label: String,
    onBytes: (Int) -> Unit = {},
): Long {
    val buffer = ByteArray(64 * 1024)
    var entryBytes = 0L
    while (true) {
        val count = read(buffer)
        if (count <= 0) break
        entryBytes += count
        budget.accountActual(count, entryBytes, compressedSize, label)
        output.write(buffer, 0, count)
        onBytes(count)
    }
    return entryBytes
}
