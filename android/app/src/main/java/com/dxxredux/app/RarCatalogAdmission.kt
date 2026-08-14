package com.dxxredux.app

import kotlinx.coroutines.CancellationException
import java.io.Closeable
import java.io.IOException
import java.text.Normalizer
import java.util.Locale

internal data class RarCatalogAdmissionPolicy(
    val maxEntries: Int = ExtractionLimits.MAX_ENTRIES,
    val maxWorkUnits: Long = ExtractionLimits.MAX_RAR_CATALOG_WORK_UNITS,
    val maxLiveBytes: Long = ExtractionLimits.MAX_COMBINED_MEMORY_BYTES,
    val isCancelled: () -> Boolean = { Thread.currentThread().isInterrupted },
    val onLiveBytesChanged: (Long) -> Unit = {},
) {
    init {
        require(maxEntries >= 0)
        require(maxWorkUnits >= 0)
        require(maxLiveBytes >= 0)
    }
}

internal interface RarCatalogSource {
    val itemCount: Int

    fun path(index: Int): String?

    fun isDirectory(index: Int): Boolean

    fun size(index: Int): Long

    fun packedSize(index: Int): Long
}

internal data class RarExtractionItem(
    val index: Int,
    val path: String,
    val isDirectory: Boolean,
    val size: Long,
    val packedSize: Long,
)

internal class RarCatalog internal constructor(
    internal val items: List<RarExtractionItem>,
    internal val extractionBudget: ExtractionBudget,
    private val attempt: RarCatalogAttempt,
) : Closeable {
    override fun close() = attempt.close()
}

internal fun enumerateRarCatalog(
    source: RarCatalogSource,
    policy: RarCatalogAdmissionPolicy = RarCatalogAdmissionPolicy(),
): RarCatalog {
    if (source.itemCount < 0) throw IOException("RAR archive has an invalid item count")
    val extractionBudget = ExtractionBudget()
    val attempt = RarCatalogAttempt(policy, extractionBudget)
    try {
        repeat(source.itemCount) { index -> attempt.inspect(source, index) }
        return RarCatalog(attempt.items, extractionBudget, attempt)
    } catch (error: Throwable) {
        attempt.close()
        throw error
    }
}

internal class RarCatalogAttempt(
    private val policy: RarCatalogAdmissionPolicy,
    private val extractionBudget: ExtractionBudget,
) : Closeable {
    internal val items = ArrayList<RarExtractionItem>()
    private val outputKinds = HashMap<String, Boolean>()
    private var entries = 0
    private var workUnits = 0L
    private var liveBytes = 0L
    private var closed = false

    fun inspect(
        source: RarCatalogSource,
        index: Int,
    ) {
        consumeWork()
        val rawPath = source.path(index) ?: return
        consumeWork(unitsForCharacters(rawPath.length))
        val rawBytes = estimatedStringBytes(rawPath.length)
        reserve(rawBytes, "RAR catalog path")
        var canonicalBytes = 0L
        try {
            val normalizationBytes = rawBytes * 4
            reserve(normalizationBytes, "RAR catalog path normalization")
            val path = canonicalArchiveRelativePath(rawPath)
            release(normalizationBytes)
            if (rawPath.replace('\\', '/').trim('/').isBlank()) return
            if (path.isNullOrBlank()) {
                throw ArchiveOutputValidationException("RAR archive has an invalid output path: $rawPath")
            }

            canonicalBytes = estimatedStringBytes(path.length)
            reserve(canonicalBytes, "RAR catalog path")
            entries++
            if (entries > policy.maxEntries) {
                throw IOException("RAR archive exceeds ${policy.maxEntries} entries")
            }
            reserve(RAR_ITEM_BYTES, "RAR catalog items")

            val isDirectory = source.isDirectory(index)
            claimOutput(path, isDirectory)
            val size = source.size(index)
            val packedSize = source.packedSize(index)
            extractionBudget.registerEntry(if (isDirectory) 0 else size, if (isDirectory) 0 else packedSize, path)
            items +=
                RarExtractionItem(
                    index = index,
                    path = path,
                    isDirectory = isDirectory,
                    size = size,
                    packedSize = packedSize,
                )
            canonicalBytes = 0
        } finally {
            if (canonicalBytes > 0) release(canonicalBytes)
            release(rawBytes)
        }
    }

    private fun claimOutput(
        path: String,
        isDirectory: Boolean,
    ) {
        var end = path.indexOf('/')
        while (true) {
            if (end < 0) end = path.length
            consumeWork(1 + unitsForCharacters(end))
            val keyBytes = estimatedStringBytes(end.toLong() * 3)
            val projectionBytes = keyBytes * 3
            reserve(projectionBytes, "RAR catalog output projection")
            var retained = false
            try {
                val projectedPath = path.substring(0, end)
                val key = Normalizer.normalize(projectedPath, Normalizer.Form.NFC).lowercase(Locale.ROOT)
                val directory = isDirectory || end < path.length
                val previous = outputKinds[key]
                if (previous == false && directory) {
                    throw ArchiveOutputValidationException(
                        "RAR archive has a file and directory output collision: $path",
                    )
                }
                if (previous == true && !directory) {
                    throw ArchiveOutputValidationException(
                        "RAR archive has a file and directory output collision: $path",
                    )
                }
                if (previous == false && !directory) {
                    throw ArchiveOutputValidationException("RAR archive has colliding file outputs: $path")
                }
                if (previous == null) {
                    reserve(RAR_OUTPUT_KEY_BYTES, "RAR catalog output projections")
                    outputKinds[key] = directory
                    retained = true
                }
            } finally {
                release(if (retained) projectionBytes - keyBytes else projectionBytes)
            }
            if (end == path.length) break
            end = path.indexOf('/', end + 1)
        }
    }

    private fun consumeWork(units: Long = 1) {
        if (policy.isCancelled()) throw CancellationException("RAR catalog enumeration was cancelled")
        if (units < 0 || units > policy.maxWorkUnits - workUnits) {
            throw IOException("RAR catalog exceeds ${policy.maxWorkUnits} work units")
        }
        workUnits += units
    }

    private fun reserve(
        bytes: Long,
        label: String,
    ) {
        if (bytes < 0 || bytes > policy.maxLiveBytes - liveBytes) {
            throw IOException("$label exceeds ${policy.maxLiveBytes} live memory bytes")
        }
        liveBytes += bytes
        policy.onLiveBytesChanged(liveBytes)
    }

    private fun release(bytes: Long) {
        liveBytes -= bytes
        check(liveBytes >= 0)
        policy.onLiveBytesChanged(liveBytes)
    }

    override fun close() {
        if (closed) return
        closed = true
        items.clear()
        outputKinds.clear()
        liveBytes = 0
        policy.onLiveBytesChanged(0)
    }

    private companion object {
        const val RAR_ITEM_BYTES = 64L
        const val RAR_OUTPUT_KEY_BYTES = 48L

        fun estimatedStringBytes(characters: Int): Long = estimatedStringBytes(characters.toLong())

        fun estimatedStringBytes(characters: Long): Long = 24L + characters * 2L

        fun unitsForCharacters(characters: Int): Long = (characters.toLong() + 255) / 256
    }
}
