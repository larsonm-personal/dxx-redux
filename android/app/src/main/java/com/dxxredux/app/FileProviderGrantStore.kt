package com.dxxredux.app

import android.content.Context
import android.net.Uri
import androidx.core.content.FileProvider
import java.io.File
import java.io.FileInputStream
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

    // Recorded demos and their trace sidecars can each exceed the ordinary export budget
    internal const val MAX_DEMO_ROOT_BYTES = 4L * 1024L * 1024L * 1024L

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

    fun copyFiles(
        context: Context,
        sources: List<File>,
        rootName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): List<Uri> =
        synchronized(lock) {
            require(rootName in roots) { "Unsupported FileProvider cache root" }
            require(sources.isNotEmpty()) { "A share needs at least one file" }
            val root = File(context.cacheDir, rootName)
            val maxRootBytes = rootByteLimit(rootName)
            val expectedBytes = sources.fold(0L) { total, source -> Math.addExact(total, source.length()) }
            val nowMs = System.currentTimeMillis()
            check(root.mkdirs() || root.isDirectory) { "Could not create grant cache" }
            pruneExpired(root, RETENTION_MS, nowMs)
            requireCapacity(root, expectedBytes, cacheBytes(root, maxRootBytes), maxRootBytes)
            ImportStorageGuard.requireFreeSpace(root, expectedBytes, "Shared files")
            val published = mutableListOf<File>()
            try {
                sources.forEach { source ->
                    published +=
                        publishFile(root, source.name, source.length(), maxRootBytes, nowMs = nowMs) { temporary ->
                            LauncherFileCopy.copyFileToFile(
                                source,
                                temporary,
                                maxBytes = maxRootBytes,
                                onProgress = onProgress,
                            )
                        }
                }
                published.map { FileProvider.getUriForFile(context, AUTHORITY, it) }
            } catch (failure: Throwable) {
                // None of these files has been handed to another app yet
                published.forEach { OwnedCacheDirectories.delete(root, it.parentFile!!) }
                throw failure
            }
        }

    fun copyLogSnapshot(
        context: Context,
        source: File,
        rootName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): Uri {
        val published = copyLogExportFile(context, source, rootName, onProgress)
        return FileProvider.getUriForFile(context, AUTHORITY, published)
    }

    fun copyLogExportFile(
        context: Context,
        source: File,
        rootName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): File {
        require(rootName in roots) { "Unsupported FileProvider cache root" }
        DebugLog.flush()
        return copyLogSnapshotFile(
            File(context.cacheDir, rootName),
            source,
            LogAssetSnapshot.capture(context.filesDir).toByteArray(Charsets.UTF_8),
            onProgress,
        )
    }

    internal fun copyLogSnapshotFile(
        root: File,
        source: File,
        appendix: ByteArray = byteArrayOf(),
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): File =
        FileInputStream(source).use { input ->
            // The launcher can still append after the game process exits
            // Capture once on the open file for both copy and publication validation
            val expectedBytes = input.channel.size()
            publishFile(root, source.name, expectedBytes + appendix.size) { temporary ->
                FileOutputStream(temporary).use { output ->
                    LauncherFileCopy.copyStream(
                        input,
                        output,
                        expectedBytes,
                        source.name,
                        maxBytes = MAX_ROOT_BYTES,
                        stopAtExpectedSize = true,
                        onProgress = onProgress,
                    )
                    output.write(appendix)
                }
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
        val published =
            publishFile(
                File(context.cacheDir, rootName),
                displayName,
                expectedBytes,
                rootByteLimit(rootName),
                writer = writer,
            )
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
            requireCapacity(root, expectedBytes, retainedBytes, maxRootBytes)

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

    private fun rootByteLimit(rootName: String): Long =
        if (rootName == INPUT_DEMO_EXPORTS) MAX_DEMO_ROOT_BYTES else MAX_ROOT_BYTES

    private fun requireCapacity(
        root: File,
        requestedBytes: Long,
        retainedBytes: Long,
        maxRootBytes: Long,
    ) {
        val fallback = if (root.name == INPUT_DEMO_EXPORTS) " Use Save, then share the files from Downloads" else ""
        val requestedSize = formatBinarySize(requestedBytes)
        val limitSize = formatBinarySize(maxRootBytes)
        if (requestedBytes > maxRootBytes) {
            throw IOException(
                "Export size $requestedSize exceeds the $limitSize sharing limit.$fallback",
            )
        }
        if (retainedBytes > maxRootBytes - requestedBytes) {
            val availableBytes = (maxRootBytes - retainedBytes).coerceAtLeast(0L)
            throw IOException(
                "Sharing cache has ${formatBinarySize(availableBytes)} available of $limitSize; " +
                    "this export needs $requestedSize. Previous shares are kept for 24 hours.$fallback",
            )
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
