package com.dxxredux.app

import java.io.File
import java.io.FileOutputStream
import java.io.OutputStreamWriter
import java.util.UUID

internal object OwnedCacheDirectories {
    fun create(root: File): File {
        check(root.mkdirs() || root.isDirectory) { "Could not create cache root" }
        val directory = File(root, UUID.randomUUID().toString())
        check(isOwned(root, directory) && directory.mkdir()) { "Could not create cache directory" }
        return directory
    }

    fun delete(
        root: File,
        directory: File,
    ): Boolean {
        if (!isOwned(root, directory)) return false
        return !directory.exists() || directory.deleteRecursively()
    }

    fun prune(
        root: File,
        maxAgeMs: Long,
        nowMs: Long = System.currentTimeMillis(),
    ): Int {
        if (maxAgeMs < 0L || !root.isDirectory) return 0
        var deleted = 0
        root.listFiles().orEmpty().forEach { child ->
            if (isOwned(root, child) && nowMs - child.lastModified() >= maxAgeMs && delete(root, child)) {
                deleted++
            }
        }
        return deleted
    }

    fun writeUtf8Atomically(
        directory: File,
        filename: String,
        text: String,
    ): File {
        require(filename.isNotBlank() && filename == File(filename).name) { "Invalid cache filename" }
        check(directory.isDirectory) { "Cache directory is missing" }
        val target = File(directory, filename)
        val temporary = File(directory, "$filename.tmp")
        check(!target.exists() && !temporary.exists()) { "Cache file already exists" }
        try {
            FileOutputStream(temporary).use { stream ->
                OutputStreamWriter(stream, Charsets.UTF_8).use { writer ->
                    writer.write(text)
                    writer.flush()
                    stream.fd.sync()
                }
            }
            check(temporary.renameTo(target)) { "Could not publish cache file" }
            return target
        } finally {
            temporary.delete()
        }
    }

    internal fun isOwned(
        root: File,
        directory: File,
    ): Boolean =
        runCatching {
            UUID.fromString(directory.name)
            directory.canonicalFile.parentFile == root.canonicalFile
        }.getOrDefault(false)
}
