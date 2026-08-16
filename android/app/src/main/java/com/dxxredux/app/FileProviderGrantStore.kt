package com.dxxredux.app

import android.content.Context
import android.net.Uri
import androidx.core.content.FileProvider
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

internal object FileProviderGrantStore {
    const val AUTHORITY = "com.dxxredux.app.fileprovider"

    // Keep these roots in sync with res/xml/file_paths.xml
    const val CONFIG_EXPORTS = "config_exports"
    const val DEBUG_LOG_EXPORTS = "debuglog_exports"
    const val CRASH_LOG_EXPORTS = "crashlog_exports"
    const val INPUT_DEMO_EXPORTS = "inputdemo_exports"
    const val STORAGE_INSPECTOR_EXPORTS = "storage_inspector_exports"
    const val FILE_VIEW = "file_view"
    internal const val RETENTION_MS = 24L * 60L * 60L * 1000L
    internal const val MAX_ROOT_BYTES = 64L * 1024L * 1024L

    private val lock = Any()
    private val roots =
        setOf(
            CONFIG_EXPORTS,
            DEBUG_LOG_EXPORTS,
            CRASH_LOG_EXPORTS,
            INPUT_DEMO_EXPORTS,
            STORAGE_INSPECTOR_EXPORTS,
            FILE_VIEW,
        )

    fun copy(
        context: Context,
        source: File,
        rootName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Uri =
        publish(context, rootName, source.name, source.length()) { temporary ->
            LauncherFileCopy.copyFileToFile(source, temporary, source.name, onProgress = onProgress)
        }

    fun writeUtf8(
        context: Context,
        rootName: String,
        displayName: String,
        text: String,
    ): Uri {
        val bytes = text.toByteArray(Charsets.UTF_8)
        return publish(context, rootName, displayName, bytes.size.toLong()) { temporary ->
            FileOutputStream(temporary).use { it.write(bytes) }
        }
    }

    fun publish(
        context: Context,
        rootName: String,
        displayName: String,
        expectedBytes: Long,
        writer: (File) -> Unit,
    ): Uri {
        require(rootName in roots) { "Unsupported FileProvider cache root" }
        val published = publishFile(File(context.cacheDir, rootName), displayName, expectedBytes, writer = writer)
        return FileProvider.getUriForFile(context, AUTHORITY, published)
    }

    internal fun publishFile(
        root: File,
        displayName: String,
        expectedBytes: Long,
        maxRootBytes: Long = MAX_ROOT_BYTES,
        retentionMs: Long = RETENTION_MS,
        nowMs: Long = System.currentTimeMillis(),
        writer: (File) -> Unit,
    ): File =
        synchronized(lock) {
            require(maxRootBytes >= 0L && retentionMs >= 0L && expectedBytes >= 0L) { "Invalid grant cache limit" }
            check(root.mkdirs() || root.isDirectory) { "Could not create grant cache" }
            pruneExpired(root, retentionMs, nowMs)
            val retainedBytes = cacheBytes(root, maxRootBytes)
            if (expectedBytes > maxRootBytes || retainedBytes > maxRootBytes - expectedBytes) {
                throw IOException("Shared-file cache is retaining unexpired grants")
            }

            val generation = OwnedCacheDirectories.create(root)
            val target = File(generation, safeDisplayName(displayName))
            val temporary = File(generation, ".${target.name}.tmp")
            try {
                writer(temporary)
                check(temporary.isFile) { "Shared file was not produced" }
                FileOutputStream(temporary, true).use { stream ->
                    stream.flush()
                    stream.fd.sync()
                }
                val actualBytes = temporary.length().coerceAtLeast(0L)
                if (actualBytes != expectedBytes) {
                    throw IOException("Shared file changed during publication")
                }
                if (actualBytes > maxRootBytes || retainedBytes > maxRootBytes - actualBytes) {
                    throw IOException("Shared-file cache is retaining unexpired grants")
                }
                check(temporary.renameTo(target)) { "Could not publish shared file" }
                check(target.setLastModified(nowMs) && generation.setLastModified(nowMs)) {
                    "Could not timestamp shared file generation"
                }
                target
            } catch (failure: Throwable) {
                OwnedCacheDirectories.delete(root, generation)
                throw failure
            } finally {
                temporary.delete()
            }
        }

    private fun pruneExpired(
        root: File,
        retentionMs: Long,
        nowMs: Long,
    ) {
        root.listFiles().orEmpty().forEach { child ->
            val expired = nowMs >= child.lastModified() && nowMs - child.lastModified() >= retentionMs
            if (!expired) return@forEach
            when {
                child.isFile -> child.delete()
                OwnedCacheDirectories.isOwned(root, child) -> OwnedCacheDirectories.delete(root, child)
            }
        }
    }

    private fun cacheBytes(
        root: File,
        stopAfterBytes: Long,
    ): Long {
        var total = 0L
        for (file in root.walkTopDown()) {
            if (!file.isFile) continue
            val length = file.length().coerceAtLeast(0L)
            if (length > stopAfterBytes - total) return stopAfterBytes + 1L
            total += length
        }
        return total
    }

    private fun safeDisplayName(displayName: String): String {
        val leaf = GameFileFormats.leafName(displayName).ifBlank { "document" }
        val rawStem = leaf.substringBeforeLast('.', leaf)
        val sanitizedStem = rawStem.replace(Regex("[^A-Za-z0-9._-]"), "_").take(96)
        val stem = sanitizedStem.takeUnless { it.isBlank() || it == "." || it == ".." } ?: "document"
        val extension =
            leaf
                .substringAfterLast('.', "")
                .replace(Regex("[^A-Za-z0-9]"), "")
                .take(16)
        return if (extension.isBlank()) stem else "$stem.$extension"
    }
}
