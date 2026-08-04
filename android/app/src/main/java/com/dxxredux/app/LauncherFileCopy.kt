package com.dxxredux.app

import android.content.Context
import android.net.Uri
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream

data class LauncherCopyProgress(
    val label: String,
    val bytesDone: Long,
    val bytesTotal: Long,
) {
    val fraction: Float
        get() = if (bytesTotal > 0L) bytesDone.toFloat() / bytesTotal.toFloat() else 0f
}

internal object LauncherFileCopy {
    private const val BUFFER_BYTES = 64 * 1024
    private const val REPORT_STEP_BYTES = 1024L * 1024L

    fun copyStream(
        input: InputStream,
        output: OutputStream,
        bytesTotal: Long,
        label: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Long {
        val buffer = ByteArray(BUFFER_BYTES)
        var bytesDone = 0L
        var lastReported = -REPORT_STEP_BYTES
        onProgress(LauncherCopyProgress(label, 0L, bytesTotal))
        while (true) {
            val count = input.read(buffer)
            if (count < 0) break
            if (count == 0) {
                val byte = input.read()
                if (byte < 0) break
                output.write(byte)
                bytesDone++
            } else {
                output.write(buffer, 0, count)
                bytesDone += count.toLong()
            }
            if (bytesDone - lastReported >= REPORT_STEP_BYTES || bytesDone == bytesTotal) {
                lastReported = bytesDone
                onProgress(LauncherCopyProgress(label, bytesDone, bytesTotal))
            }
        }
        if (bytesTotal > 0L && bytesDone != bytesTotal) {
            throw IOException("Incomplete copy of $label: expected $bytesTotal bytes, received $bytesDone")
        }
        onProgress(LauncherCopyProgress(label, bytesDone, bytesTotal))
        return bytesDone
    }

    fun copyUriToFile(
        context: Context,
        uri: Uri,
        dest: File,
        label: String = dest.name,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Long {
        val total = ImportStorageGuard.queryUriSizeBytes(context.contentResolver, uri) ?: 0L
        return copyInputToFile(dest, total, label, onProgress) {
            context.contentResolver.openInputStream(uri) ?: throw IOException("Could not open selected file")
        }
    }

    fun copyFileToFile(
        source: File,
        dest: File,
        label: String = source.name,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Long {
        val expectedBytes = source.length()
        return copyInputToFile(dest, expectedBytes, label, onProgress) { FileInputStream(source) }
    }

    fun copyFileToUri(
        context: Context,
        source: File,
        destUri: Uri,
        label: String = source.name,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Long {
        FileInputStream(source).use { input ->
            context.contentResolver.openOutputStream(destUri)?.use { output ->
                return copyStream(input, output, source.length(), label, onProgress)
            }
        }
        throw java.io.IOException("Could not open output file")
    }

    internal fun copyInputToFile(
        dest: File,
        expectedBytes: Long,
        label: String = dest.name,
        onProgress: (LauncherCopyProgress) -> Unit = {},
        openInput: () -> InputStream,
    ): Long {
        ImportStorageGuard.requireFreeSpace(dest.parentFile ?: dest, expectedBytes, label)
        dest.parentFile?.mkdirs()
        val temporary = AtomicFilePublication.uniqueSibling(dest, "copy")
        try {
            val copied =
                openInput().use { input ->
                    FileOutputStream(temporary).use { output ->
                        val count = copyStream(input, output, expectedBytes, label, onProgress)
                        output.flush()
                        output.fd.sync()
                        count
                    }
                }
            AtomicFilePublication.publishFile(temporary, dest)
            return copied
        } finally {
            temporary.delete()
        }
    }
}
