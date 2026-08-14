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

    // Keep synchronized with DXX_EXTRACT_MAX_MEMORY_BYTES in extract_limits.h
    const val MAX_COMBINED_MEMORY_BYTES = 128L * 1024L * 1024L
    const val MAX_SEVEN_Z_DECODER_MEMORY_KIB = 256 * 1024
    const val MAX_ENTRIES = 4096
    const val MAX_RAR_CATALOG_WORK_UNITS = 65536L
    const val MAX_RATIO = 1000L
}

internal class BoundedReadMemoryPolicy(
    val maxLiveBytes: Long = ExtractionLimits.MAX_COMBINED_MEMORY_BYTES,
    val chunkBytes: Int = 64 * 1024,
    val allocate: (Int) -> ByteArray = { ByteArray(it) },
    val onLiveBytesChanged: (Long) -> Unit = {},
) {
    init {
        require(maxLiveBytes in 1..Int.MAX_VALUE.toLong())
        require(chunkBytes > 0)
    }
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

    fun newDiscExtractionAttempt(): DiscExtractionAttempt =
        DiscExtractionAttempt(longArrayOf(actualBytes, entries.toLong(), 0L))

    fun acceptDiscExtractionAttempt(attempt: DiscExtractionAttempt) {
        if (attempt.cancelled) throw IOException("Archive extraction was cancelled")
        if (attempt.outputBytes < actualBytes || attempt.outputBytes > maxTotalBytes) {
            throw IOException("Archive exceeds $maxTotalBytes extracted bytes")
        }
        if (attempt.entries < entries || attempt.entries > maxEntries.toLong()) {
            throw IOException("Archive exceeds $maxEntries entries")
        }
        actualBytes = attempt.outputBytes
        entries = attempt.entries.toInt()
    }

    fun registerEntry(
        declaredSize: Long,
        compressedSize: Long,
        label: String,
    ) {
        checkCancellation()
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
        checkCancellation()
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

    fun validateExpansion(
        expandedSize: Long,
        compressedSize: Long,
        label: String,
    ) {
        checkCancellation()
        if (expandedSize <= 0) return
        if (compressedSize <= 0) throw IOException("$label has no usable compressed size")
        if (!ratioAllowed(expandedSize, compressedSize)) {
            throw IOException("$label exceeds the $maxRatio:1 expansion ratio")
        }
    }

    private fun checkCancellation() {
        if (Thread.currentThread().isInterrupted) throw IOException("Archive extraction was cancelled")
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
    expectedSizeBytes: Long = -1,
    memoryPolicy: BoundedReadMemoryPolicy = BoundedReadMemoryPolicy(),
): ByteArray {
    require(maxBytes in 1..Int.MAX_VALUE.toLong())
    require(expectedSizeBytes >= -1)
    return if (expectedSizeBytes >= 0) {
        readBytesExact(maxBytes, label, budget, compressedSize, expectedSizeBytes, memoryPolicy)
    } else {
        readBytesSegmented(maxBytes, label, budget, compressedSize, memoryPolicy)
    }
}

private class BoundedReadMemory(
    private val label: String,
    private val policy: BoundedReadMemoryPolicy,
) {
    var liveBytes = 0L
        private set

    fun allocate(size: Int): ByteArray {
        if (size.toLong() > policy.maxLiveBytes - liveBytes) {
            throw IOException("$label exceeds ${policy.maxLiveBytes} live memory bytes")
        }
        val result =
            try {
                policy.allocate(size)
            } catch (error: OutOfMemoryError) {
                throw IOException("Unable to allocate $size bytes for $label", error)
            }
        liveBytes += size
        policy.onLiveBytesChanged(liveBytes)
        return result
    }

    fun release(size: Int) {
        liveBytes -= size
        check(liveBytes >= 0)
        policy.onLiveBytesChanged(liveBytes)
    }
}

private fun InputStream.readBytesExact(
    maxBytes: Long,
    label: String,
    budget: ExtractionBudget?,
    compressedSize: Long,
    expectedSizeBytes: Long,
    memoryPolicy: BoundedReadMemoryPolicy,
): ByteArray {
    if (expectedSizeBytes > maxBytes) throw IOException("$label exceeds $maxBytes bytes")
    val memory = BoundedReadMemory(label, memoryPolicy)
    val output = memory.allocate(expectedSizeBytes.toInt())
    var total = 0
    try {
        while (total < output.size) {
            var count = read(output, total, output.size - total)
            if (count < 0) throw IOException("Unexpected end of $label")
            if (count == 0) {
                val value = read()
                if (value < 0) throw IOException("Unexpected end of $label")
                output[total] = value.toByte()
                count = 1
            }
            total += count
            budget?.accountActual(count, total.toLong(), compressedSize, label)
        }
        if (read() >= 0) throw IOException("$label exceeds its declared size of $expectedSizeBytes bytes")
        return output
    } catch (error: Throwable) {
        memory.release(output.size)
        throw error
    }
}

private data class BoundedReadChunk(
    val bytes: ByteArray,
    var used: Int = 0,
)

private fun InputStream.readBytesSegmented(
    maxBytes: Long,
    label: String,
    budget: ExtractionBudget?,
    compressedSize: Long,
    memoryPolicy: BoundedReadMemoryPolicy,
): ByteArray {
    val memory = BoundedReadMemory(label, memoryPolicy)
    val payloadLimit = minOf(maxBytes, memoryPolicy.maxLiveBytes / 2)
    val chunks = ArrayList<BoundedReadChunk>()
    var total = 0L
    var result: ByteArray? = null
    try {
        var eof = false
        while (!eof && total < payloadLimit) {
            val size = minOf(memoryPolicy.chunkBytes.toLong(), payloadLimit - total).toInt()
            val chunk = BoundedReadChunk(memory.allocate(size))
            chunks += chunk
            while (chunk.used < chunk.bytes.size) {
                var count = read(chunk.bytes, chunk.used, chunk.bytes.size - chunk.used)
                if (count < 0) {
                    eof = true
                    break
                }
                if (count == 0) {
                    val value = read()
                    if (value < 0) {
                        eof = true
                        break
                    }
                    chunk.bytes[chunk.used] = value.toByte()
                    count = 1
                }
                chunk.used += count
                total += count
                budget?.accountActual(count, total, compressedSize, label)
            }
        }
        if (!eof && read() >= 0) {
            if (total == maxBytes) throw IOException("$label exceeds $maxBytes bytes")
            throw IOException("$label exceeds ${memoryPolicy.maxLiveBytes} live memory bytes")
        }
        result = memory.allocate(total.toInt())
        var offset = 0
        for (chunk in chunks) {
            chunk.bytes.copyInto(result, offset, 0, chunk.used)
            offset += chunk.used
        }
        for (chunk in chunks) memory.release(chunk.bytes.size)
        chunks.clear()
        return result
    } catch (error: Throwable) {
        result?.let { memory.release(it.size) }
        for (chunk in chunks) memory.release(chunk.bytes.size)
        chunks.clear()
        throw error
    }
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
