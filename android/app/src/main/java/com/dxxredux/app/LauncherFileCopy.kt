package com.dxxredux.app

import android.content.Context
import android.net.Uri
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
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
            if (count <= 0) break
            output.write(buffer, 0, count)
            bytesDone += count.toLong()
            if (bytesDone - lastReported >= REPORT_STEP_BYTES || bytesDone == bytesTotal) {
                lastReported = bytesDone
                onProgress(LauncherCopyProgress(label, bytesDone, bytesTotal))
            }
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
        context.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(dest).use { output ->
                return copyStream(input, output, total, label, onProgress)
            }
        } ?: throw java.io.IOException("Could not open selected file")
    }

    fun copyFileToFile(
        source: File,
        dest: File,
        label: String = source.name,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Long {
        FileInputStream(source).use { input ->
            FileOutputStream(dest).use { output ->
                return copyStream(input, output, source.length(), label, onProgress)
            }
        }
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
}
