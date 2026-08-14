package com.dxxredux.app

import java.io.IOException

internal class MissionMusicCatalogRejected(
    message: String,
) : IOException(message)

internal class MissionMusicCatalogBudget(
    maxWorkEntries: Int = ExtractionLimits.MAX_ENTRIES,
    maxRetainedEntries: Int = ExtractionLimits.MAX_ENTRIES,
    maxRetainedBytes: Long = ExtractionLimits.MAX_METADATA_BYTES,
    maxExpandedBytes: Long = ExtractionLimits.MAX_TOTAL_BYTES,
    maxExpansionRatio: Long = ExtractionLimits.MAX_RATIO,
    private val isCancelled: () -> Boolean = { Thread.currentThread().isInterrupted },
) {
    private val maxRetainedEntryBytes = minOf(maxRetainedBytes, ExtractionLimits.MAX_DESCRIPTOR_BYTES)

    init {
        require(maxWorkEntries >= 0)
        require(maxRetainedEntries >= 0)
        require(maxRetainedBytes in 1..Int.MAX_VALUE.toLong())
        require(maxExpandedBytes > 0)
        require(maxExpansionRatio > 0)
    }

    private val workBudget =
        ExtractionBudget(
            maxEntryBytes = 1,
            maxTotalBytes = 1,
            maxEntries = maxWorkEntries,
        )
    private val retainedBudget =
        ExtractionBudget(
            maxEntryBytes = maxRetainedEntryBytes,
            maxTotalBytes = maxRetainedBytes,
            maxEntries = maxRetainedEntries,
        )
    internal val expansionBudget =
        ExtractionBudget(
            maxTotalBytes = maxExpandedBytes,
            maxRatio = maxExpansionRatio,
        )

    fun visit(label: String) {
        checkCancelled()
        rejectAsCatalogFailure { workBudget.registerEntry(0, 0, label) }
    }

    fun retain(
        label: String,
        vararg values: String,
    ) {
        checkCancelled()
        val bytes =
            values.fold(RETAINED_OBJECT_BYTES) { total, value ->
                saturatingAdd(total, saturatingMultiply(utf8Length(value), RETAINED_STRING_BYTE_FACTOR))
            }
        if (bytes > Int.MAX_VALUE) throw MissionMusicCatalogRejected("$label exceeds retained catalog memory")
        rejectAsCatalogFailure {
            retainedBudget.registerEntry(bytes, bytes, label)
            retainedBudget.accountActual(bytes.toInt(), bytes, bytes, label)
        }
    }

    fun checkCancelled() {
        if (isCancelled()) throw MissionMusicCatalogRejected("Music catalog scan was cancelled")
    }

    private fun rejectAsCatalogFailure(action: () -> Unit) {
        try {
            action()
        } catch (e: IOException) {
            throw MissionMusicCatalogRejected(e.message ?: "Music catalog budget was exceeded")
        }
    }

    private fun utf8Length(value: String): Long {
        var bytes = 0L
        var index = 0
        while (index < value.length) {
            val character = value[index]
            bytes +=
                when {
                    character.code <= 0x7f -> {
                        1
                    }

                    character.code <= 0x7ff -> {
                        2
                    }

                    Character.isHighSurrogate(character) &&
                        index + 1 < value.length &&
                        Character.isLowSurrogate(value[index + 1]) -> {
                        index++
                        4
                    }

                    else -> {
                        3
                    }
                }
            index++
        }
        return bytes
    }

    private fun saturatingMultiply(
        left: Long,
        right: Long,
    ): Long =
        if (left == 0L || right <= Long.MAX_VALUE / left) {
            left * right
        } else {
            Long.MAX_VALUE
        }

    private fun saturatingAdd(
        left: Long,
        right: Long,
    ): Long = if (right <= Long.MAX_VALUE - left) left + right else Long.MAX_VALUE

    private companion object {
        const val RETAINED_OBJECT_BYTES = 256L
        const val RETAINED_STRING_BYTE_FACTOR = 2L
    }
}
