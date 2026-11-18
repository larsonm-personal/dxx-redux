package com.dxxredux.app

import androidx.annotation.Keep
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.RandomAccessFile

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
        // A SAF or compressed stream may be forward-only. The last physical header
        // can change an earlier lookup, so capture bounded payloads before callbacks.
        val spool = File.createTempFile("dxx-hog-", ".tmp")
        try {
            RandomAccessFile(spool, "rw").use { file ->
                // Android can unlink an open file immediately, including across process death.
                spool.delete()
                val entries = mutableListOf<Entry>()
                val offsets = mutableListOf<Long>()
                val names = ByteArrayOutputStream()
                val scratch = ByteArray(64 * 1024)
                var position = 3L
                while (true) {
                    val entry = nativeReadEntry(input) ?: break
                    position += entry.headerBytes
                    val entryLabel = "$label:${entry.name}"
                    budget.registerEntry(entry.size, entry.size, entryLabel)
                    if (expectedSize >= 0 && (position > expectedSize || entry.size > expectedSize - position)) {
                        throw IOException("Truncated HOG member ${entry.name}")
                    }
                    offsets += file.filePointer
                    entries += entry
                    names.write(entry.name.toByteArray(Charsets.ISO_8859_1))
                    names.write(0)
                    var copied = 0L
                    while (copied < entry.size) {
                        val requested = minOf(entry.size - copied, scratch.size.toLong()).toInt()
                        val count = input.read(scratch, 0, requested)
                        if (count !in 1..requested) throw IOException("Truncated HOG member $entryLabel")
                        copied += count
                        budget.accountActual(count, copied, entry.size, entryLabel)
                        file.write(scratch, 0, count)
                    }
                    position += entry.size
                }
                if (expectedSize >= 0 &&
                    position != expectedSize
                ) {
                    throw IOException("HOG size changed while reading $label")
                }
                val selected = nativeSelect(names.toByteArray(), entries.size)
                entries.forEachIndexed { index, entry ->
                    if (selected[index]) {
                        budget.checkCancellation()
                        file.seek(offsets[index])
                        val payload = Payload(file, entry.size, budget, "$label:${entry.name}")
                        try {
                            visitor(entry, payload)
                        } finally {
                            payload.finish()
                        }
                    }
                }
            }
        } finally {
            spool.delete()
        }
    }

    private class Payload(
        private val input: RandomAccessFile,
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
            budget.checkCancellation()
            val count = input.read(bytes, offset, requested)
            if (count <= 0 || count > requested) throw IOException("Truncated HOG member $label")
            consumed += count
            return count
        }

        fun finish() {
            finished = true
        }

        override fun close() = Unit
    }

    @JvmStatic
    private external fun nativeSelect(
        names: ByteArray,
        count: Int,
    ): BooleanArray

    @JvmStatic
    private external fun nativeReadSignature(input: InputStream)

    @JvmStatic
    private external fun nativeReadEntry(input: InputStream): Entry?
}
