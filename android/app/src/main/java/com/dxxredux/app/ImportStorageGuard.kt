package com.dxxredux.app

import android.content.ContentResolver
import android.net.Uri
import android.os.StatFs
import android.provider.OpenableColumns
import java.io.File
import java.io.IOException
import java.util.Locale

internal class InsufficientStorageException(
    val requiredFreeBytes: Long,
    val availableBytes: Long,
    target: String,
) : IOException(
        "Not enough free space for $target " +
            "(need ${ImportStorageGuard.formatMib(requiredFreeBytes)} free, " +
            "have ${ImportStorageGuard.formatMib(availableBytes)})",
    )

internal object ImportStorageGuard {
    private const val HEADROOM_BYTES = 50L * 1024L * 1024L
    private const val ERROR_FILE_NAME = "last_extract_error.txt"
    private const val MIB = 1024L * 1024L

    fun queryUriSizeBytes(
        contentResolver: ContentResolver,
        uri: Uri,
    ): Long? {
        val queried =
            try {
                contentResolver
                    .query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)
                    ?.use { cursor ->
                        if (cursor.moveToFirst() && !cursor.isNull(0)) cursor.getLong(0) else null
                    }
            } catch (_: Exception) {
                null
            }
        if (queried != null && queried >= 0L) return queried

        return try {
            contentResolver.openAssetFileDescriptor(uri, "r")?.use { afd ->
                afd.length.takeIf { it >= 0L }
            }
        } catch (_: Exception) {
            null
        }
    }

    fun requireFreeSpace(
        targetDir: File,
        bytesToWrite: Long,
        target: String,
    ) {
        val requiredFreeBytes = bytesToWrite.coerceAtLeast(0L) + HEADROOM_BYTES
        val measurement = measureAvailableBytes(targetDir)
        if (bytesToWrite > 0L || measurement.availableBytes <= 0L || measurement.availableBytes < requiredFreeBytes) {
            logStorageCheck(
                "storage-check target=$target path=${measurement.path.absolutePath} " +
                    "write_bytes=$bytesToWrite required_bytes=$requiredFreeBytes " +
                    "available_bytes=${measurement.availableBytes}",
            )
        }
        // A zero result also represents a failed or unsupported filesystem
        // query. Let the transactional writer try in that case; it will still
        // roll back its temporary output if the volume is genuinely full.
        if (shouldReject(measurement.availableBytes, requiredFreeBytes)) {
            throw InsufficientStorageException(requiredFreeBytes, measurement.availableBytes, target)
        }
    }

    fun messageForFailure(error: InsufficientStorageException): String =
        "Not enough free space to continue.\n\n" +
            "Required: ${formatMib(error.requiredFreeBytes)}\n" +
            "Available: ${formatMib(error.availableBytes)}\n\n" +
            "The operation was stopped before writing more files."

    fun recordFailure(
        filesDir: File,
        message: String,
        throwable: Throwable? = null,
    ) {
        try {
            val errorFile = File(filesDir, ERROR_FILE_NAME)
            val details =
                buildString {
                    append(message)
                    throwable?.message?.takeIf { it.isNotBlank() }?.let {
                        append('\n')
                        append(it)
                    }
                }
            errorFile.writeText(details)
        } catch (_: Exception) {
        }
    }

    fun formatMib(bytes: Long): String {
        val roundedUp = ((bytes.coerceAtLeast(0L) + MIB - 1L) / MIB).coerceAtLeast(1L)
        return String.format(Locale.US, "%d MiB", roundedUp)
    }

    fun archiveEntryBytes(entries: Iterable<Long>): Long {
        var total = 0L
        for (size in entries) {
            if (size > 0L) total += size
        }
        return total
    }

    internal data class StorageMeasurement(
        val path: File,
        val availableBytes: Long,
    )

    internal fun shouldReject(
        availableBytes: Long,
        requiredFreeBytes: Long,
    ): Boolean = availableBytes > 0L && availableBytes < requiredFreeBytes

    private fun logStorageCheck(message: String) {
        try {
            LauncherDebugLog.log(message)
        } catch (_: Throwable) {
            println(message)
        }
    }

    internal fun measureAvailableBytes(dir: File): StorageMeasurement {
        var current: File? = dir
        while (current != null && !current.exists()) {
            current = current.parentFile
        }
        val statPath = current ?: dir
        val available =
            try {
                StatFs(statPath.absolutePath).availableBytes
            } catch (_: Throwable) {
                statPath.usableSpace
            }
        return StorageMeasurement(statPath, available)
    }
}
