package com.dxxredux.app

import androidx.annotation.Keep
import java.io.IOException
import java.io.InputStream

/** Headers use the engine's native format reader; payloads stay bounded streams */
@Keep
internal object NativeHogArchive {
    init {
        System.loadLibrary("dxx_hog_reader")
    }

    @Keep
    data class Entry(
        val name: String,
        val size: Long,
        val extended: Boolean,
        val headerBytes: Int,
    )

    // Match the engine's byte-wise ASCII case fold, including legacy filename bytes
    fun key(name: String): String = name.map { if (it in 'A'..'Z') it + ('a' - 'A') else it }.joinToString("")

    fun visit(
        input: InputStream,
        label: String,
        budget: ExtractionBudget = ExtractionBudget(),
        expectedSize: Long = -1,
        visitor: (Entry, InputStream) -> Unit,
    ) {
        nativeReadSignature(input)
        // Signature length is a public container constant; header widths come from native code
        var position = 3L
        val names = mutableSetOf<String>()
        val discard = ByteArray(64 * 1024)
        while (true) {
            val entry = nativeReadEntry(input) ?: break
            position += entry.headerBytes
            if (!names.add(key(entry.name))) throw IOException("Ambiguous duplicate HOG member ${entry.name}")
            val entryLabel = "$label:${entry.name}"
            budget.registerEntry(entry.size, entry.size, entryLabel)
            if (expectedSize >= 0 && (position > expectedSize || entry.size > expectedSize - position)) {
                throw IOException("Truncated HOG member ${entry.name}")
            }
            val payload = Payload(input, entry.size, budget, entryLabel)
            try {
                visitor(entry, payload)
                while (payload.read(discard) >= 0) {
                    // Validate every payload and trailing header before publishing a catalog or extraction
                }
            } finally {
                payload.finish()
            }
            position += entry.size
        }
        if (expectedSize >= 0 && position != expectedSize) throw IOException("HOG size changed while reading $label")
    }

    private class Payload(
        private val input: InputStream,
        private val size: Long,
        private val budget: ExtractionBudget,
        private val label: String,
    ) : InputStream() {
        private var consumed = 0L
        private var finished = false

        override fun read(): Int {
            val byte = ByteArray(1)
            return if (read(byte, 0, 1) < 0) -1 else byte[0].toInt() and 0xff
        }

        override fun read(
            bytes: ByteArray,
            offset: Int,
            length: Int,
        ): Int {
            if (offset < 0 || length < 0 || offset > bytes.size - length) throw IndexOutOfBoundsException()
            if (finished) throw IOException("HOG entry stream is no longer active")
            if (length == 0) return 0
            if (consumed == size) return -1
            val requested = minOf(length.toLong(), size - consumed).toInt()
            val count = input.read(bytes, offset, requested)
            if (count <= 0 || count > requested) throw IOException("Truncated HOG member $label")
            consumed += count
            budget.accountActual(count, consumed, size, label)
            return count
        }

        fun finish() {
            finished = true
        }

        override fun close() = Unit
    }

    @JvmStatic
    private external fun nativeReadSignature(input: InputStream)

    @JvmStatic
    private external fun nativeReadEntry(input: InputStream): Entry?
}
