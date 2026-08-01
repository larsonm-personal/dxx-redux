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

    fun publishDirectory(
        temporary: File,
        target: File,
        beforePublish: (File, File) -> Unit = { _, _ -> },
    ) = transaction {
        check(temporary.isDirectory) { "Temporary cache generation is missing" }
        beforePublish(temporary, target)
        replaceLocked(temporary, target)
    }

    fun uniqueSibling(
        target: File,
        suffix: String,
    ): File = File(target.parentFile, ".${target.name}.${UUID.randomUUID()}.$suffix")

    private fun replaceLocked(
        temporary: File,
        target: File,
    ) {
        if (temporary.renameTo(target)) return
        val backup = uniqueSibling(target, "old")
        check(target.exists() && target.renameTo(backup)) { "Could not retain the previous cache generation" }
        try {
            check(temporary.renameTo(target)) { "Could not publish cache generation" }
        } catch (failure: Throwable) {
            if (!target.exists()) backup.renameTo(target)
            throw failure
        }
        if (backup.isDirectory) backup.deleteRecursively() else backup.delete()
    }
}
