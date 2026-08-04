package com.dxxredux.app

import java.io.File

internal object NativeFatalErrorStore {
    private const val FILE_NAME = ".native_fatal_error"
    private const val MAX_MESSAGE_CHARS = 4096

    fun publish(
        filesDir: File,
        message: String,
    ) {
        AtomicFilePublication.writeUtf8(
            File(filesDir, FILE_NAME),
            message.take(MAX_MESSAGE_CHARS),
        )
    }

    fun consume(filesDir: File): String? =
        AtomicFilePublication.transaction {
            val file = File(filesDir, FILE_NAME)
            if (!file.isFile) return@transaction null
            val message = file.readText(Charsets.UTF_8).take(MAX_MESSAGE_CHARS).trim()
            file.delete()
            message.takeIf(String::isNotEmpty)
        }
}
