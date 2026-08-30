package com.dxxredux.app

import java.util.Locale

private const val BINARY_KB = 1024L
private const val BINARY_MB = BINARY_KB * 1024L
private const val BINARY_GB = BINARY_MB * 1024L

internal fun formatBinarySize(bytes: Long): String =
    when {
        bytes >= BINARY_GB -> String.format(Locale.US, "%.2f GB", bytes / BINARY_GB.toDouble())
        bytes >= BINARY_MB -> String.format(Locale.US, "%.1f MB", bytes / BINARY_MB.toDouble())
        bytes >= BINARY_KB -> String.format(Locale.US, "%.0f KB", bytes / BINARY_KB.toDouble())
        else -> "$bytes B"
    }

internal fun formatBinaryRate(bytesPerSecond: Long): String =
    when {
        bytesPerSecond >= BINARY_GB -> String.format(Locale.US, "%.1f GB/s", bytesPerSecond / BINARY_GB.toDouble())
        bytesPerSecond >= BINARY_MB -> String.format(Locale.US, "%.1f MB/s", bytesPerSecond / BINARY_MB.toDouble())
        bytesPerSecond >= BINARY_KB -> String.format(Locale.US, "%.1f KB/s", bytesPerSecond / BINARY_KB.toDouble())
        else -> "$bytesPerSecond B/s"
    }

internal fun formatBinaryMegabytesRoundedUp(bytes: Long): String {
    val safeBytes = bytes.coerceAtLeast(0L)
    val roundedUp = (safeBytes / BINARY_MB + if (safeBytes % BINARY_MB == 0L) 0L else 1L).coerceAtLeast(1L)
    return "$roundedUp MB"
}
