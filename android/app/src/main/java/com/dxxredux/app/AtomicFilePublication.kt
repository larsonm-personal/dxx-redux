package com.dxxredux.app

import java.io.File
import java.io.FileOutputStream
import java.io.OutputStreamWriter
import java.util.UUID

internal object AtomicFilePublication {
    private val lock = Any()

    fun <T> transaction(block: () -> T): T = synchronized(lock, block)

    fun writeUtf8(
        target: File,
        text: String,
        beforePublish: (File, File) -> Unit = { _, _ -> },
    ) = transaction {
        target.parentFile?.mkdirs()
        val temporary = uniqueSibling(target, "tmp")
        try {
            FileOutputStream(temporary).use { stream ->
                OutputStreamWriter(stream, Charsets.UTF_8).use { writer ->
                    writer.write(text)
                    writer.flush()
                    stream.fd.sync()
                }
            }
            beforePublish(temporary, target)
            replaceLocked(temporary, target)
        } finally {
            temporary.delete()
        }
    }

    fun writeUtf8Batch(
        updates: List<Pair<File, String>>,
        beforePublish: (Int, File, File) -> Unit = { _, _, _ -> },
    ) = transaction {
        val distinctUpdates = updates.distinctBy { it.first.absolutePath }
        val staged = mutableListOf<BatchEntry>()
        var published = 0
        try {
            for ((target, text) in distinctUpdates) {
                target.parentFile?.mkdirs()
                val temporary = uniqueSibling(target, "tmp")
                val backup =
                    if (target.exists()) {
                        uniqueSibling(target, "old")
                    } else {
                        null
                    }
                staged.add(BatchEntry(target, temporary, backup))
                writeSynced(temporary, text.toByteArray(Charsets.UTF_8))
                if (backup != null) writeSynced(backup, target.readBytes())
            }
            for ((index, entry) in staged.withIndex()) {
                beforePublish(index, entry.temporary, entry.target)
                replaceLocked(entry.temporary, entry.target)
                published++
            }
        } catch (failure: Throwable) {
            for (index in published - 1 downTo 0) {
                val entry = staged[index]
                if (entry.backup == null) {
                    entry.target.delete()
                } else {
                    replaceLocked(entry.backup, entry.target)
                }
            }
            throw failure
        } finally {
            for (entry in staged) {
                entry.temporary.delete()
                entry.backup?.delete()
            }
        }
    }

    fun publishDirectory(
        temporary: File,
        target: File,
        beforePublish: (File, File) -> Unit = { _, _ -> },
    ) = transaction {
        check(temporary.isDirectory) { "Temporary cache generation is missing" }
        beforePublish(temporary, target)
        replaceLocked(temporary, target)
    }

    fun publishFile(
        temporary: File,
        target: File,
        beforePublish: (File, File) -> Unit = { _, _ -> },
    ) = transaction {
        check(temporary.isFile) { "Temporary file generation is missing" }
        target.parentFile?.mkdirs()
        beforePublish(temporary, target)
        replaceLocked(temporary, target)
    }

    fun publishFiles(files: List<Pair<File, File>>) =
        transaction {
            val entries =
                files.distinctBy { it.second.absolutePath }.map { (temporary, target) ->
                    check(temporary.isFile) { "Temporary file generation is missing" }
                    target.parentFile?.mkdirs()
                    BatchEntry(target, temporary, target.takeIf(File::exists)?.let { uniqueSibling(target, "old") })
                }
            var published = 0
            var committed = false
            try {
                entries.forEach { entry ->
                    entry.backup?.let {
                        check(
                            entry.target.renameTo(it),
                        ) { "Could not retain the previous file generation" }
                    }
                }
                entries.forEach { entry ->
                    check(entry.temporary.renameTo(entry.target)) { "Could not publish file generation" }
                    published++
                }
                committed = true
            } catch (failure: Throwable) {
                for (index in published - 1 downTo 0) entries[index].target.delete()
                entries.forEach { entry ->
                    entry.backup?.takeIf(File::exists)?.let { backup ->
                        if (!backup.renameTo(entry.target)) {
                            failure.addSuppressed(
                                IllegalStateException("Could not restore the previous file generation"),
                            )
                        }
                    }
                }
                throw failure
            } finally {
                if (committed) {
                    entries.forEach { entry -> entry.backup?.delete() }
                }
            }
        }

    fun uniqueSibling(
        target: File,
        suffix: String,
    ): File = File(target.parentFile, ".${target.name}.${UUID.randomUUID()}.$suffix")

    private data class BatchEntry(
        val target: File,
        val temporary: File,
        val backup: File?,
    )

    private fun writeSynced(
        target: File,
        data: ByteArray,
    ) {
        FileOutputStream(target).use { stream ->
            stream.write(data)
            stream.flush()
            stream.fd.sync()
        }
    }

    private fun replaceLocked(
        temporary: File,
        target: File,
    ) {
        if (!target.exists()) {
            check(renameWithRetries(temporary, target)) { "Could not publish cache generation" }
            return
        }
        val backup = uniqueSibling(target, "old")
        check(renameWithRetries(target, backup)) { "Could not retain the previous cache generation" }
        try {
            check(renameWithRetries(temporary, target)) { "Could not publish cache generation" }
        } catch (failure: Throwable) {
            if (!target.exists()) renameWithRetries(backup, target)
            throw failure
        }
        if (backup.isDirectory) backup.deleteRecursively() else backup.delete()
    }

    private fun renameWithRetries(
        source: File,
        target: File,
    ): Boolean {
        repeat(RENAME_ATTEMPTS) { attempt ->
            if (source.renameTo(target)) return true
            if (attempt + 1 < RENAME_ATTEMPTS) Thread.sleep(RENAME_RETRY_DELAY_MS)
        }
        return false
    }

    private const val RENAME_ATTEMPTS = 3
    private const val RENAME_RETRY_DELAY_MS = 10L
}
